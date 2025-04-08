#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace eng
{
    struct SpriteInstance
    {
        glm::vec3 position = { 0, 0, 0 };
        glm::vec3 scale = { 1, 1, 1 };
        glm::vec2 minTexCoord = { 0, 0 };
        glm::vec2 texCoordScale = { 1, 1 };
        float angle = 0.0f;
        uint32_t textureIndex = 0;
        glm::vec4 tintColor = { 1, 1, 1, 1 };
    };

    struct GeometryInstance
    {
        glm::vec3 position = { 0, 0, 0 };
        glm::vec3 scale = { 1, 1, 1 };
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec2 texCoordOffset = { 0, 0 };
        uint32_t textureIndex = 0;
        glm::vec4 tintColor = { 1, 1, 1, 1 };
        uint32_t geometryIndex;
    };

    struct GeometryDescription
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::vec3> normals;
        std::vector<uint32_t> indices;
    };

    struct TextureInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct Light
    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 intensity = glm::vec3(1);
    };

    struct Decal
    {
        glm::vec3 position = { 0, 0, 0 };
        glm::vec3 scale = { 1, 1, 1 };
        glm::quat rotation = glm::identity<glm::quat>();
        uint32_t textureIndex = 0;
    };

    struct SceneLayer
    {
        glm::mat4 projection = glm::ortho<float>(-1, 1, -1, 1, -1, 1);
        glm::mat4 view = glm::identity<glm::mat4>();
        struct {
            glm::vec2 offset = glm::vec2(0);
            glm::vec2 extent = glm::vec2(1);
        } viewport;
        struct {
            glm::ivec2 offset = glm::ivec2(0);
            glm::uvec2 extent = glm::uvec2(1);
        } scissor;
        glm::vec3 ambientLight = glm::vec3(0.1);

        std::vector<SpriteInstance> spriteInstances;
        std::vector<GeometryInstance> geometryInstances;
        std::vector<SpriteInstance> overlaySpriteInstances;
        std::vector<Light> lights;
        std::vector<Decal> decals;
    };

    struct ResourceLoaderInterface
    {
        virtual uint32_t loadTexture(const std::string& filePath, TextureInfo* textureInfo = nullptr) = 0;
        virtual uint32_t createGeometry(const GeometryDescription& description) = 0;
    };

    struct SceneInterface
    {
        virtual std::vector<SceneLayer>& layers() = 0;
        virtual std::pair<uint32_t, uint32_t> framebufferSize() const = 0;
    };

    struct InputInterface
    {
        enum class CursorAxis
        {
            X,
            Y,
        };

        enum class BoolStateEvent
        {
            Down,
            Pressed,
            Released,
        };

        enum class RealStateEvent
        {
            Value,
            Delta,
            Threshold,
        };

        virtual uint32_t createMapping() = 0;

        // for convenience the following functions return the passed-in mapping handle
        virtual uint32_t mapKey(const uint32_t mapping, const int scancode, const BoolStateEvent event = BoolStateEvent::Down) = 0;
        virtual uint32_t mapMouseButton(const uint32_t mapping, const int button, const BoolStateEvent event = BoolStateEvent::Down) = 0;
        virtual uint32_t mapCursor(const uint32_t mapping, const CursorAxis axis, const RealStateEvent event = RealStateEvent::Value, const float param = 0) = 0;
        virtual uint32_t mapGamepadAxis(const uint32_t mapping, const int axis, const RealStateEvent = RealStateEvent::Value, const float param = 0) = 0;
        virtual uint32_t mapGamepadButton(const uint32_t mapping, const int button, const BoolStateEvent = BoolStateEvent::Down) = 0;
        virtual uint32_t mapAnyKey(const uint32_t mapping, const BoolStateEvent event = BoolStateEvent::Down) = 0;
        virtual uint32_t mapAnyMouseButton(const uint32_t mapping, const BoolStateEvent event = BoolStateEvent::Down) = 0;
        virtual uint32_t mapAnyGamepadButton(const uint32_t mapping, const BoolStateEvent event = BoolStateEvent::Down) = 0;

        virtual bool getBoolean(const uint32_t mapping) const = 0;
        virtual double getReal(const uint32_t mapping) const = 0;
    };

    struct AppInterface
    {
        virtual void setWantsCursorLock(const bool value) = 0;
        virtual void setWantsFullscreen(const bool value) = 0;
        virtual void requestQuit() = 0;
        virtual void requestReload() = 0;
        virtual std::pair<uint32_t, uint32_t> getWindowSize() const = 0;
    };

    struct AudioInterface
    {
        virtual uint32_t createLoop(const std::string& filePath) = 0;
        virtual void destroyLoop(uint32_t index) = 0;
        virtual uint32_t createSingleShot(const std::string& filePath) = 0;
        virtual void destroySingleShot(uint32_t index) = 0;
        virtual void setMuted(const bool value) = 0;
    };

    struct GameLogicInterface
    {
        virtual ~GameLogicInterface() = default;

        virtual void init(ResourceLoaderInterface& resourceLoader,
                SceneInterface& scene,
                InputInterface& input,
                AppInterface& app,
                AudioInterface& audio) = 0;
        virtual void runFrame(SceneInterface& scene,
                InputInterface& input,
                AppInterface& app,
                AudioInterface& audio,
                const double deltaTime) = 0;
        virtual void cleanup() = 0;
    };

    struct ApplicationInfo
    {
        std::string appName;
        uint32_t appVersion;
        std::string windowTitle;
        uint32_t windowWidth;
        uint32_t windowHeight;
    };
}

extern eng::ApplicationInfo EngineApp_GetApplicationInfo();
extern eng::GameLogicInterface* EngineApp_CreateGameLogic();
