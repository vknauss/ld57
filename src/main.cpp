#include "engine.hpp"
#include "obj_load.hpp"
#include "physics.hpp"
#include "dungeon.hpp"
#include "jph_glm_convert.hpp"

#include <iostream>
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

namespace EnemyAnimationStates
{
    enum EnemyAnimationStates
    {
        Walk,
        Shooting,
        Damage,
        MAX_VALUE,
    };
};

struct Enemy
{
    enum class State
    {
        Initial, Idle, Damaged, Pursuing, Targeting, Firing, Dead,
    };

    glm::vec3 position = glm::vec3(0);
    float angle = 0;
    glm::vec2 extents = glm::vec3(0.5);
    State state = State::Idle;
    State lastState = State::Initial;
    float stateTime = 0.0f;
    int health = 5;
    int maxHealth = 5;
    glm::vec3 poi;
    JPH::Ref<JPH::Character> character;
    int animationState = EnemyAnimationStates::Walk;
    uint32_t animationOffset = 0;
    bool loopAnimation = true;
};

namespace PlayerStates
{
    enum PlayerStates
    {
        Idle,
        Walk,
        Slide,
        Damaged,
        Shooting,
        Dead,
        FallingInHole,
        FallenInHole,
        Dazed,
        MAX_VALUE,
    };
};

struct Bullet
{
    JPH::BodyID bodyID;
    float angle;
    bool friendly;
};

struct CooldownTrigger
{
    std::vector<uint32_t> inputs;
    float cooldown;
    float timer = 0;
    bool buttonPressed = false;
    bool triggerNext = false;
    bool trigger = false;

    void update(const eng::InputInterface& input, const float deltaTime)
    {
        if (timer > 0) timer -= deltaTime;
        trigger = false;
        if (std::reduce(inputs.begin(), inputs.end(), false, [&](const bool state, const uint32_t mapping) {
                        return state || input.getBoolean(mapping);
                    }))
        {
            if (!buttonPressed)
            {
                buttonPressed = true;
                if (timer > 0)
                {
                    triggerNext = true;
                }
                else
                {
                    trigger = true;
                }
            }
        }
        else
        {
            buttonPressed = false;
        }
        if (triggerNext && timer <= 0)
        {
            trigger = true;
            triggerNext = false;
            timer = cooldown;
        }
    }
};

struct GameCommon
{
    const int numDungeons = 3;

    struct {
        uint32_t blank;
        uint32_t blood;
        uint32_t floor;
        uint32_t wall;
        std::vector<uint32_t> bullet;
        std::vector<uint32_t> spiderBullet;
        uint32_t splat;
        uint32_t spiderweb;
        uint32_t font;
        std::vector<uint32_t> player[PlayerStates::MAX_VALUE];
        std::vector<uint32_t> spider[EnemyAnimationStates::MAX_VALUE];
        std::vector<uint32_t> hole;
        std::vector<uint32_t> muzzleFlash;
    } textures;

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
        std::vector<uint32_t> slide;
    } inputMappings;

    std::vector<Dungeon> dungeons;
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> dungeonGeometryResourcePairs;

    GameCommon(eng::ResourceLoaderInterface& resourceLoader, eng::InputInterface& input)
    {
        textures = {
            .blank = resourceLoader.loadTexture("resources/textures/blank.png"),
            .blood = resourceLoader.loadTexture("resources/textures/Goop1.png"),
            .floor = resourceLoader.loadTexture("resources/textures/floor1_floortexrture.png"),
            .wall = resourceLoader.loadTexture("resources/textures/woodWallTexture.png"),
            .bullet = getIndexedTextures(resourceLoader, "resources/textures/pc_projectile/PCProjectile{:}.png", 1, 2),
            .spiderBullet = getIndexedTextures(resourceLoader, "resources/textures/spider/SpiderProjectile{:}.png", 2, 2),
            .splat = resourceLoader.loadTexture("resources/textures/Goop2.png"),
            .spiderweb = resourceLoader.loadTexture("resources/textures/Spiderweb.png"),
            .font = resourceLoader.loadTexture("resources/textures/font.png"),
            .hole = getIndexedTextures(resourceLoader, "resources/textures/hole/FloorFallingThruAnim{:}.png", 1, 8),
            .muzzleFlash = getIndexedTextures(resourceLoader, "resources/textures/muzzleflash/PCMuzzleFlash{:}.png", 1, 2),
        };

        for (uint32_t i = 0; i < PlayerStates::MAX_VALUE; ++i)
        {
            switch (i)
            {
                case PlayerStates::Idle: textures.player[i] = { resourceLoader.loadTexture("resources/textures/player/PCWalk2.png") }; break;
                case PlayerStates::Walk: textures.player[i] = getIndexedTextures(resourceLoader, "resources/textures/player/PCWalk{:}.png", 1, 4); break;
                case PlayerStates::Slide: textures.player[i] = { resourceLoader.loadTexture("resources/textures/player/PCSlide.png") }; break;
                case PlayerStates::Damaged: textures.player[i] = getIndexedTextures(resourceLoader, "resources/textures/player/PCDamageFrames{:}.png", 1, 2); break;
                case PlayerStates::Shooting: textures.player[i] = { resourceLoader.loadTexture("resources/textures/player/PCShooting.png") }; break;
                default: break;
            }
        }
        textures.player[PlayerStates::FallingInHole] = textures.player[PlayerStates::Idle];
        textures.player[PlayerStates::Dazed] = textures.player[PlayerStates::Slide];

        textures.spider[EnemyAnimationStates::Walk] = getIndexedTextures(resourceLoader, "resources/textures/spider/SpiderEnemyWalk{:}.png", 2, 3);
        textures.spider[EnemyAnimationStates::Shooting] = getIndexedTextures(resourceLoader, "resources/textures/spider/SpiderShooting{:}.png", 1, 3);
        textures.spider[EnemyAnimationStates::Damage] = getIndexedTextures(resourceLoader, "resources/textures/spider/SpiderDamage{:}.png", 1, 2);

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
            .slide {
                input.mapGamepadButton(input.createMapping(), SDL_GAMEPAD_BUTTON_EAST),
                input.mapKey(input.createMapping(), SDL_GetScancodeFromKey(SDLK_SPACE, nullptr)),
            },
        };

        dungeons.reserve(numDungeons);
        dungeonGeometryResourcePairs.reserve(numDungeons);
        for (int i = 0; i < numDungeons; ++i)
        {
            auto dungeon = Dungeon::generate(Dungeon::GenerationParams {
                        .seed = static_cast<uint64_t>(time(0)),
                        .width = 60,
                        .height = 40,
                        .partitionedRoomCount = 25,
                        .targetRoomCount = 8,
                        .minSplitDimension = 6,
                        .minPortalOverlap = 2,
                    });
            auto geometry = dungeon.createGeometry(3, 1.0f, 0.5f, 2, 1);
            dungeonGeometryResourcePairs.push_back({
                    { textures.floor, resourceLoader.createGeometry(geometry.floor) },
                    { textures.wall, resourceLoader.createGeometry(geometry.walls) },
                    { textures.wall, resourceLoader.createGeometry(geometry.obstacleSides) },
                    { textures.wall, resourceLoader.createGeometry(geometry.obstacleTops) },
                });
            dungeons.push_back(std::move(dungeon));
        }
    }
};

