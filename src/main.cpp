#include "engine.hpp"
#include "obj_load.hpp"
#include "wmap.hpp"
#include "physics.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

static JPH::Vec3 glm_to_jph(const glm::vec3& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

static glm::vec3 jph_to_glm(const JPH::Vec3& v)
{
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

static JPH::Quat glm_to_jph(const glm::quat& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

static glm::quat jph_to_glm(const JPH::Quat& q)
{
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

struct Enemy
{
    enum class State
    {
        Idle, Damaged, Dead,
    };

    // uint32_t sprite = 0;
    glm::vec3 position = glm::vec3(0);
    float angle = 0;
    glm::vec2 extents = glm::vec3(0.5);
    State state = State::Idle;
    State lastState = State::Idle;
    float stateTime = 0.0f;
    int health = 10;
    int maxHealth = 10;
    JPH::Ref<JPH::Character> character;
};

enum class SpriteDirection
{
    d0, d45, d90, d135, d180, d225, d270, d315
};

static std::vector<uint32_t> getIndexedTextures(eng::ResourceLoaderInterface& resourceLoader, const std::format_string<uint32_t>& basePath, uint32_t firstIndex, uint32_t count)
{
    std::vector<uint32_t> values;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        values.push_back(resourceLoader.loadTexture(std::format(basePath, firstIndex + i)));
    }
    return values;
}

struct GameLogic final : eng::GameLogicInterface
{
    struct {
        uint32_t blank;
        uint32_t lockOn;
        uint32_t gun;
        uint32_t muzzle_flash;
        uint32_t blood;
        uint32_t player;
        uint32_t rat;
    } textures;

    std::vector<std::pair<uint32_t, uint32_t>> skeletonResources;
    std::vector<std::pair<uint32_t, uint32_t>> mapResourcePairs;
    std::map<SpriteDirection, uint32_t> enemyTextures;
    wmap::Map map;

    std::map<SpriteDirection, std::vector<uint32_t>> plasmaTextures;

    glm::vec3 cameraPosition = { 0, 2, 0 };
    float playerAngle = 0;
    bool wasShooting = false;
    bool showMuzzleFlash = false;
    float muzzleFlashTimer = 0;
    bool inJump = false;
    float jumpHoldTimer = 0;

    struct {
        uint32_t left;
        uint32_t right;
        uint32_t forward;
        uint32_t back;
        uint32_t mouseLookX;
        uint32_t mouseLookY;

        uint32_t gpLeftStickXAxis;
        uint32_t gpLeftStickYAxis;
        uint32_t gpRightStickXAxis;
        uint32_t gpRightStickYAxis;
        uint32_t target;
        uint32_t shoot;
        std::vector<uint32_t> jump;
        std::vector<uint32_t> jumpHold;
    } inputMappings;

    const float movementSpeed = 7.0f;
    const float lookSensitivity = 0.01;
    const float gamepadLookSensitivity = 4.0f;
    const float jumpMinVelocity = 0.0f;
    const float jumpMaxVelocity = 5.0f;
    const float jumpHoldAcceleration = 85.0f;
    const float jumpMinHoldTime = 0.03;
    const float targetMinCos = glm::cos(glm::radians(30.0f));
    const float targetInDirectionMinCos = glm::cos(glm::radians(40.0f));
    const float minCameraVerticalAngle = glm::radians(-75.0f);
    const float maxCameraVerticalAngle = glm::radians(110.0f);
    const float minSinCameraVerticalAngle = glm::sin(minCameraVerticalAngle);
    const float maxSinCameraVerticalAngle = glm::sin(maxCameraVerticalAngle);
    const float lockOnInterpolationStrength = 5.0f;
    const float cosineThresholdCardinal = glm::cos(glm::radians(22.5));

    const float flickThreshold = 0.4;
    const float flickDetectionMinDistance = 0.3;
    const float flickDetectionMaxTime = 0.4;
    glm::vec2 flickDirection = glm::vec2(0);
    float maxDistanceOutOfFlickThreshold = 0;
    float timeExitedFlickThreshold = 0;

    uint32_t animationCounter = 0;
    const uint32_t animationFPS = 24;
    double animationTimer = 0;

    std::unique_ptr<fff::PhysicsWorldInterface> physicsWorld;

    std::vector<JPH::Ref<JPH::Shape>> shapeRefs;
    JPH::Ref<JPH::CharacterVirtual> playerCharacter;

    std::vector<Enemy> enemies;
    std::vector<eng::Light> lights;
    std::vector<eng::Decal> bloodDecals;
    std::vector<std::tuple<glm::vec3, glm::quat>> plasmas;

    ~GameLogic()
    {
        for (auto& enemy : enemies)
        {
            enemy.character->RemoveFromPhysicsSystem();
        }
        enemies.clear();
        playerCharacter = nullptr;
        shapeRefs.clear();
        physicsWorld.reset();
    }

    void updatePlayerPosition(const glm::vec2& moveInput, const float deltaTime)
    {
        glm::vec3 direction(0);
        if (glm::dot(moveInput, moveInput) > glm::epsilon<float>())
        {
            const glm::vec3 forward = glm::vec3(0, 0, -1);
            const glm::vec3 right = glm::vec3(1, 0, 0);
            direction = moveInput.y * forward + moveInput.x * right;
            if (auto len = glm::length(direction); len > 1.0f)
            {
                direction /= len;
            }
        }

        auto velocity = jph_to_glm(playerCharacter->GetLinearVelocity());
        playerCharacter->SetLinearVelocity(glm_to_jph(movementSpeed * direction + glm::vec3(0, velocity.y, 0)));
    }

    void generateRooms(const uint64_t seed = 0, const uint32_t width = 50, const uint32_t height = 35, const uint32_t partitionedRoomCount = 20, const uint32_t targetRoomCount = 8, const uint32_t minSplitDimension = 4, const uint32_t minDoorOverlap = 2)
    {
        struct RoomNode {
            uint32_t x, y;
            uint32_t width, height;
            int axis = 0;
            uint32_t children[2];
        };
        std::vector<RoomNode> roomNodes;
        auto compareRoomIndices = [&](auto left, auto right) {
            return std::min(roomNodes[left].width, roomNodes[left].height) < std::min(roomNodes[right].width, roomNodes[right].height);
        };
        std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(compareRoomIndices)> roomIndicesQueue(compareRoomIndices);
        roomNodes.push_back(RoomNode { .x = 0, .y = 0, .width = width, .height = height });
        roomIndicesQueue.push(roomNodes.size() - 1);

        auto printRoomChars = [&](std::vector<bool> marked = {}, std::vector<std::pair<uint32_t, uint32_t>> doors = {}) {
            std::vector<char> roomChars((height + 1) * (width + 2), ' ');

            for (uint32_t i = 0; i <= height; ++i)
            {
                roomChars[i * (width + 2) + width + 1] = '\n';
            }

            if (marked.size() == roomNodes.size()) for(uint32_t index = 0; index < roomNodes.size(); ++index) if (marked[index])
            {
                for (uint32_t i = 0; i <= roomNodes[index].width; ++i)
                {
                    for (uint32_t j = 0; j <= roomNodes[index].height; ++j)
                    {
                        roomChars[(roomNodes[index].y + j) * (width + 2) + roomNodes[index].x + i] = '/';
                    }
                }
            }

            for (const auto& roomNode : roomNodes)
            {
                if (roomNode.axis == 0)
                {
                    for (uint32_t i = 0; i <= roomNode.width; ++i)
                    {
                        roomChars[roomNode.y * (width + 2) + roomNode.x + i] = 'X';
                        roomChars[(roomNode.y + roomNode.height) * (width + 2) + roomNode.x + i] = 'X';
                    }
                    for (uint32_t i = 0; i <= roomNode.height; ++i)
                    {
                        roomChars[(roomNode.y + i) * (width + 2) + roomNode.x] = 'X';
                        roomChars[(roomNode.y + i) * (width + 2) + roomNode.x + roomNode.width] = 'X';
                    }
                }
            }

            for (const auto [x, y] : doors)
            {
                roomChars[y * (width + 2) + x] = '_';
            }

            for (uint32_t i = 0; i < roomNodes.size(); ++i) if (roomNodes[i].axis == 0)
            {
                roomChars[(roomNodes[i].y + roomNodes[i].height/2) * (width + 2) + roomNodes[i].x + roomNodes[i].width/2] = '0' + ((i/10)%10);
                roomChars[(roomNodes[i].y + roomNodes[i].height/2) * (width + 2) + roomNodes[i].x + roomNodes[i].width/2 + 1] = '0' + (i%10);
            }

            std::cout << std::string(roomChars.data(), roomChars.size()) << std::endl;
        };

        std::mt19937_64 prng;
        prng.seed(seed);

        uint32_t leafNodes = 1;
        while (!roomIndicesQueue.empty() && leafNodes < partitionedRoomCount)
        {
            uint32_t index = roomIndicesQueue.top();
            roomIndicesQueue.pop();

            std::vector<bool> marked(roomNodes.size());
            marked[index] = true;
            // printRoomChars(marked);

            if (std::max(roomNodes[index].width, roomNodes[index].height) < minSplitDimension)
            {
                continue;
            }

            roomNodes[index].children[0] = roomNodes.size();
            roomNodes[index].children[1] = roomNodes.size() + 1;
            roomNodes.push_back(roomNodes[index]);
            roomNodes.push_back(roomNodes[index]);
    
            if (roomNodes[index].width > roomNodes[index].height)
            {
                roomNodes[index].axis = 1;
                uint32_t split = (prng() % (roomNodes[index].width - minSplitDimension)) + minSplitDimension / 2;
                roomNodes[roomNodes[index].children[0]].width = split;
                roomNodes[roomNodes[index].children[1]].x += split;
                roomNodes[roomNodes[index].children[1]].width = roomNodes[index].width - split;
            }
            else
            {
                roomNodes[index].axis = 2;
                uint32_t split = (prng() % (roomNodes[index].height - minSplitDimension)) + minSplitDimension / 2;
                roomNodes[roomNodes[index].children[0]].height = split;
                roomNodes[roomNodes[index].children[1]].y += split;
                roomNodes[roomNodes[index].children[1]].height = roomNodes[index].height - split;
            }

            roomIndicesQueue.push(roomNodes[index].children[0]);
            roomIndicesQueue.push(roomNodes[index].children[1]);

            ++leafNodes;

        }


        roomNodes.erase(std::remove_if(roomNodes.begin(), roomNodes.end(), [](const auto& roomNode) { return roomNode.axis > 0; }), roomNodes.end());
        // printRoomChars();

        struct RoomEdge
        {
            uint32_t nodes[2];
            int axis = 0;
            int overlap = 0;
        };
        std::vector<RoomEdge> roomEdges;

        std::vector<uint32_t> minIndices(roomNodes.size());
        std::iota(minIndices.begin(), minIndices.end(), 0);
        std::vector<uint32_t> maxIndices = minIndices;
        std::sort(minIndices.begin(), minIndices.end(), [&](const auto left, const auto right) { return roomNodes[left].x < roomNodes[right].x; });
        std::sort(maxIndices.begin(), maxIndices.end(), [&](const auto left, const auto right) { return roomNodes[left].x + roomNodes[left].width < roomNodes[right].x + roomNodes[right].width; });
        for (uint32_t mini = 0, maxi = 0; mini < minIndices.size() && maxi < maxIndices.size(); ++mini)
        {
            uint32_t val = roomNodes[minIndices[mini]].x;
            for (; maxi < maxIndices.size() && roomNodes[maxIndices[maxi]].x + roomNodes[maxIndices[maxi]].width < val; ++maxi);
            for (uint32_t tmaxi = maxi; tmaxi < maxIndices.size() && roomNodes[maxIndices[tmaxi]].x + roomNodes[maxIndices[tmaxi]].width == val; ++tmaxi)
            {
                int overlap = static_cast<int>(std::min(roomNodes[minIndices[mini]].y + roomNodes[minIndices[mini]].height,
                                                        roomNodes[maxIndices[tmaxi]].y + roomNodes[maxIndices[tmaxi]].height))
                    - static_cast<int>(std::max(roomNodes[minIndices[mini]].y, roomNodes[maxIndices[tmaxi]].y));
                if (overlap >= static_cast<int>(minDoorOverlap))
                {
                    roomEdges.push_back(RoomEdge{ .nodes = { minIndices[mini], maxIndices[tmaxi] }, .axis = 1, .overlap = overlap });
                }
            }
        }

        std::sort(minIndices.begin(), minIndices.end(), [&](const auto left, const auto right) { return roomNodes[left].y < roomNodes[right].y; });
        std::sort(maxIndices.begin(), maxIndices.end(), [&](const auto left, const auto right) { return roomNodes[left].y + roomNodes[left].height < roomNodes[right].y + roomNodes[right].height; });
        for (uint32_t mini = 0, maxi = 0; mini < minIndices.size() && maxi < maxIndices.size(); ++mini)
        {
            uint32_t val = roomNodes[minIndices[mini]].y;
            for (; maxi < maxIndices.size() && roomNodes[maxIndices[maxi]].y + roomNodes[maxIndices[maxi]].height < val; ++maxi);
            for (uint32_t tmaxi = maxi; tmaxi < maxIndices.size() && roomNodes[maxIndices[tmaxi]].y + roomNodes[maxIndices[tmaxi]].height == val; ++tmaxi)
            {
                int overlap = static_cast<int>(std::min(roomNodes[minIndices[mini]].x + roomNodes[minIndices[mini]].width,
                                                        roomNodes[maxIndices[tmaxi]].x + roomNodes[maxIndices[tmaxi]].width))
                    - static_cast<int>(std::max(roomNodes[minIndices[mini]].x, roomNodes[maxIndices[tmaxi]].x));
                if (overlap >= static_cast<int>(minDoorOverlap))
                {
                    roomEdges.push_back(RoomEdge{ .nodes = { minIndices[mini], maxIndices[tmaxi] }, .axis = 2, .overlap = overlap });
                }
            }
        }

        /* for (const auto& roomEdge : roomEdges)
        {
            std::cout << " nodes: " << roomEdge.nodes[0] << " " << roomEdge.nodes[1] << " axis: " << roomEdge.axis << " overlap: " << roomEdge.overlap << std::endl;
        } */

        std::vector<bool> selectedRoomNodes(roomNodes.size(), false);
        selectedRoomNodes[prng() % roomNodes.size()] = true;
        // printRoomChars(selectedRoomNodes);
        std::vector<std::pair<uint32_t, uint32_t>> doors;
        uint32_t countSelected = 1;
        std::vector<uint32_t> edgeIndices(roomEdges.size());
        std::iota(edgeIndices.begin(), edgeIndices.end(), 0);
        while (countSelected < targetRoomCount)
        {
            std::sort(edgeIndices.begin(), edgeIndices.end(), [&](const auto left, const auto right) {
                        bool boundary0 = selectedRoomNodes[roomEdges[left].nodes[0]] != selectedRoomNodes[roomEdges[left].nodes[1]];
                        bool boundary1 = selectedRoomNodes[roomEdges[right].nodes[0]] != selectedRoomNodes[roomEdges[right].nodes[1]];
                        return (boundary0 && !boundary1) || (boundary0 && boundary1 && roomEdges[left].overlap < roomEdges[right].overlap);
                    });
            /* for (const auto index : edgeIndices) if (const auto& roomEdge = roomEdges[index]; true)
            {
                std::cout << " nodes: " << roomEdge.nodes[0] << " " << roomEdge.nodes[1] << " axis: " << roomEdge.axis << " overlap: " << roomEdge.overlap << std::endl;
            } */
            const auto& roomEdge = roomEdges[edgeIndices.front()];
            if (selectedRoomNodes[roomEdge.nodes[0]] == selectedRoomNodes[roomEdge.nodes[1]])
            {
                break;
            }

            const auto& node0 = roomNodes[roomEdge.nodes[0]];
            const auto& node1 = roomNodes[roomEdge.nodes[1]];
            if (roomEdge.axis == 1)
            {
                doors.emplace_back(node0.x, (std::max(node0.y, node1.y) + std::min(node0.y + node0.height, node1.y + node1.height)) / 2);
            }
            else
            {
                doors.emplace_back((std::max(node0.x, node1.x) + std::min(node0.x + node0.width, node1.x + node1.width)) / 2, node0.y);
            }

            selectedRoomNodes[roomEdge.nodes[0]] = true;
            selectedRoomNodes[roomEdge.nodes[1]] = true;
            ++countSelected;
            // printRoomChars(selectedRoomNodes, doors);
        }

        for (uint32_t i = 0, j = 0; i < roomNodes.size(); ++i)
        {
            if (selectedRoomNodes[i])
            {
                roomNodes[j++] = roomNodes[i];
            }
        }
        roomNodes.resize(countSelected);
        // std::cout << "FINAL:" << std::endl;
        printRoomChars({}, doors);
    }

    void init(eng::ResourceLoaderInterface& resourceLoader, eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app) override
    {
        physicsWorld.reset(fff::createPhysicsWorld());

        textures = {
            .blank = resourceLoader.loadTexture("resources/textures/blank.png"),
            .lockOn = resourceLoader.loadTexture("resources/textures/corners.png"),
            .gun = resourceLoader.loadTexture("resources/textures/gun.png"),
            .muzzle_flash = resourceLoader.loadTexture("resources/textures/muzzle_flash.png"),
            .blood = resourceLoader.loadTexture("resources/textures/blood.png"),
            .player = resourceLoader.loadTexture("resources/textures/player.png"),
            .rat = resourceLoader.loadTexture("resources/textures/rat.png"),
        };

        inputMappings = {
            .left = input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_A, nullptr)),
            .right = input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_D, nullptr)),
            .forward = input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_W, nullptr)),
            .back = input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_S, nullptr)),
            .mouseLookX = input.mapCursor(input.createMapping(), eng::InputInterface::CursorAxis::X),
            .mouseLookY = input.mapCursor(input.createMapping(), eng::InputInterface::CursorAxis::Y),
            .gpLeftStickXAxis = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_LEFTX),
            .gpLeftStickYAxis = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_LEFTY),
            .gpRightStickXAxis = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_RIGHTX),
            .gpRightStickYAxis = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_RIGHTY),
            .target = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER, eng::InputInterface::RealStateEvent::Threshold, 0.5f),
            .shoot = input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, eng::InputInterface::RealStateEvent::Threshold, 0.5f),
            .jump = {
                input.mapGamepadButton(input.createMapping(), SDL_GAMEPAD_BUTTON_SOUTH, eng::InputInterface::BoolStateEvent::Pressed),
                input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_SPACE, nullptr), eng::InputInterface::BoolStateEvent::Pressed),
            },
            .jumpHold = {
                input.mapGamepadButton(input.createMapping(), SDL_GAMEPAD_BUTTON_SOUTH, eng::InputInterface::BoolStateEvent::Down),
                input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_SPACE, nullptr), eng::InputInterface::BoolStateEvent::Down),
            },
        };

        skeletonResources = loadGeometryObj(resourceLoader, "resources/models/skeleton.obj", textures.blank, "resources");

        map = wmap::load("resources/maps/test.map");
        mapResourcePairs = wmap::createGeometry(map, resourceLoader, "resources/textures");

        std::mt19937_64 prng;
        for (int i = 0; i < 50; ++i) generateRooms(prng());

        std::vector<JPH::Vec3> shapePoints;
        for (const auto& mshape : map.shapes)
        {
            shapePoints.clear();
            for (const auto& face : mshape.faces)
            {
                for (const auto& vert : face.vertices)
                {
                    shapePoints.push_back(JPH::Vec3(vert.x, vert.y, vert.z));
                }
            }

            JPH::ConvexHullShapeSettings settings(shapePoints.data(), shapePoints.size());
            auto shapeResult = settings.Create();
            if (shapeResult.IsValid())
            {
                const auto& shape = shapeResult.Get();
                physicsWorld->getPhysicsSystem().GetBodyInterface().CreateAndAddBody(
                        JPH::BodyCreationSettings(shape, JPH::Vec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
                        JPH::EActivation::DontActivate);
            }
        }

        enemyTextures = {
            { SpriteDirection::d0, resourceLoader.loadTexture("resources/textures/body/0001.png") },
            { SpriteDirection::d45, resourceLoader.loadTexture("resources/textures/body/0002.png") },
            { SpriteDirection::d90, resourceLoader.loadTexture("resources/textures/body/0003.png") },
            { SpriteDirection::d135, resourceLoader.loadTexture("resources/textures/body/0004.png") },
            { SpriteDirection::d180, resourceLoader.loadTexture("resources/textures/body/0005.png") },
            { SpriteDirection::d225, resourceLoader.loadTexture("resources/textures/body/0006.png") },
            { SpriteDirection::d270, resourceLoader.loadTexture("resources/textures/body/0007.png") },
            { SpriteDirection::d315, resourceLoader.loadTexture("resources/textures/body/0008.png") },
        };

        plasmaTextures = {
            { SpriteDirection::d0, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 0, 24) },
            { SpriteDirection::d45, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 24, 24) },
            { SpriteDirection::d90, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 48, 24) },
            { SpriteDirection::d135, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 72, 24) },
            { SpriteDirection::d180, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 96, 24) },
            { SpriteDirection::d225, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 120, 24) },
            { SpriteDirection::d270, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 144, 24) },
            { SpriteDirection::d315, getIndexedTextures(resourceLoader, "resources/textures/plasma/{:04}.png", 168, 24) },
        };

        glm::vec3 playerStartPosition(0, 1, 0);
        glm::quat playerStartOrientation = glm::identity<glm::quat>();

        const auto& enemyShape = shapeRefs.emplace_back(new JPH::SphereShape(0.25f));

        for (const auto& entity : map.entities)
        {
            glm::vec3 position(0);
            if (auto paramIter = entity.params.find("origin"); paramIter != entity.params.end())
            {
                std::stringstream ss(paramIter->second);
                ss >> position.x >> position.y >> position.z;
                position = glm::vec3(-position.y, position.z, -position.x) / 64.0f;
            }

            glm::quat orientation = glm::identity<glm::quat>();
            if (auto paramIter = entity.params.find("angle"); paramIter != entity.params.end())
            {
                float angle = std::stof(paramIter->second);
                orientation = glm::angleAxis(glm::radians(angle), glm::vec3(0, 1, 0));
            }

            if (auto paramIter = entity.params.find("classname"); paramIter != entity.params.end())
            {
                if (paramIter->second == "Enemy")
                {
                    JPH::CharacterSettings characterSettings;
                    characterSettings.mShape = enemyShape;
                    characterSettings.mLayer = 1;

                    enemies.push_back(Enemy {
                                .position = position,
                                .extents = glm::vec2(0.25, 0.25),
                                .character = new JPH::Character(&characterSettings, glm_to_jph(position), JPH::Quat::sIdentity(), 0, &physicsWorld->getPhysicsSystem()),
                            });
                    enemies.back().character->AddToPhysicsSystem();
                }
                else if (paramIter->second == "info_player_start")
                {
                    playerStartPosition = position;
                    playerStartOrientation = orientation;
                }
                else if (paramIter->second == "light")
                {
                    glm::vec3 intensity(1);
                    if (auto paramIter = entity.params.find("intensity"); paramIter != entity.params.end())
                    {
                        std::stringstream ss(paramIter->second);
                        ss >> intensity.x >> intensity.y >> intensity.z;
                    }
                    lights.push_back(eng::Light { .position = position, .intensity = intensity });
                    plasmas.push_back({ position, glm::identity<glm::quat>() });
                }
            }
        }

        const auto& capsuleShape = shapeRefs.emplace_back(new JPH::CapsuleShape(0.6f, 0.2f));

        JPH::CharacterVirtualSettings characterSettings;
        characterSettings.mShape = capsuleShape;
        characterSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0);
        characterSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.2f);
        playerCharacter = new JPH::CharacterVirtual(&characterSettings,
                glm_to_jph(playerStartPosition), glm_to_jph(playerStartOrientation), &physicsWorld->getPhysicsSystem());

        app.setWantsCursorLock(true);
    }

    SpriteDirection getSpriteDirection(const glm::vec3& forward, const glm::vec3& right, const glm::vec3& entityFacingDirection)
    {
        const float dForward = glm::dot(forward, entityFacingDirection);
        const float dRight = glm::dot(right, entityFacingDirection);
        if (glm::abs(dForward) > cosineThresholdCardinal)
        {
            return (dForward > 0) ? SpriteDirection::d180 : SpriteDirection::d0;
        }
        else if (glm::abs(dRight) > cosineThresholdCardinal)
        {
            return (dRight > 0) ? SpriteDirection::d90 : SpriteDirection::d270;
        }
        else if (dForward > 0)
        {
            return (dRight > 0) ? SpriteDirection::d135 : SpriteDirection::d225;
        }
        else
        {
            return (dRight > 0) ? SpriteDirection::d45 : SpriteDirection::d315;
        }
    }

    SpriteDirection getSpriteDirectionToCamera(const glm::vec3& position, const glm::quat& orientation)
    {
        SpriteDirection direction = SpriteDirection::d0;
        const glm::vec3 deltaPos = position - cameraPosition;
        const glm::vec3 deltaHPos = glm::vec3(deltaPos.x, 0, deltaPos.z);
        if (glm::dot(deltaHPos, deltaHPos) > glm::epsilon<float>())
        {
            const glm::vec3 forward = glm::normalize(deltaHPos);
            const glm::vec3 right = glm::vec3(-forward.z, 0, forward.x);
            const glm::vec3 facingDirection = orientation * glm::vec3(0, 0, -1);
            direction = getSpriteDirection(forward, right, facingDirection);
        }

        return direction;
    }

    void render(eng::SceneInterface& scene)
    {
        const auto [framebufferWidth, framebufferHeight] = scene.framebufferSize();
        const float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

        scene.layers().resize(2);

        auto& sceneLayer = scene.layers()[0];
        sceneLayer.spriteInstances.clear();
        sceneLayer.geometryInstances.clear();
        sceneLayer.overlaySpriteInstances.clear();
        sceneLayer.view = glm::translate(glm::lookAt(glm::vec3(0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)), -cameraPosition);
        sceneLayer.viewport = { .offset = { 0, framebufferHeight }, .extent = { framebufferWidth, -static_cast<float>(framebufferHeight) } };
        sceneLayer.scissor = { .extent = { framebufferWidth, framebufferHeight } };
        sceneLayer.projection = glm::perspectiveRH_ZO(0.25f * glm::pi<float>(), aspectRatio, 0.1f, 100.f);
        sceneLayer.lights.assign(lights.begin(), lights.end());
        sceneLayer.ambientLight = glm::vec3(0);

        sceneLayer.decals.clear();
        sceneLayer.decals.insert(sceneLayer.decals.end(), bloodDecals.begin(), bloodDecals.end());

        for (const auto& [materialResource, geometryResource] : mapResourcePairs)
        {
            sceneLayer.geometryInstances.push_back(eng::GeometryInstance {
                        .textureIndex = materialResource,
                        .geometryIndex = geometryResource,
                    });
        }

        for (uint32_t i = 0; i < enemies.size(); ++i)
        {
            const auto& enemy = enemies[i];

            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = enemy.position,
                        .scale = glm::vec3(enemy.extents, 1),
                        .angle = enemy.angle,
                        .textureIndex = textures.rat,
                        .tintColor = enemy.state == Enemy::State::Damaged ? glm::vec4(1.0, 0.5, 0.5, 1.0) : glm::vec4(1.0),
                    });

            /* if (static_cast<int>(i) == lockedOnEnemy)
            {
                sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                            .position = enemy.position,
                            .scale = glm::vec3(enemy.extents, 1),
                            .textureIndex = textures.lockOn,
                        });

                sceneLayer.overlaySpriteInstances.push_back(eng::SpriteInstance {
                            .position = enemy.position + glm::vec3(0, 1, 0),
                            .scale = glm::vec3(0.1, 0.02, 1.0),
                            .tintColor = glm::vec4(1, 0, 0, 1),
                        });
            } */
        }

        for (const auto& [position, rotation] : plasmas)
        {
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = position,
                        .textureIndex = plasmaTextures[getSpriteDirection(glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), rotation * glm::vec3(0, 0, -1))][animationCounter],
                    });
        }

        sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                    .position = jph_to_glm(playerCharacter->GetPosition()),
                    .scale = glm::vec3(0.25),
                    .angle = playerAngle,
                    .textureIndex = textures.player,
                });

        sceneLayer.lights.push_back(eng::Light {
                    .position = jph_to_glm(playerCharacter->GetPosition()) + glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * glm::vec3(0.25, 2, 0),
                    .intensity = glm::vec3(5),
                });

        /* auto& overlayLayer = scene.layers()[1];
        overlayLayer.spriteInstances.clear();
        overlayLayer.geometryInstances.clear();
        overlayLayer.overlaySpriteInstances.clear();
        overlayLayer.viewport = { .offset = { 0, framebufferHeight }, .extent = { framebufferWidth, -static_cast<float>(framebufferHeight) } };
        overlayLayer.scissor = { .extent = { framebufferWidth, framebufferHeight } };
        overlayLayer.projection = glm::orthoRH_ZO(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
        overlayLayer.ambientLight = glm::vec3(1);

        if (showMuzzleFlash)
        {
            overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = { 0.5f, -0.5f, 0.0f },
                        .scale = { 0.5f, 0.5f, 1.0f },
                        .textureIndex = textures.muzzle_flash,
                    });
            sceneLayer.lights.push_back(eng::Light {
                        .position = cameraPosition + cameraOrientation * glm::vec3(0, 0, -1),
                        .intensity = glm::vec3(1, 1, 0),
                    });
        } */

        /* overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                    .position = { 0.5f, -0.5f, 0.0f },
                    .scale = { 0.5f, 0.5f, 1.0f },
                    .textureIndex = textures.gun,
                }); */

        /* for (const auto& [materialResource, geometryResource] : skeletonResources)
        {
            overlayLayer.geometryInstances.push_back(eng::GeometryInstance {
                        .position = { -1.0f, -0.35f, 0.0f },
                        .scale = glm::vec3(0.1f),
                        .rotation = glm::angleAxis(skeletonAngle, glm::vec3(0, 1, 0)),
                        .textureIndex = materialResource,
                        .geometryIndex = geometryResource,
                    });
        } */
    }

    void enemyUpdateDamaged(Enemy& enemy, const float deltaTime)
    {
        if (enemy.stateTime > 1.0f)
        {
            enemy.state = Enemy::State::Idle;
        }
    }

    void runFrame(eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app, const double deltaTime) override
    {
        animationTimer += deltaTime;
        if (animationTimer >= 1.0)
        {
            animationTimer -= std::floor(animationTimer);
        }
        animationCounter = static_cast<uint32_t>(animationFPS * animationTimer);

        const auto [windowWidth, windowHeight] = app.getWindowSize();
        const glm::vec2 mouseLookInput = {
            input.getReal(inputMappings.mouseLookX) - 0.5f * windowWidth,
            input.getReal(inputMappings.mouseLookY) - 0.5f * windowHeight,
        };

        const glm::vec2 gamepadLookInput = {
            input.getReal(inputMappings.gpRightStickXAxis),
            input.getReal(inputMappings.gpRightStickYAxis),
        };

        if (glm::any(glm::greaterThan(glm::abs(mouseLookInput), glm::vec2(0))))
        {
            playerAngle = atan2(-mouseLookInput.x, -mouseLookInput.y);
        }
        else if (glm::any(glm::greaterThan(glm::abs(gamepadLookInput), glm::vec2(0.2))))
        {
            playerAngle = atan2(gamepadLookInput.y, gamepadLookInput.x);
        }

        const glm::vec2 keyboardMoveInput = {
            static_cast<float>(input.getBoolean(inputMappings.right)) - static_cast<float>(input.getBoolean(inputMappings.left)),
            static_cast<float>(input.getBoolean(inputMappings.forward)) - static_cast<float>(input.getBoolean(inputMappings.back))
        };

        const glm::vec2 gamepadMoveInput = {
            input.getReal(inputMappings.gpLeftStickXAxis), -input.getReal(inputMappings.gpLeftStickYAxis),
        };

        if (glm::any(glm::greaterThan(glm::abs(keyboardMoveInput), glm::vec2(0))))
        {
            updatePlayerPosition(keyboardMoveInput, deltaTime);
        }
        else if (auto len = glm::length(gamepadMoveInput); len > 0.2f)
        {
            updatePlayerPosition(gamepadMoveInput, deltaTime);
        }
        else
        {
            updatePlayerPosition(glm::vec2(0), deltaTime);
        }

        /* if (input.getBoolean(inputMappings.target))
        {
            if (lockedOnEnemy == -1)
            {
                updateLockedOnEnemy();
            }
        }
        else
        {
            lockedOnEnemy = -1;
        } */

        /* if (input.getBoolean(inputMappings.shoot))
        {
            if (!wasShooting)
            {
                if (lockedOnEnemy != -1)
                {
                    enemies[lockedOnEnemy].state = Enemy::State::Damaged;
                    showMuzzleFlash = true;
                    muzzleFlashTimer = 0;

                    const JPH::RRayCast raycast(glm_to_jph(enemies[lockedOnEnemy].position),
                        glm_to_jph(glm::angleAxis(glm::linearRand(0.0f, 2.0f * glm::pi<float>()), glm::vec3(0, 1, 0)) *
                            glm::angleAxis(glm::linearRand(0.0f, glm::radians(15.0f)), glm::vec3(1, 0, 0)) *
                            glm::vec3(0, -5, 0)));
                    JPH::RayCastResult raycastResult;
                    if (physicsWorld->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(raycast, raycastResult, JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0))))
                    {
                        JPH::BodyLockRead lock(physicsWorld->getPhysicsSystem().GetBodyLockInterface(), raycastResult.mBodyID);
                        if (lock.Succeeded())
                        {
                            const auto position = raycast.GetPointOnRay(raycastResult.mFraction);
                            const auto normal = lock.GetBody().GetWorldSpaceSurfaceNormal(raycastResult.mSubShapeID2, position);
                            bloodDecals.push_back(eng::Decal {
                                    .position = jph_to_glm(position),
                                    .scale = glm::vec3(1, 1, 0.05),
                                    .rotation = glm::rotation(glm::vec3(0, 0, -1), jph_to_glm(normal)) * glm::angleAxis(glm::linearRand(0.0f, glm::pi<float>()), glm::vec3(0, 0, 1)),
                                    .textureIndex = textures.blood,
                                });
                        }
                    }
                }
            }
            wasShooting = true;
        }
        else
        {
            wasShooting = false;
        } */

        if (showMuzzleFlash)
        {
            if (muzzleFlashTimer > 0.2f)
            {
                showMuzzleFlash = false;
            }
            muzzleFlashTimer += deltaTime;
        }

        for (auto& enemy : enemies)
        {
            enemy.position = jph_to_glm(enemy.character->GetPosition());

            if (enemy.state != enemy.lastState)
            {
                if (enemy.state == Enemy::State::Damaged)
                {
                    const glm::vec3 deltaPos = enemy.position - cameraPosition;
                    const glm::vec3 deltaHPos = glm::vec3(deltaPos.x, 0, deltaPos.z);
                    if (glm::dot(deltaHPos, deltaHPos) > glm::epsilon<float>())
                    {
                        const glm::vec3 forward = glm::normalize(deltaHPos);
                        enemy.character->AddImpulse(glm_to_jph(forward) * 100.0f);
                    }

                    --enemy.health;
                }
                else if (enemy.state == Enemy::State::Dead)
                {
                    enemy.character->RemoveFromPhysicsSystem();
                }

                enemy.lastState = enemy.state;
                enemy.stateTime = 0;
            }

            if (enemy.state == Enemy::State::Damaged && enemy.stateTime >= 1.0f)
            {
                enemy.state = Enemy::State::Idle;
            }
            if (enemy.health <= 0)
            {
                enemy.state = Enemy::State::Dead;
                /* if (lockedOnEnemy != -1 && &enemies[lockedOnEnemy] == &enemy)
                {
                    lockedOnEnemy = -1;
                } */
            }

            enemy.stateTime += deltaTime;
        }

        std::erase_if(enemies, [](const auto& enemy){ return enemy.lastState == Enemy::State::Dead; });

        physicsWorld->update(deltaTime);

        for (auto& enemy : enemies)
        {
            enemy.character->PostSimulation(0.05);
        }

        playerCharacter->UpdateGroundVelocity();
        auto velocity = jph_to_glm(playerCharacter->GetLinearVelocity());
        auto groundVelocity = jph_to_glm(playerCharacter->GetGroundVelocity());

        for (const auto& contact : playerCharacter->GetActiveContacts())
        {
            if (contact.mHadCollision && contact.mContactNormal.GetY() < -0.1)
            {
                const auto normal = jph_to_glm(contact.mContactNormal);
                velocity -= glm::dot(normal, velocity) * normal;
                inJump = false;
            }
        }
        
        bool jumpHold = false;
        for (const auto& mapping : inputMappings.jumpHold)
        {
            if (input.getBoolean(mapping))
            {
                jumpHold = true;
                break;
            }
        }

        bool jumpPress = false;
        for (const auto& mapping : inputMappings.jump)
        {
            if (input.getBoolean(mapping))
            {
                jumpPress = true;
                break;
            }
        }

        if (inJump && (jumpHoldTimer <= jumpMinHoldTime || jumpHold) && velocity.y < jumpMaxVelocity)
        {
            jumpHoldTimer += deltaTime;
            velocity.y += jumpHoldAcceleration * deltaTime;
        }
        else
        {
            inJump = false;
            jumpHoldTimer = 0;
        }

        if (velocity.y - groundVelocity.y < 0.1f &&
            playerCharacter->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
        {
            velocity = glm::vec3(velocity.x, 0, velocity.z) + groundVelocity;
            if (jumpPress)
            {
                velocity.y += jumpMinVelocity;
                inJump = true;
            }
        }

        velocity += jph_to_glm(physicsWorld->getPhysicsSystem().GetGravity() * deltaTime);
        playerCharacter->SetLinearVelocity(glm_to_jph(velocity));
        physicsWorld->updateCharacter(*playerCharacter, deltaTime);

        cameraPosition = jph_to_glm(playerCharacter->GetPosition()) + glm::vec3(0, 5, 0);

        render(scene);
    }

    void cleanup() override
    {
        for (auto& enemy : enemies)
        {
            enemy.character->RemoveFromPhysicsSystem();
        }
        enemies.clear();
        playerCharacter = nullptr;
        shapeRefs.clear();
        physicsWorld.reset();
    }
};

eng::ApplicationInfo EngineApp_GetApplicationInfo()
{
    return eng::ApplicationInfo {
        .appName = "game",
        .appVersion = 0,
        .windowTitle = "Ludum Dare 57",
        .windowWidth = 1920,
        .windowHeight = 1080,
    };
}

eng::GameLogicInterface* EngineApp_CreateGameLogic()
{
    return new GameLogic;
}
