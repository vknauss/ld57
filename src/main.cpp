#include "engine.hpp"
#include "obj_load.hpp"
#include "physics.hpp"
#include "dungeon.hpp"
#include "jph_glm_convert.hpp"

#include <iostream>
#include <map>
#include <memory>
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
#include <SDL3/SDL_mouse.h>

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

namespace PlayerStates
{
    enum PlayerStates
    {
        Idle,
        Walk,
        Slide,
        MAX_VALUE,
    };
};

struct Bullet
{
    JPH::BodyID bodyID;
    float angle;
};

struct GameLogic final : eng::GameLogicInterface
{
    struct {
        uint32_t blank;
        uint32_t lockOn;
        uint32_t gun;
        uint32_t muzzle_flash;
        uint32_t blood;
        uint32_t rat;
        uint32_t floor;
        uint32_t bullet;
        uint32_t splat;
        uint32_t font;
        std::vector<uint32_t> player[PlayerStates::MAX_VALUE];
        std::vector<uint32_t> spiderWalk;
    } textures;

    std::map<SpriteDirection, uint32_t> enemyTextures;

    uint32_t dungeonGeometryResource;

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
        std::vector<uint32_t> shoot;
    } inputMappings;

    const float movementSpeed = 7.0f;
    const float cosineThresholdCardinal = glm::cos(glm::radians(22.5));
    const uint32_t animationFPS = 8;
    const float shootTimeout = 0.4f;
    const glm::vec3 bulletOrigin = glm::vec3(0.0625, 0, -0.5);
    const float bulletSpeed = 20.0f;
    const float bulletRadius = 0.05f;

    const glm::vec2 fontTexCoordScale = { 1.0f / 16.0f, 1.0f / 8.0f };

    std::unique_ptr<fff::PhysicsWorldInterface> physicsWorld;

    std::vector<JPH::Ref<JPH::Shape>> shapeRefs;
    JPH::Ref<JPH::CharacterVirtual> playerCharacter;
    JPH::Ref<JPH::Shape> bulletShape;

    std::vector<Enemy> enemies;
    std::vector<eng::Light> lights;
    std::vector<eng::Decal> bloodDecals;
    std::vector<Bullet> bullets;

    uint32_t animationCounter = 0;
    double animationTimer = 0;

    glm::vec3 cameraPosition = { 0, 2, 0 };
    float playerAngle = 0;
    PlayerStates::PlayerStates playerState = PlayerStates::Idle;
    float shootTimer = 0;
    bool shootButtonPressed = false;
    bool shootNext = false;

    std::string text = "X-Terminator 5000";

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

    void init(eng::ResourceLoaderInterface& resourceLoader, eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app, eng::AudioInterface& audio) override
    {
        audio.createLoop("resources/audio/GasStationTheme.wav");

        physicsWorld.reset(fff::createPhysicsWorld());
        // physicsWorld->getPhysicsSystem().SetGravity(JPH::Vec3(0, 0, 0));
        //
        physicsWorld->setOnCollisionEnter([this](const JPH::BodyID body0, const JPH::BodyID body1){
                    auto it = std::find_if(bullets.begin(), bullets.end(), [body0](const auto& bullet) { return bullet.bodyID == body0; });
                    if (it != bullets.end())
                    {
                        auto position = physicsWorld->getPhysicsSystem().GetBodyInterface().GetPosition(body0);

                        physicsWorld->getPhysicsSystem().GetBodyInterface().RemoveBody(body0);
                        physicsWorld->getPhysicsSystem().GetBodyInterface().DestroyBody(body0);
                        bullets.erase(it);

                        const auto [ idPair, contact ] = physicsWorld->getContacts(body0, body1).front();
                        bool first = idPair.GetBody1ID() == body0;

                        bloodDecals.push_back(eng::Decal {
                                .position = jph_to_glm(first ? contact.GetWorldSpaceContactPointOn2(0) : contact.GetWorldSpaceContactPointOn1(0)),
                                .scale = glm::vec3(1, 1, 0.05),
                                .rotation = glm::rotation(glm::vec3(0, 0, -1), jph_to_glm(contact.mWorldSpaceNormal)) * glm::angleAxis(glm::linearRand(0.0f, glm::pi<float>()), glm::vec3(0, 0, 1)),
                                .textureIndex = textures.splat,
                            });
                    }
                });

        textures = {
            .blank = resourceLoader.loadTexture("resources/textures/blank.png"),
            .lockOn = resourceLoader.loadTexture("resources/textures/corners.png"),
            .gun = resourceLoader.loadTexture("resources/textures/gun.png"),
            .muzzle_flash = resourceLoader.loadTexture("resources/textures/muzzle_flash.png"),
            .blood = resourceLoader.loadTexture("resources/textures/blood.png"),
            .rat = resourceLoader.loadTexture("resources/textures/rat.png"),
            .floor = resourceLoader.loadTexture("resources/textures/floor1_floortexrture.png"),
            .bullet = resourceLoader.loadTexture("resources/textures/bullet.png"),
            .splat = resourceLoader.loadTexture("resources/textures/splat.png"),
            .font = resourceLoader.loadTexture("resources/textures/font.png"),
        };

        for (uint32_t i = 0; i < PlayerStates::MAX_VALUE; ++i)
        {
            switch (i)
            {
                case PlayerStates::Idle: textures.player[i] = { resourceLoader.loadTexture("resources/textures/player/PCWalk2.png") }; break;
                case PlayerStates::Walk: textures.player[i] = getIndexedTextures(resourceLoader, "resources/textures/player/PCWalk{:}.png", 1, 4); break;
                case PlayerStates::Slide: textures.player[i] = { resourceLoader.loadTexture("resources/textures/player/PCSlide.png") }; break;
                default: throw std::runtime_error("not loaded player textures for state: " + std::to_string(i));
            }
        }
        textures.spiderWalk = getIndexedTextures(resourceLoader, "resources/textures/spider/SpiderEnemyWalk{:}.png", 2, 3);

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
            .shoot = {
                input.mapGamepadAxis(input.createMapping(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, eng::InputInterface::RealStateEvent::Threshold, 0.5f),
                input.mapMouseButton(input.createMapping(), SDL_BUTTON_LEFT),
            },
        };

        auto dungeon = Dungeon::generate(Dungeon::GenerationParams {
                    .seed = static_cast<uint64_t>(time(0)),
                    .width = 60,
                    .height = 40,
                    .partitionedRoomCount = 35,
                    .targetRoomCount = 12,
                    .minSplitDimension = 6,
                    .minPortalOverlap = 2,
                });
        dungeonGeometryResource = resourceLoader.createGeometry(dungeon.createGeometry(3, 1.0f, 0.5f, 2));

        std::vector<JPH::BodyID> mapBodies;
        dungeon.createPhysicsBodies(2, 1, 0.5, mapBodies, shapeRefs, physicsWorld->getPhysicsSystem());

        uint32_t playerStartRoom = dungeon.rooms.size();
        uint32_t playerStartRoomSize = 0;
        for (uint32_t i = 0; i < dungeon.rooms.size(); ++i)
        {
            if (dungeon.rooms[i].portalRecordsRange.end - dungeon.rooms[i].portalRecordsRange.start == 1)
            {
                uint32_t size = dungeon.rooms[i].width * dungeon.rooms[i].height;
                if (playerStartRoom == dungeon.rooms.size() || size < playerStartRoomSize)
                {
                    playerStartRoom = i;
                    playerStartRoomSize = size;
                }
            }
        }

        if (playerStartRoom == dungeon.rooms.size()) throw std::runtime_error("failed to generate player start location");


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

        glm::vec3 playerStartPosition(dungeon.rooms[playerStartRoom].x + dungeon.rooms[playerStartRoom].width / 2, 1,
                dungeon.rooms[playerStartRoom].y + dungeon.rooms[playerStartRoom].height / 2);

        const auto& enemyShape = shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(0.25f, 0.6, 0.25f)));

        for (const auto& room : dungeon.rooms)
        {
            if (&room == &dungeon.rooms[playerStartRoom]) continue;

            JPH::CharacterSettings characterSettings;
            characterSettings.mShape = enemyShape;
            characterSettings.mLayer = 1;
            characterSettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;

            glm::vec3 position(room.x + room.width / 2, 0, room.y + room.height / 2);
            enemies.push_back(Enemy {
                        .position = position,
                        .extents = glm::vec2(0.25, 0.25),
                        .character = new JPH::Character(&characterSettings, glm_to_jph(position), JPH::Quat::sIdentity(), 0, &physicsWorld->getPhysicsSystem()),
                    });
            enemies.back().character->AddToPhysicsSystem();
        }

        JPH::CharacterVirtualSettings characterSettings;
        characterSettings.mShape = shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(0.2f, 0.6f, 0.2f)));
        characterSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0);
        characterSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.2f);
        playerCharacter = new JPH::CharacterVirtual(&characterSettings,
                glm_to_jph(playerStartPosition), JPH::Quat::sIdentity(), &physicsWorld->getPhysicsSystem());

        bulletShape = shapeRefs.emplace_back(new JPH::SphereShape(bulletRadius));

        // app.setWantsCursorLock(true);
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

        sceneLayer.geometryInstances.push_back(eng::GeometryInstance {
                    .textureIndex = textures.floor,
                    .geometryIndex = dungeonGeometryResource,
                });

        sceneLayer.decals.clear();
        sceneLayer.decals.insert(sceneLayer.decals.end(), bloodDecals.begin(), bloodDecals.end());

        for (uint32_t i = 0; i < enemies.size(); ++i)
        {
            const auto& enemy = enemies[i];

            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = enemy.position,
                        .scale = glm::vec3(0.5),
                        .angle = enemy.angle,
                        .textureIndex = textures.spiderWalk[animationCounter % textures.spiderWalk.size()],
                        .tintColor = enemy.state == Enemy::State::Damaged ? glm::vec4(1.0, 0.5, 0.5, 1.0) : glm::vec4(1.0),
                    });
        }

        sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                    .position = jph_to_glm(playerCharacter->GetPosition()),
                    .scale = glm::vec3(0.5),
                    .angle = playerAngle,
                    .textureIndex = textures.player[playerState][animationCounter % textures.player[playerState].size()],
                });

        for (const auto& bullet : bullets)
        {
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = jph_to_glm(physicsWorld->getPhysicsSystem().GetBodyInterface().GetPosition(bullet.bodyID)),
                        .scale = glm::vec3(0.125f),
                        .angle = bullet.angle,
                        .textureIndex = textures.bullet,
                    });
        }

        sceneLayer.lights.push_back(eng::Light {
                    .position = jph_to_glm(playerCharacter->GetPosition()) + glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * glm::vec3(0.25, 1, 0),
                    .intensity = glm::vec3(5),
                });

        auto& overlayLayer = scene.layers()[1];
        overlayLayer.spriteInstances.clear();
        overlayLayer.geometryInstances.clear();
        overlayLayer.overlaySpriteInstances.clear();
        overlayLayer.viewport = { .offset = { 0, framebufferHeight }, .extent = { framebufferWidth, -static_cast<float>(framebufferHeight) } };
        overlayLayer.scissor = { .extent = { framebufferWidth, framebufferHeight } };
        overlayLayer.projection = glm::orthoRH_ZO(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
        overlayLayer.ambientLight = glm::vec3(1);
        
        const glm::vec2 textPos(-0.5, -0.875);
        const float textScale = 0.1;
        const float fontAspect = fontTexCoordScale.x / fontTexCoordScale.y;
        overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                .position = glm::vec3(textPos.x + 0.5 * text.size() * fontAspect * textScale, -textPos.y - 0.5 * textScale, 0),
                .scale = 0.5f * textScale * glm::vec3(fontAspect * text.size(), 1, 1),
                .textureIndex = textures.blank,
                .tintColor = glm::vec4(0, 0, 0, 1),
            });
        for (uint32_t i = 0; i < text.size(); ++i)
        {
            glm::vec2 minTexCoord = glm::vec2(text[i] / 8, text[i] % 8) * fontTexCoordScale;
            overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                    .position = glm::vec3(textPos.x + (i + 0.5) * fontAspect * textScale, -textPos.y - 0.5 * textScale, 0),
                    .scale = 0.5f * textScale * glm::vec3(fontAspect, 1, 1),
                    .minTexCoord = minTexCoord,
                    .texCoordScale = fontTexCoordScale,
                    .textureIndex = textures.font,
                    .tintColor = glm::vec4(1, 0, 0, 1),
                });
        }
    }

    void enemyUpdateDamaged(Enemy& enemy, const float deltaTime)
    {
        if (enemy.stateTime > 1.0f)
        {
            enemy.state = Enemy::State::Idle;
        }
    }

    void runFrame(eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app, eng::AudioInterface& audio, const double deltaTime) override
    {
        animationTimer += deltaTime;
        while (animationTimer >= 1.0 / animationFPS)
        {
            ++animationCounter;
            animationTimer -= 1.0 / animationFPS;
        }

        if (shootTimer > 0) shootTimer -= deltaTime;

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
            playerState = PlayerStates::Walk;
        }
        else if (auto len = glm::length(gamepadMoveInput); len > 0.2f)
        {
            updatePlayerPosition(gamepadMoveInput, deltaTime);
            playerState = PlayerStates::Walk;
        }
        else
        {
            updatePlayerPosition(glm::vec2(0), deltaTime);
            playerState = PlayerStates::Idle;
        }

        bool shoot = false;
        if (std::reduce(inputMappings.shoot.begin(), inputMappings.shoot.end(), false, [&](const bool state, const uint32_t mapping) {
                        return state || input.getBoolean(mapping);
                    }))
        {
            if (!shootButtonPressed)
            {
                shootButtonPressed = true;
                if (shootTimer > 0)
                {
                    shootNext = true;
                }
                else
                {
                    shoot = true;
                }
            }
        }
        else
        {
            shootButtonPressed = false;
        }
        if (shootNext && shootTimer <= 0)
        {
            shoot = true;
            shootNext = false;
        }
        if (shoot)
        {
            shootTimer = shootTimeout;
            auto& bullet = bullets.emplace_back(
                    physicsWorld->getPhysicsSystem().GetBodyInterface().CreateAndAddBody(
                        JPH::BodyCreationSettings(bulletShape,
                            playerCharacter->GetPosition() + glm_to_jph(glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * bulletOrigin),
                            JPH::Quat::sIdentity(),
                            JPH::EMotionType::Dynamic, 1),
                        JPH::EActivation::Activate),
                    playerAngle);
            physicsWorld->getPhysicsSystem().GetBodyInterface().SetLinearVelocity(bullet.bodyID,
                    glm_to_jph(glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * glm::vec3(0, 0, -1) * bulletSpeed));
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

        for (const auto& contact : playerCharacter->GetActiveContacts())
        {
            if (contact.mHadCollision && contact.mContactNormal.GetY() < -0.1)
            {
                const auto normal = jph_to_glm(contact.mContactNormal);
                velocity -= glm::dot(normal, velocity) * normal;
            }
        }
        
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