struct GameSceneRunner : public JPH::CharacterContactListener
{
    const GameCommon& common;
    uint32_t dungeonIndex;

    const float movementSpeed = 7.0f;
    const float slideSpeed = 12.0f;
    const float cosineThresholdCardinal = glm::cos(glm::radians(22.5));
    const uint32_t animationFPS = 8;
    const float shootCooldown = 0.4f;
    const float slideCooldown = 0.4f;
    const glm::vec3 bulletOrigin = glm::vec3(0.0625, 0, -0.5);
    const float bulletSpeed = 20.0f;
    const float bulletRadius = 0.05f;
    const int playerMaxHealth = 10;
    const float slideTime = 3.0f;
    const float shootTime = 2.0f;
    const glm::vec2 fontTexCoordScale = { 1.0f / 16.0f, 1.0f / 8.0f };
    const float fadeOutTime = 3.0f;
    const float fadeInTime = 3.0f;
    const float maxLightIntensity = 1.0f;
    const float maxAmbientLightIntensity = 0.1;
    const float minAmbientLightIntensity = 0.001;

    std::unique_ptr<fff::PhysicsWorldInterface> physicsWorld;

    std::vector<JPH::Ref<JPH::Shape>> shapeRefs;
    JPH::Ref<JPH::CharacterVirtual> playerCharacter;
    JPH::Ref<JPH::Shape> bulletShape;
    JPH::Ref<JPH::Shape> characterShape;

    std::vector<Enemy> enemies;
    std::vector<eng::Decal> decals;
    std::vector<Bullet> bullets;

    uint32_t animationCounter = 0;
    double animationTimer = 0;

    glm::vec3 cameraPosition = { 0, 2, 0 };
    float playerAngle = 0;
    PlayerStates::PlayerStates playerState = PlayerStates::Dazed;
    PlayerStates::PlayerStates lastPlayerState = PlayerStates::MAX_VALUE;
    float playerStateTimer = 0;
    uint32_t playerStateAnimationOffset = 0;
    glm::vec3 playerSlideVelocity = glm::vec3(0);

    CooldownTrigger shootTrigger;
    CooldownTrigger slideTrigger;

    int playerHealth = playerMaxHealth;
    uint32_t holeAnimationOffset = 0;
    float lightIntensity = 0;
    float ambientLightIntensity = minAmbientLightIntensity;

    enum class State
    {
        Running,
        GameOver,
        Completed,
    } state = State::Running;

    GameSceneRunner(const GameCommon& common, uint32_t dungeonIndex) :
        common(common),
        dungeonIndex(dungeonIndex)
    {
        physicsWorld.reset(fff::createPhysicsWorld());
        physicsWorld->setOnCollisionEnter([this](const JPH::BodyID body0, const JPH::BodyID body1) {
                onCollisionEnter(body0, body1);
            });

        shootTrigger.cooldown = shootCooldown;
        shootTrigger.inputs = common.inputMappings.shoot;

        slideTrigger.cooldown = slideCooldown;
        slideTrigger.inputs = common.inputMappings.slide;

        const auto& dungeon = common.dungeons[dungeonIndex];
        std::vector<JPH::BodyID> mapBodies;
        dungeon.createPhysicsBodies(2, 1, 0.5, mapBodies, shapeRefs, physicsWorld->getPhysicsSystem());

        glm::vec3 playerStartPosition(dungeon.playerSpawn.first + 0.5, 1, dungeon.playerSpawn.second + 0.5);

        const auto& enemyShape = shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(0.25f, 0.6, 0.25f)));

        for (const auto& [x, y] : dungeon.spawnPoints)
        {
            JPH::CharacterSettings characterSettings;
            characterSettings.mShape = enemyShape;
            characterSettings.mLayer = 1;
            characterSettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;

            glm::vec3 position(x + 0.5, 0, y + 0.5);
            enemies.push_back(Enemy {
                        .position = position,
                        .extents = glm::vec2(0.25, 0.25),
                        .character = new JPH::Character(&characterSettings, glm_to_jph(position), JPH::Quat::sIdentity(), 0, &physicsWorld->getPhysicsSystem()),
                    });
            enemies.back().character->AddToPhysicsSystem();
        }

        characterShape = shapeRefs.emplace_back(new JPH::BoxShape(JPH::Vec3(0.2f, 0.6f, 0.2f)));

        JPH::CharacterVirtualSettings characterSettings;
        characterSettings.mShape = characterShape;
        characterSettings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0);
        characterSettings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.2f);
        playerCharacter = new JPH::CharacterVirtual(&characterSettings,
                glm_to_jph(playerStartPosition), JPH::Quat::sIdentity(), &physicsWorld->getPhysicsSystem());
        playerCharacter->SetListener(this);

        bulletShape = shapeRefs.emplace_back(new JPH::SphereShape(bulletRadius));
    }

    ~GameSceneRunner()
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

    // Player contact
    void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::BodyID& bodyID1, const JPH::SubShapeID& subShapeID1,
            const JPH::RVec3Arg contactPosition, const JPH::Vec3Arg contactNormal, JPH::CharacterContactSettings& ioSettings) override
    {
        if (auto bulletIter = std::find_if(bullets.begin(), bullets.end(),
                    [bodyID1](const auto& bullet) { return bullet.bodyID == bodyID1; });
                bulletIter != bullets.end())
        {
            physicsWorld->getPhysicsSystem().GetBodyInterface().RemoveBody(bodyID1);
            physicsWorld->getPhysicsSystem().GetBodyInterface().DestroyBody(bodyID1);
            bullets.erase(bulletIter);

            if (character == playerCharacter && playerState != PlayerStates::Slide)
            {
                playerState = PlayerStates::Damaged;
            }
        }
    }

    // generic bodies
    void onCollisionEnter(const JPH::BodyID body0, const JPH::BodyID body1)
    {
        if (auto bulletIter = std::find_if(bullets.begin(), bullets.end(),
                    [body0](const auto& bullet) { return bullet.bodyID == body0; });
                bulletIter != bullets.end())
        {
            const bool friendly = bulletIter->friendly;
            physicsWorld->getPhysicsSystem().GetBodyInterface().RemoveBody(body0);
            physicsWorld->getPhysicsSystem().GetBodyInterface().DestroyBody(body0);
            bullets.erase(bulletIter);

            if (physicsWorld->getPhysicsSystem().GetBodyInterface().GetObjectLayer(body1) == 0)
            {
                const auto [ idPair, contact ] = physicsWorld->getContacts(body0, body1).front();
                bool first = idPair.GetBody1ID() == body0;

                decals.push_back(eng::Decal {
                        .position = jph_to_glm(first ? contact.GetWorldSpaceContactPointOn2(0) : contact.GetWorldSpaceContactPointOn1(0)),
                        .scale = glm::vec3(1, 1, 0.05),
                        .rotation = glm::rotation(glm::vec3(0, 0, -1), jph_to_glm(contact.mWorldSpaceNormal)) * glm::angleAxis(glm::linearRand(0.0f, glm::pi<float>()), glm::vec3(0, 0, 1)),
                        .textureIndex = friendly? common.textures.splat : common.textures.spiderweb,
                    });
            }

            else if (auto enemyIter = std::find_if(enemies.begin(), enemies.end(),
                        [body1](const auto& enemy) { return enemy.character->GetBodyID() == body1; });
                    enemyIter != enemies.end())
            {
                enemyIter->state = Enemy::State::Damaged;
            }
        }
    }

    void runFrame(eng::InputInterface& input, eng::AppInterface& app, eng::AudioInterface& audio, const double deltaTime)
    {
        animationTimer += deltaTime;
        while (animationTimer >= 1.0 / animationFPS)
        {
            ++animationCounter;
            animationTimer -= 1.0 / animationFPS;
        }

        const auto [windowWidth, windowHeight] = app.getWindowSize();
        const glm::vec2 mouseLookInput = {
            input.getReal(common.inputMappings.mouseLookX) - 0.5f * windowWidth,
            input.getReal(common.inputMappings.mouseLookY) - 0.5f * windowHeight,
        };

        const glm::vec2 gamepadLookInput = {
            input.getReal(common.inputMappings.gpRightStickXAxis),
            input.getReal(common.inputMappings.gpRightStickYAxis),
        };

        const glm::vec2 keyboardMoveInput = {
            static_cast<float>(input.getBoolean(common.inputMappings.right)) - static_cast<float>(input.getBoolean(common.inputMappings.left)),
            static_cast<float>(input.getBoolean(common.inputMappings.forward)) - static_cast<float>(input.getBoolean(common.inputMappings.back))
        };

        const glm::vec2 gamepadMoveInput = {
            input.getReal(common.inputMappings.gpLeftStickXAxis), -input.getReal(common.inputMappings.gpLeftStickYAxis),
        };

        shootTrigger.update(input, deltaTime);
        slideTrigger.update(input, deltaTime);

        if (playerState != lastPlayerState)
        {
            if (playerState == PlayerStates::Damaged)
            {
                playerHealth --;
                audio.createSingleShot("resources/audio/characterhit.wav");
            }
            else if (playerState == PlayerStates::Slide)
            {
                audio.createSingleShot("resources/audio/XTerminatorSlideSound.wav");
                glm::vec2 moveInput(0, -1);
                if (glm::any(glm::greaterThan(glm::abs(keyboardMoveInput), glm::vec2(0))))
                {
                    moveInput = keyboardMoveInput;
                }
                else if (auto len = glm::length(gamepadMoveInput); len > 0.2f)
                {
                    moveInput = gamepadMoveInput;
                }

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
                playerAngle = atan2(-moveInput.x, moveInput.y);

                auto velocity = jph_to_glm(playerCharacter->GetLinearVelocity());
                playerCharacter->SetLinearVelocity(glm_to_jph(slideSpeed * direction + glm::vec3(0, velocity.y, 0)));
            }
            else if (playerState == PlayerStates::Shooting)
            {
                audio.createSingleShot("resources/audio/shotfx.wav");
                auto& bullet = bullets.emplace_back(
                        physicsWorld->getPhysicsSystem().GetBodyInterface().CreateAndAddBody(
                            JPH::BodyCreationSettings(bulletShape,
                                playerCharacter->GetPosition() + glm_to_jph(glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * bulletOrigin),
                                JPH::Quat::sIdentity(),
                                JPH::EMotionType::Dynamic, 1),
                            JPH::EActivation::Activate),
                        playerAngle, true);
                physicsWorld->getPhysicsSystem().GetBodyInterface().SetLinearVelocity(bullet.bodyID,
                        glm_to_jph(glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * glm::vec3(0, 0, -1) * bulletSpeed));
            }
            else if (playerState == PlayerStates::Dead)
            {
                audio.createSingleShot("resources/audio/Gameoverfx.wav");
                state = State::GameOver;
                return;
            }
            else if (playerState == PlayerStates::FallingInHole)
            {
                holeAnimationOffset = animationCounter;
            }

            lastPlayerState = playerState;
            playerStateTimer = 0;
            playerStateAnimationOffset = animationCounter;
        }

        if (playerState == PlayerStates::Damaged)
        {
            if (playerStateTimer > static_cast<float>(common.textures.player[PlayerStates::Damaged].size()) / animationFPS)
            {
                if (playerHealth > 0)
                    playerState = PlayerStates::Idle;
                else
                    playerState = PlayerStates::Dead;
            }
        }
        else if (playerState == PlayerStates::Slide)
        {
            if (playerStateTimer > slideTime / animationFPS)
            {
                playerState = PlayerStates::Idle;
            }
        }
        else if (playerState == PlayerStates::Shooting)
        {
            if (playerStateTimer > shootTime / animationFPS)
            {
                playerState = PlayerStates::Idle;
            }
        }
        else if (playerState == PlayerStates::FallingInHole)
        {
            if (animationCounter - holeAnimationOffset >= common.textures.hole.size())
            {
                playerState = PlayerStates::FallenInHole;
            }
        }
        else if (playerState == PlayerStates::FallenInHole)
        {
            lightIntensity = std::max(maxLightIntensity * (1.0f - playerStateTimer / fadeOutTime), 0.0f);
            ambientLightIntensity = std::max(minAmbientLightIntensity + (maxAmbientLightIntensity - minAmbientLightIntensity) * (1.0f - playerStateTimer / fadeOutTime),
                    minAmbientLightIntensity);
            if (playerStateTimer >= fadeOutTime)
            {
                state = State::Completed;
                return;
            }
        }
        else if (playerState == PlayerStates::Dazed)
        {
            lightIntensity = std::min(maxLightIntensity * (playerStateTimer / fadeInTime), maxLightIntensity);
            ambientLightIntensity = std::min(minAmbientLightIntensity + (maxAmbientLightIntensity - minAmbientLightIntensity) * (playerStateTimer / fadeOutTime),
                    maxAmbientLightIntensity);
            if (playerStateTimer >= fadeInTime)
            {
                playerState = PlayerStates::Idle;
            }
        }

        const bool playerInputAllowed = playerState == PlayerStates::Idle
            || playerState == PlayerStates::Walk
            || playerState == PlayerStates::Damaged
            || playerState == PlayerStates::Shooting;

        bool isPlayerMoving = false;

        if (playerInputAllowed)
        {
            if (glm::any(glm::greaterThan(glm::abs(mouseLookInput), glm::vec2(0))))
            {
                playerAngle = atan2(-mouseLookInput.x, -mouseLookInput.y);
            }
            else if (glm::any(glm::greaterThan(glm::abs(gamepadLookInput), glm::vec2(0.2))))
            {
                playerAngle = atan2(gamepadLookInput.y, gamepadLookInput.x);
            }

            if (glm::any(glm::greaterThan(glm::abs(keyboardMoveInput), glm::vec2(0))))
            {
                updatePlayerPosition(keyboardMoveInput, deltaTime);
                isPlayerMoving = true;
            }
            else if (auto len = glm::length(gamepadMoveInput); len > 0.2f)
            {
                updatePlayerPosition(gamepadMoveInput, deltaTime);
                isPlayerMoving = true;
            }
            else
            {
                updatePlayerPosition(glm::vec2(0), deltaTime);
            }

            if (slideTrigger.trigger)
            {
                playerState = PlayerStates::Slide;
            }
            if (shootTrigger.trigger)
            {
                playerState = PlayerStates::Shooting;
            }
        }

        if (playerState == PlayerStates::Idle && isPlayerMoving)
        {
            playerState = PlayerStates::Walk;
        }
        else if (playerState == PlayerStates::Walk && !isPlayerMoving)
        {
            playerState = PlayerStates::Idle;
        }

        playerStateTimer += deltaTime;

        for (auto& enemy : enemies)
        {
            enemy.position = jph_to_glm(enemy.character->GetPosition());

            const auto findPoi = [&]() {
                const float maxDistance = glm::linearRand(1.0f, 5.0f);
                const glm::vec2 dir = glm::circularRand(maxDistance);
                enemy.poi = jph_to_glm(enemy.character->GetPosition()) + glm::vec3(dir.x, 0, dir.y);
                const JPH::RRayCast raycast(enemy.character->GetPosition(), JPH::Vec3(dir.x, 0, dir.y));
                JPH::RayCastResult raycastResult;
                if (physicsWorld->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(raycast, raycastResult, JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0))))
                {
                    JPH::BodyLockRead lock(physicsWorld->getPhysicsSystem().GetBodyLockInterface(), raycastResult.mBodyID);
                    if (lock.Succeeded())
                    {
                        enemy.poi = jph_to_glm(raycast.GetPointOnRay(glm::max(0.0f, raycastResult.mFraction - 0.1f / maxDistance)));
                    }
                }
            };

            const auto sightlineToPlayer = [&]() {
                const JPH::RRayCast raycast(enemy.character->GetPosition(), playerCharacter->GetPosition() - enemy.character->GetPosition());
                JPH::RayCastResult raycastResult;
                if (physicsWorld->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(raycast, raycastResult, JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0))))
                {
                    return false;
                }
                return true;
            };

            if (enemy.state != enemy.lastState)
            {
                if (enemy.state == Enemy::State::Idle)
                {
                    findPoi();
                }
                else if (enemy.state == Enemy::State::Damaged)
                {
                    audio.createSingleShot("resources/audio/attacksound.wav");

                    const glm::vec3 deltaPos = enemy.position - cameraPosition;
                    const glm::vec3 deltaHPos = glm::vec3(deltaPos.x, 0, deltaPos.z);
                    if (glm::dot(deltaHPos, deltaHPos) > glm::epsilon<float>())
                    {
                        const glm::vec3 forward = glm::normalize(deltaHPos);
                        enemy.character->AddImpulse(glm_to_jph(forward) * 100.0f);
                    }

                    const JPH::RRayCast raycast(enemy.character->GetPosition(),
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
                            decals.push_back(eng::Decal {
                                    .position = jph_to_glm(position),
                                    .scale = glm::vec3(1, 1, 0.05),
                                    .rotation = glm::rotation(glm::vec3(0, 0, -1), jph_to_glm(normal)) * glm::angleAxis(glm::linearRand(0.0f, glm::pi<float>()), glm::vec3(0, 0, 1)),
                                    .textureIndex = common.textures.blood,
                                });
                        }
                    }

                    --enemy.health;
                }
                else if (enemy.state == Enemy::State::Firing)
                {
                    audio.createSingleShot("resources/audio/spiderattackfx.wav");
                    auto& bullet = bullets.emplace_back(
                            physicsWorld->getPhysicsSystem().GetBodyInterface().CreateAndAddBody(
                                JPH::BodyCreationSettings(bulletShape,
                                    enemy.character->GetPosition() + glm_to_jph(glm::angleAxis(enemy.angle, glm::vec3(0, 1, 0)) * glm::vec3(0, 0, -0.5)),
                                    JPH::Quat::sIdentity(),
                                    JPH::EMotionType::Dynamic, 1),
                                JPH::EActivation::Activate),
                            enemy.angle, false);
                    physicsWorld->getPhysicsSystem().GetBodyInterface().SetLinearVelocity(bullet.bodyID,
                            glm_to_jph(glm::angleAxis(enemy.angle, glm::vec3(0, 1, 0)) * glm::vec3(0, 0, -1) * bulletSpeed));
                }
                else if (enemy.state == Enemy::State::Dead)
                {
                    enemy.character->RemoveFromPhysicsSystem();
                }

                enemy.stateTime = 0;
                enemy.lastState = enemy.state;
                if (enemy.state == Enemy::State::Damaged)
                {
                    enemy.animationState = EnemyAnimationStates::Damage;
                    enemy.loopAnimation = false;
                    enemy.animationOffset = animationCounter;
                }
                else if (enemy.state == Enemy::State::Targeting)
                {
                    enemy.animationState = EnemyAnimationStates::Shooting;
                    enemy.loopAnimation = false;
                    enemy.animationOffset = animationCounter;
                }
                else
                {
                    enemy.animationState = EnemyAnimationStates::Walk;
                    enemy.loopAnimation = true;
                }
            }

            if (enemy.state == Enemy::State::Idle)
            {
                if (glm::distance2(enemy.position, enemy.poi) < 1)
                {
                    findPoi();
                }
                else
                {
                    enemy.angle = glm::atan(enemy.position.x - enemy.poi.x, enemy.position.z - enemy.poi.z);
                    enemy.character->SetLinearVelocity(glm_to_jph(glm::normalize(enemy.poi - enemy.position)));
                }

                if (enemy.character->GetPosition().IsClose(playerCharacter->GetPosition(), 2.0f) && sightlineToPlayer())
                {
                    enemy.state = Enemy::State::Targeting;
                }
            }
            else if (enemy.state == Enemy::State::Damaged)
            {
                if (enemy.stateTime >= static_cast<float>(common.textures.spider[EnemyAnimationStates::Damage].size()) / animationFPS)
                {
                    if (enemy.health > 0)
                        enemy.state = Enemy::State::Idle;
                    else
                        enemy.state = Enemy::State::Dead;
                }
            }
            else if (enemy.state == Enemy::State::Targeting)
            {
                auto playerPosition = playerCharacter->GetPosition();
                enemy.angle = glm::atan(enemy.position.x - playerPosition.GetX(), enemy.position.z - playerPosition.GetZ());
                if (enemy.stateTime >= 0.5)
                {
                    enemy.state = Enemy::State::Firing;
                }
                if (!sightlineToPlayer())
                {
                    enemy.state = Enemy::State::Idle;
                }
            }
            else if (enemy.state == Enemy::State::Firing)
            {
                if (enemy.stateTime >= 0.5)
                {
                    enemy.state = Enemy::State::Idle;
                }
            }

            enemy.stateTime += deltaTime;

            if (enemy.lastState != enemy.state)
            {
                enemy.stateTime = 0;
                if (enemy.state == Enemy::State::Damaged)
                {
                    enemy.animationState = EnemyAnimationStates::Damage;
                    enemy.loopAnimation = false;
                    enemy.animationOffset = animationCounter;
                }
                else if (enemy.state == Enemy::State::Targeting)
                {
                    enemy.animationState = EnemyAnimationStates::Shooting;
                    enemy.loopAnimation = false;
                    enemy.animationOffset = animationCounter;
                }
                else
                {
                    enemy.animationState = EnemyAnimationStates::Walk;
                    enemy.loopAnimation = true;
                }
            }
        }

        if (std::erase_if(enemies, [](const auto& enemy){ return enemy.lastState == Enemy::State::Dead; }) && enemies.empty())
        {
            playerState = PlayerStates::FallingInHole;
        }

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

        if (lastPlayerState != playerState)
        {
            playerStateTimer = 0;
            playerStateAnimationOffset = animationCounter;
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
        sceneLayer.ambientLight = glm::vec3(ambientLightIntensity);
        sceneLayer.lights.clear();

        for (const auto [ textureIndex, geometryIndex ] : common.dungeonGeometryResourcePairs[dungeonIndex])
        {
            sceneLayer.geometryInstances.push_back(eng::GeometryInstance {
                        .textureIndex = textureIndex,
                        .geometryIndex = geometryIndex,
                    });
        }

        sceneLayer.decals.clear();
        sceneLayer.decals.insert(sceneLayer.decals.end(), decals.begin(), decals.end());

        for (uint32_t i = 0; i < enemies.size(); ++i)
        {
            const auto& enemy = enemies[i];

            const auto& frames = common.textures.spider[enemy.animationState];
            if (!frames.empty())
            {
                uint32_t frame = animationCounter - enemy.animationOffset;
                frame = enemy.loopAnimation ? frame % frames.size() : std::min<uint32_t>(frame, frames.size() - 1);
                sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                            .position = enemy.position,
                            .scale = glm::vec3(0.5),
                            .angle = enemy.angle,
                            .textureIndex = frames[frame],
                        });
            }

            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = enemy.position + glm::vec3(0, 0, 0.3f),
                        .scale = 0.25f * glm::vec3(static_cast<float>(enemy.health) / enemy.maxHealth, 0.1f, 0),
                        .textureIndex = common.textures.blank,
                        .tintColor = glm::vec4(1, 0, 0, 1),
                    });
        }

        if (lastPlayerState == PlayerStates::FallingInHole || lastPlayerState == PlayerStates::FallenInHole)
        {
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = jph_to_glm(playerCharacter->GetPosition()),
                        .scale = glm::vec3(0.5),
                        .textureIndex = common.textures.hole[std::min<uint32_t>(animationCounter - holeAnimationOffset, common.textures.hole.size()-1)],
                    });
        }
        if (!common.textures.player[playerState].empty())
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = jph_to_glm(playerCharacter->GetPosition()),
                        .scale = glm::vec3(0.5),
                        .angle = playerAngle,
                        .textureIndex = common.textures.player[playerState][animationCounter % common.textures.player[playerState].size()],
                    });
        if (playerState == PlayerStates::Shooting)
        {
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = jph_to_glm(playerCharacter->GetPosition()) + glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * bulletOrigin,
                        .scale = glm::vec3(0.5),
                        .angle = playerAngle,
                        .textureIndex = common.textures.muzzleFlash[std::min<uint32_t>(animationCounter - playerStateAnimationOffset, common.textures.muzzleFlash.size() - 1)],
                    });
            sceneLayer.lights.push_back(eng::Light {
                        .position = jph_to_glm(playerCharacter->GetPosition()) + glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * bulletOrigin,
                        .intensity = glm::vec3(0, 0.5, 0.2),
                    });
        }

        for (const auto& bullet : bullets)
        {
            sceneLayer.spriteInstances.push_back(eng::SpriteInstance {
                        .position = jph_to_glm(physicsWorld->getPhysicsSystem().GetBodyInterface().GetPosition(bullet.bodyID)),
                        .scale = glm::vec3(0.5f),
                        .angle = bullet.angle,
                        .textureIndex = bullet.friendly ? common.textures.bullet[animationCounter % common.textures.bullet.size()]
                            : common.textures.spiderBullet[animationCounter % common.textures.spiderBullet.size()],
                    });
        }

        sceneLayer.lights.push_back(eng::Light {
                    .position = jph_to_glm(playerCharacter->GetPosition()) + glm::angleAxis(playerAngle, glm::vec3(0, 1, 0)) * glm::vec3(0.25, 1, 0),
                    .intensity = glm::vec3(lightIntensity),
                });

        auto& overlayLayer = scene.layers()[1];
        overlayLayer.spriteInstances.clear();
        overlayLayer.geometryInstances.clear();
        overlayLayer.overlaySpriteInstances.clear();
        overlayLayer.viewport = { .offset = { 0, framebufferHeight }, .extent = { framebufferWidth, -static_cast<float>(framebufferHeight) } };
        overlayLayer.scissor = { .extent = { framebufferWidth, framebufferHeight } };
        overlayLayer.projection = glm::orthoRH_ZO(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
        overlayLayer.ambientLight = glm::vec3(1);
        
        std::string text = (std::stringstream{} << "enemies remaining: " << enemies.size()).str();
        const glm::vec2 textPos(-0.5, -0.875);
        const float textScale = 0.1;
        const float fontAspect = fontTexCoordScale.x / fontTexCoordScale.y;
        overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                .position = glm::vec3(textPos.x + 0.5 * text.size() * fontAspect * textScale, -textPos.y - 0.5 * textScale, 0),
                .scale = 0.5f * textScale * glm::vec3(fontAspect * text.size(), 1, 1),
                .textureIndex = common.textures.blank,
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
                    .textureIndex = common.textures.font,
                    .tintColor = glm::vec4(1, 0, 0, 1),
                });
        }

        const glm::vec2 healthPos(-aspectRatio + 0.1, 0.8);
        const float healthScale = 0.1f;
        const float healthSpacing = 0.15f;
        for (int i = 0; i < playerHealth; ++i)
        {
            overlayLayer.spriteInstances.push_back(eng::SpriteInstance {
                    .position = glm::vec3(healthPos.x + 0.5 * healthScale + i * healthSpacing, -healthPos.y - 0.5 * healthScale, 0),
                    .scale = glm::vec3(0.5f * healthScale),
                    .textureIndex = common.textures.blank,
                    .tintColor = glm::vec4(1, 0, 0, 1),
                });
        }
    }
};

struct GameLogic final: eng::GameLogicInterface
{
    const int numDungeons = 3;
    int currentDungeon = 0;
    std::unique_ptr<GameCommon> common;
    std::unique_ptr<GameSceneRunner> sceneRunner;

    std::vector<uint32_t> anyActionInputs;
    bool lastPressed = false;
    uint32_t themeLoop;

    struct Screens
    {
        enum ScreenIndex
        {
            Title,
            Lose,
            Win,
            MAX_VALUE,
        };
    };

    std::vector<uint32_t> screens[Screens::MAX_VALUE];
    int currentScreen = Screens::Title;

    const uint32_t animationFPS = 8;
    uint32_t animationCounter = 0;
    double animationTimer = 0;

    void init(eng::ResourceLoaderInterface& resourceLoader, eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app, eng::AudioInterface& audio) override
    {
        themeLoop = audio.createLoop("resources/audio/GasStationThemereal.wav");

        common.reset(new GameCommon(resourceLoader, input));
        anyActionInputs = {
            input.mapAnyKey(input.createMapping()),
            input.mapAnyMouseButton(input.createMapping()),
            input.mapAnyGamepadButton(input.createMapping()),
        };

        screens[Screens::Title] = { resourceLoader.loadTexture("resources/textures/title.png") };
        screens[Screens::Lose] = { resourceLoader.loadTexture("resources/textures/gameover.png") };
        screens[Screens::Win] = { resourceLoader.loadTexture("resources/textures/win.png") };
    }

    void runFrame(eng::SceneInterface& scene, eng::InputInterface& input, eng::AppInterface& app, eng::AudioInterface& audio, const double deltaTime) override
    {
        animationTimer += deltaTime;
        while (animationTimer >= 1.0 / animationFPS)
        {
            ++animationCounter;
            animationTimer -= 1.0 / animationFPS;
        }

        if (sceneRunner)
        {
            sceneRunner->runFrame(input, app, audio, deltaTime);
            if (sceneRunner->state == GameSceneRunner::State::Running)
            {
                sceneRunner->render(scene);
            }
            else if (sceneRunner->state == GameSceneRunner::State::Completed)
            {
                audio.destroyLoop(themeLoop);
                if (currentDungeon < numDungeons)
                {
                    if (currentDungeon == 1) themeLoop = audio.createLoop("resources/audio/loop1real.wav");
                    if (currentDungeon == 2) themeLoop = audio.createLoop("resources/audio/loop2real.wav");
                    sceneRunner.reset(new GameSceneRunner(*common, currentDungeon++));
                }
                else
                {
                    themeLoop = audio.createLoop("resources/audio/GasStationThemereal.wav");
                    currentDungeon = 0;
                    sceneRunner.reset();
                    currentScreen = Screens::Win;
                    animationCounter = 0;
                    animationTimer = 0;
                }
            }
            else if (sceneRunner->state == GameSceneRunner::State::GameOver)
            {
                audio.destroyLoop(themeLoop);
                themeLoop = audio.createLoop("resources/audio/GasStationThemereal.wav");
                currentDungeon = 0;
                sceneRunner.reset();
                currentScreen = Screens::Lose;
                animationCounter = 0;
                animationTimer = 0;
            }
        }
        else
        {
            bool pressed = std::reduce(anyActionInputs.begin(), anyActionInputs.end(), false,
                    [&](const bool state, const uint32_t mapping) { return state || input.getBoolean(mapping); });
            if (pressed && !lastPressed)
            {
                if (currentScreen == Screens::Title)
                {
                    sceneRunner.reset(new GameSceneRunner(*common, currentDungeon++));
                }
                else
                {
                    currentScreen = Screens::Title;
                    animationCounter = 0;
                    animationTimer = 0;
                }
            }
            lastPressed = pressed;

            const auto [framebufferWidth, framebufferHeight] = scene.framebufferSize();
            const float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
            scene.layers().resize(1);
            auto& layer = scene.layers().front();
            layer.spriteInstances.clear();
            layer.geometryInstances.clear();
            layer.overlaySpriteInstances.clear();
            layer.lights.clear();
            layer.decals.clear();
            layer.viewport = { .offset = { 0, framebufferHeight }, .extent = { framebufferWidth, -static_cast<float>(framebufferHeight) } };
            layer.scissor = { .extent = { framebufferWidth, framebufferHeight } };
            layer.projection = glm::orthoRH_ZO(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
            layer.view = glm::identity<glm::mat4>();
            layer.ambientLight = glm::vec3(1);

            layer.spriteInstances.push_back(eng::SpriteInstance {
                        .textureIndex = screens[currentScreen][animationCounter % screens[currentScreen].size()],
                    });
        }
    }

    void cleanup() override
    {
        sceneRunner.reset();
        common.reset();
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
