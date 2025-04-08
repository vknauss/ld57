#include "engine.hpp"
#include "config.h" // IWYU pragma: keep
#include "geometry_loader.hpp"
#include "input_manager.hpp"
#include "loader_utility.hpp"
#include "renderer.hpp"
#include "swapchain.hpp"
#include "texture_loader.hpp"
#include "vulkan_includes.hpp"
#include "util.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <stb_image.h>
#include <glm/glm.hpp>
#include <iostream>
#include <optional>
#include <queue>
#include <stdexcept>
#include <sstream>
#include <vector>

using namespace eng;

struct SDLWindowWrapper
{
    SDL_Window* window;

    explicit SDLWindowWrapper(SDL_Window* window)
        : window(window)
    {
    }

    SDLWindowWrapper(SDLWindowWrapper&& rh)
        : window(rh.window)
    {
        rh.window = nullptr;
    }

    SDLWindowWrapper(const SDLWindowWrapper&) = delete;

    ~SDLWindowWrapper()
    {
        SDL_DestroyWindow(window);
    }

    SDLWindowWrapper& operator=(SDLWindowWrapper&& rh)
    {
        window = rh.window;
        rh.window = nullptr;
        return *this;
    }

    SDLWindowWrapper& operator=(const SDLWindowWrapper&) = delete;

    operator SDL_Window*() const
    {
        return window;
    }

    vk::Extent2D getFramebufferExtent() const
    {
        int framebufferWidth, framebufferHeight;
        SDL_GetWindowSizeInPixels(window, &framebufferWidth, &framebufferHeight);
        return vk::Extent2D {
                static_cast<uint32_t>(framebufferWidth),
                static_cast<uint32_t>(framebufferHeight)
            };
    }
};

struct SDLLibraryWrapper
{
    SDLLibraryWrapper(const ApplicationInfo& applicationInfo)
    {
        const std::string appVersion = std::to_string(applicationInfo.appVersion);
        const std::string appID = std::string("rip.vxnt.eng.app.") + applicationInfo.appName;
        SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, applicationInfo.appName.c_str());
        SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, appVersion.c_str());
        SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, appID.c_str());

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
        {
            throw std::runtime_error((std::stringstream{} << "Failed to initialize SDL: " << SDL_GetError()).str());
        }

        if (!SDL_Vulkan_LoadLibrary(nullptr))
        {
            throw std::runtime_error((std::stringstream{} << "Failed to load Vulkan: " << SDL_GetError()).str());
        }
    }

    ~SDLLibraryWrapper()
    {
        SDL_Vulkan_UnloadLibrary();
        // SDL_Quit();
    }

    SDLWindowWrapper CreateWindow(const int width, const int height, const char* title) const
    {
        SDL_Window* window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window)
        {
            throw std::runtime_error((std::stringstream{} << "Failed to create SDL Vulkan window: " << SDL_GetError()).str());
        }
        return SDLWindowWrapper(window);
    }
};

struct SDLWindowSurfaceWrapper
{
    const vk::raii::Instance& instance;
    vk::SurfaceKHR surface;

    SDLWindowSurfaceWrapper(SDL_Window* window, const vk::raii::Instance& instance) :
        instance(instance)
    {
        VkSurfaceKHR handle;
        if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &handle))
        {
            throw std::runtime_error((std::stringstream{} << "Failed to create SDL window Vulkan surface: " << SDL_GetError()).str());
        }
        surface = vk::SurfaceKHR(handle);
    }

    ~SDLWindowSurfaceWrapper()
    {
        SDL_Vulkan_DestroySurface(*instance, surface, nullptr);
    }

    operator const vk::SurfaceKHR&() const
    {
        return surface;
    }
};

struct Audio final : eng::AudioInterface
{
    struct Sound
    {
        uint8_t* pcm = nullptr;
        uint32_t length = 0;
        SDL_AudioStream* stream = nullptr;

        Sound() = default;

        Sound(Sound&& s) :
            pcm(s.pcm),
            length(s.length),
            stream(s.stream)
        {
            s.pcm = nullptr;
            s.length = 0;
            s.stream = nullptr;
        }

        Sound(Sound&) = delete;

        ~Sound()
        {
            SDL_DestroyAudioStream(stream);
            SDL_free(pcm);
        }

        Sound& operator=(Sound&& s)
        {
            SDL_DestroyAudioStream(stream);
            SDL_free(pcm);
            pcm = s.pcm;
            length = s.length;
            stream = s.stream;
            s.pcm = nullptr;
            s.length = 0;
            s.stream = nullptr;
            return *this;
        }

        Sound& operator=(Sound& s) = delete;
    };

    SDL_AudioDeviceID device;
    std::vector<Sound> loops;
    std::vector<Sound> singleShot;
    std::queue<uint32_t> freeLoopIndices;
    std::queue<uint32_t> freeSingleShotIndices;

    Audio() :
        device(SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr))
    {
        if (!device)
        {
            throw std::runtime_error((std::stringstream{} << "failed to open audio playback device: " << SDL_GetError()).str());
        }
    }

    ~Audio()
    {
        SDL_CloseAudioDevice(device);
    }

    uint32_t createSound(const std::string& filePath, std::vector<Sound>& sounds, std::queue<uint32_t>& freeIndices)
    {
        Sound sound;
        SDL_AudioSpec audioSpec;
        if (!SDL_LoadWAV(filePath.c_str(), &audioSpec, &sound.pcm, &sound.length))
        {
            throw std::runtime_error((std::stringstream{} << "failed to load audio from path: " << filePath << ": " << SDL_GetError()).str());
        }

        sound.stream = SDL_CreateAudioStream(&audioSpec, nullptr);
        if (!sound.stream)
        {
            throw std::runtime_error((std::stringstream{} << "failed to create audio stream: " << SDL_GetError()).str());
        }

        if (!SDL_BindAudioStream(device, sound.stream))
        {
            throw std::runtime_error((std::stringstream{} << "failed to bind audio stream for playback: " << SDL_GetError()).str());
        }

        if (!SDL_PutAudioStreamData(sound.stream, sound.pcm, sound.length))
        {
            throw std::runtime_error((std::stringstream{} << "failed to send audio stream data: " << SDL_GetError()).str());
        }

        uint32_t index;
        if (freeIndices.empty())
        {
            index = sounds.size();
            sounds.push_back(std::move(sound));
        }
        else
        {
            index = freeIndices.front();
            freeIndices.pop();
            sounds[index] = std::move(sound);
        }
        return index;
    }

    void destroySound(uint32_t index, std::vector<Sound>& sounds, std::queue<uint32_t>& freeIndices)
    {
        if (index < sounds.size() && sounds[index].stream)
        {
            sounds[index] = Sound{};
            freeIndices.push(index);
        }
    }

    uint32_t createLoop(const std::string& filePath) override
    {
        return createSound(filePath, loops, freeLoopIndices);
    }

    void destroyLoop(uint32_t index) override
    {
        destroySound(index, loops, freeLoopIndices);
    }

    uint32_t createSingleShot(const std::string& filePath) override
    {
        return createSound(filePath, singleShot, freeSingleShotIndices);
    }

    void destroySingleShot(uint32_t index) override
    {
        destroySound(index, singleShot, freeSingleShotIndices);
    }

    void setMuted(const bool value) override
    {
        SDL_SetAudioDeviceGain(device, value ? 0 : 1);
    }

    void update()
    {
        for (const auto& sound : loops)
        {
            if (sound.stream)
            {
                if (SDL_GetAudioStreamQueued(sound.stream) < static_cast<int>(sound.length))
                {
                    if (!SDL_PutAudioStreamData(sound.stream, sound.pcm, sound.length))
                    {
                        throw std::runtime_error((std::stringstream{} << "failed to send audio stream data: " << SDL_GetError()).str());
                    }
                }
            }
        }
        for (uint32_t i = 0; i < singleShot.size(); ++i)
        {
            const auto& sound = singleShot[i];
            if (sound.stream)
            {
                if (SDL_GetAudioStreamQueued(sound.stream) == 0)
                {
                    destroySingleShot(i);
                }
            }
        }
    }
};

static auto createInstance(const vk::raii::Context& context, const ApplicationInfo& applicationInfo)
{
    uint32_t numRequiredInstanceExtensions;
    const char* const* requiredInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&numRequiredInstanceExtensions);
    if (!requiredInstanceExtensions)
    {
        throw std::runtime_error("Vulkan not supported for window surface creation");
    }
#ifdef USE_VALIDATION_LAYERS
    const std::array validationLayers = { "VK_LAYER_KHRONOS_validation" };
#endif
    std::vector<const char*> extensionNames(numRequiredInstanceExtensions);
    std::copy(requiredInstanceExtensions, requiredInstanceExtensions + numRequiredInstanceExtensions, extensionNames.begin());
#ifdef USE_PORTABILITY_EXTENSION
        extensionNames.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    const vk::ApplicationInfo instanceApplicationInfo {
        .pApplicationName = applicationInfo.appName.c_str(),
        .applicationVersion = applicationInfo.appVersion,
        .apiVersion = vk::ApiVersion13,
    };

    return vk::raii::Instance(context, vk::InstanceCreateInfo {
#ifdef USE_PORTABILITY_EXTENSION
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
#endif
            .pApplicationInfo = &instanceApplicationInfo,
#ifdef USE_VALIDATION_LAYERS
            .enabledLayerCount = validationLayers.size(),
            .ppEnabledLayerNames = validationLayers.data(),
#endif
            .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
            .ppEnabledExtensionNames = extensionNames.data(),
        });
}

static vk::raii::PhysicalDevice getPhysicalDevice(const vk::raii::Instance& instance)
{
    auto physicalDevices = instance.enumeratePhysicalDevices();
    if (!physicalDevices.empty())
    {
        std::cout << "Detected " << physicalDevices.size() << " physical devices:" << std::endl;
        for (const auto& physicalDevice : physicalDevices)
        {
            auto properties = physicalDevice.getProperties();
            std::cout << properties.deviceName << std::endl;
        }
        return physicalDevices.front();
    }
    throw std::runtime_error("No Vulkan devices found");
}

static uint32_t getQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::QueueFlags& flags)
{
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & flags)
        {
            return i;
        }
    }
    throw std::runtime_error("No suitable queue family found");
}

static vk::raii::Device createDevice(const vk::raii::PhysicalDevice& physicalDevice, uint32_t queueFamilyIndex)
{
    const float queuePriority = 1.0f;
    const vk::DeviceQueueCreateInfo queueCreateInfo {
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    auto extensionProperties = physicalDevice.enumerateDeviceExtensionProperties();
    for (const auto& properties : extensionProperties)
    {
        if (strcmp(properties.extensionName, "VK_KHR_portability_subset") == 0)
        {
            deviceExtensions.push_back(properties.extensionName);
        }
    }

    const auto physicalDeviceFeaturesChain = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
    const auto& physicalDeviceVulkan12Features = physicalDeviceFeaturesChain.get<vk::PhysicalDeviceVulkan12Features>();
    const bool bindlessSupported = (physicalDeviceVulkan12Features.shaderSampledImageArrayNonUniformIndexing && physicalDeviceVulkan12Features.runtimeDescriptorArray);

    const vk::StructureChain deviceCreateInfoChain {
        vk::DeviceCreateInfo {
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        },
        vk::PhysicalDeviceFeatures2 {
            .features = {
                .multiDrawIndirect = vk::True,
            },
        },
        vk::PhysicalDeviceVulkan12Features {
            .shaderSampledImageArrayNonUniformIndexing = bindlessSupported ? vk::True : vk::False,
            .descriptorBindingPartiallyBound = vk::True,
            .runtimeDescriptorArray = bindlessSupported ? vk::True : vk::False,
            .samplerFilterMinmax = vk::True,
            .timelineSemaphore = vk::True,
        },
        vk::PhysicalDeviceSynchronization2Features {
            .synchronization2 = vk::True,
        },
        vk::PhysicalDeviceDynamicRenderingFeatures {
            .dynamicRendering = vk::True,
        },
    };

    return vk::raii::Device(physicalDevice, deviceCreateInfoChain.get<vk::DeviceCreateInfo>());
}

static vk::SurfaceFormatKHR getSurfaceFormat(const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface)
{
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
    std::optional<vk::Format> format;
    for (const auto& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            if (surfaceFormat.format == vk::Format::eB8G8R8A8Srgb || surfaceFormat.format == vk::Format::eR8G8B8A8Srgb)
            {
                return surfaceFormat;
            }
            format = format.value_or(surfaceFormat.format);
        }
    }
    if (format)
    {
        return vk::SurfaceFormatKHR{ *format, vk::ColorSpaceKHR::eSrgbNonlinear };
    }
    throw std::runtime_error("No suitable surface format found");
}

static std::optional<vk::Format> findDepthFormat(const vk::raii::PhysicalDevice& physicalDevice)
{
    for (const vk::Format format : {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD16Unorm,
            vk::Format::eD16UnormS8Uint,
        })
    {
        if (physicalDevice.getFormatProperties(format).optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }
    return std::nullopt;
}

struct ResourceLoader final : ResourceLoaderInterface
{
    const vk::raii::Device& device;
    const vma::Allocator& allocator;
    TextureLoader& textureLoader;
    GeometryLoader& geometryLoader;
    std::vector<Texture>& textures;
    std::vector<RenderGeometry>& geometry;

    ResourceLoader(const vk::raii::Device& device, const vma::Allocator& allocator, TextureLoader& textureLoader, GeometryLoader& geometryLoader, std::vector<Texture>& textures, std::vector<RenderGeometry>& geometry):
        device(device),
        allocator(allocator),
        textureLoader(textureLoader),
        geometryLoader(geometryLoader),
        textures(textures),
        geometry(geometry)
    {
    }

    uint32_t loadTexture(const std::string& filePath, TextureInfo* textureInfo) override
    {
        int width, height, components;
        stbi_uc* textureData = stbi_load(filePath.c_str(), &width, &height, &components, 4);
        if (!textureData)
        {
            throw std::runtime_error("Failed to load texture: " + filePath);
        }

        uint32_t index = textures.size();
        textures.push_back(textureLoader.loadTexture(reinterpret_cast<const char*>(textureData),
                    width * height * 4,
                    vk::Format::eR8G8B8A8Srgb,
                    vk::Extent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) }));

        stbi_image_free(textureData);

        if (textureInfo)
        {
            *textureInfo = {
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
            };
        }

        return index;
    }

    uint32_t createGeometry(const GeometryDescription& description) override
    {
        uint32_t index = geometry.size();
        geometry.push_back(geometryLoader.createGeometry(description.positions, description.texCoords, description.normals, description.indices));
        return index;
    }
};

struct Scene final : public SceneInterface
{
    std::vector<SceneLayer>& layers() override
    {
        return layers_;
    }

    std::pair<uint32_t, uint32_t> framebufferSize() const override
    {
        return framebufferSize_;
    }

    std::vector<SceneLayer> layers_;
    std::pair<uint32_t, uint32_t> framebufferSize_;
};

struct AppInterfaceProvider final : public AppInterface
{
    SDL_Window* window;
    bool quitRequested = false;
    bool reloadRequested = false;

    explicit AppInterfaceProvider(SDL_Window* window) :
        window(window)
    {}

    void setWantsCursorLock(const bool value) override
    {
        // SDL_SetWindowRelativeMouseMode(window, value);
        SDL_SetWindowMouseGrab(window, value);
    }

    void setWantsFullscreen(const bool value) override
    {
        SDL_SetWindowFullscreen(window, value);
    }

    void requestQuit() override
    {
        quitRequested = true;
    }

    void requestReload() override
    {
        reloadRequested = true;
    }

    std::pair<uint32_t, uint32_t> getWindowSize() const override
    {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }
};

enum class FrameResult
{
    Continue, Quit, Reload
};

class Application
{
    std::unique_ptr<GameLogicInterface> gameLogic;
    const SDLLibraryWrapper sdlWrapper;
    const vk::raii::Context context;
    const vk::raii::Instance instance;
    const vk::raii::PhysicalDevice physicalDevice;
    const uint32_t queueFamilyIndex;
    const vk::raii::Device device;
    const vk::raii::Queue queue;
    const vma::UniqueAllocator allocator;
    const SDLWindowWrapper window;
    const SDLWindowSurfaceWrapper surface;
    const vk::SurfaceFormatKHR surfaceFormat;
    const vk::Format depthFormat;
    Audio audio;
    Swapchain swapchain;
    LoaderUtility loaderUtility;
    TextureLoader textureLoader;
    GeometryLoader geometryLoader;
    std::vector<Texture> textures;
    std::vector<RenderGeometry> geometry;
    ResourceLoader resourceLoader;
    Scene scene;
    InputManager inputManager;
    AppInterfaceProvider appInterface;
    InitShim<&GameLogicInterface::init> gameLogicInit;
    std::optional<std::pair<AllocatedBuffer, AllocatedBuffer>> geometryBuffers;
    InitShim<&LoaderUtility::commit> loaderUtilityCommit;
    const vk::Buffer geometryVertexBuffer;
    const vk::Buffer geometryIndexBuffer;
    Renderer renderer;
    InitShim<&LoaderUtility::finalize> loaderUtilityFinalize;
    double lastTime;

public:
    explicit Application(const ApplicationInfo& applicationInfo, GameLogicInterface* gameLogic) :
        gameLogic(gameLogic),
        sdlWrapper(applicationInfo),
        instance(createInstance(context, applicationInfo)),
        physicalDevice(getPhysicalDevice(instance)),
        queueFamilyIndex(getQueueFamilyIndex(physicalDevice, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)),
        device(createDevice(physicalDevice, queueFamilyIndex)),
        queue(device.getQueue(queueFamilyIndex, 0)),
        allocator(vma::createAllocatorUnique(vma::AllocatorCreateInfo {
                    .physicalDevice = *physicalDevice,
                    .device = *device,
                    .instance = *instance,
                    .vulkanApiVersion = vk::ApiVersion13,
                })),
        window(sdlWrapper.CreateWindow(applicationInfo.windowWidth, applicationInfo.windowHeight, applicationInfo.windowTitle.c_str())),
        surface(window, instance),
        surfaceFormat(getSurfaceFormat(physicalDevice, surface)),
        depthFormat(findDepthFormat(physicalDevice).value()),
        swapchain(device, physicalDevice, surface, surfaceFormat, window.getFramebufferExtent()),
        loaderUtility(device, queue, queueFamilyIndex, *allocator),
        textureLoader(device, *allocator, loaderUtility),
        geometryLoader(device, *allocator, loaderUtility),
        textures(),
        geometry(),
        resourceLoader(device, *allocator, textureLoader, geometryLoader, textures, geometry),
        appInterface(window),
        gameLogicInit(*gameLogic, resourceLoader, scene, inputManager, appInterface, audio),
        geometryBuffers(geometry.empty()
                ? std::nullopt
                : std::optional{ geometryLoader.createGeometryVertexAndIndexBuffers() }),
        loaderUtilityCommit(loaderUtility),
        geometryVertexBuffer(geometryBuffers ? *std::get<0>(geometryBuffers->first) : nullptr),
        geometryIndexBuffer(geometryBuffers ? *std::get<0>(geometryBuffers->second) : nullptr),
        renderer(device, queue, queueFamilyIndex, *allocator,
                textures, geometryVertexBuffer, geometryIndexBuffer, 3,
                surfaceFormat.format, depthFormat, window.getFramebufferExtent(),
                physicalDevice.getProperties().limits.minUniformBufferOffsetAlignment),
        loaderUtilityFinalize(loaderUtility),
        lastTime(SDL_GetTicksNS() * 1.e-9)
    {
        const vk::Extent2D framebufferExtent = window.getFramebufferExtent();
        scene.framebufferSize_ = { framebufferExtent.width, framebufferExtent.height };
    }

    ~Application()
    {
        gameLogic->cleanup();
        queue.waitIdle();
    }

    Application(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    SDL_AppResult HandleEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            return SDL_APP_SUCCESS;
        }
        switch (event.type)
        {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                inputManager.handleKey(event.key.key, event.key.scancode, event.key.down, event.key.mod);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                inputManager.handleMouseButton(event.button.button, event.button.down);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                inputManager.handleMouseMotion(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_OpenGamepad(event.gdevice.which);
                inputManager.handleGamepadConnection(event.gdevice.which, true);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                inputManager.handleGamepadConnection(event.gdevice.which, false);
                SDL_CloseGamepad(SDL_GetGamepadFromID(event.gdevice.which));
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                inputManager.handleGamepadAxisMotion(event.gaxis.which, event.gaxis.axis,
                        2 * (static_cast<float>(event.gaxis.value) - std::numeric_limits<int16_t>::min()) / std::numeric_limits<uint16_t>::max() - 1);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                inputManager.handleGamepadButton(event.gbutton.which, event.gbutton.button, event.gbutton.down);
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            {
                const vk::Extent2D framebufferExtent = window.getFramebufferExtent();
                swapchain.recreate(framebufferExtent);
                renderer.updateFramebufferExtent(framebufferExtent);
                scene.framebufferSize_ = { framebufferExtent.width, framebufferExtent.height };
                break;
            }
            default:
                break;
        }
        return SDL_APP_CONTINUE;
    }

    FrameResult RunFrame()
    {
        auto time = SDL_GetTicksNS() * 1.e-9;
        gameLogic->runFrame(scene, inputManager, appInterface, audio, time - lastTime);
        lastTime = time;

        audio.update();

        renderer.nextFrame();
        renderer.beginFrame();
        renderer.updateFrame(scene, geometry);

        try
        {
            renderer.drawFrame(swapchain, glm::vec2(scene.framebufferSize_.first, scene.framebufferSize_.second));
        }
        catch (vk::OutOfDateKHRError& e)
        {
            const vk::Extent2D framebufferExtent = window.getFramebufferExtent();
            swapchain.recreate(framebufferExtent);
            renderer.updateFramebufferExtent(framebufferExtent);
            scene.framebufferSize_ = { framebufferExtent.width, framebufferExtent.height };
        }
        inputManager.nextFrame();
        return appInterface.quitRequested ? FrameResult::Quit :
            appInterface.reloadRequested ? FrameResult::Reload :
            FrameResult::Continue;
    }
};

struct AppWrap { std::unique_ptr<Application> app; };

SDL_AppResult SDL_AppInit(void** appState, int argc, char** argv)
{
    try
    {
        *appState = new AppWrap;
        static_cast<AppWrap*>(*appState)->app.reset(new Application(EngineApp_GetApplicationInfo(), EngineApp_CreateGameLogic()));
        return SDL_APP_CONTINUE;
    }
    catch(std::exception& e)
    {
        SDL_Log("Failure in app initialization: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

SDL_AppResult SDL_AppEvent(void* appState, SDL_Event* event)
{
    try
    {
        return static_cast<AppWrap*>(appState)->app->HandleEvent(*event);
    }
    catch(std::exception& e)
    {
        SDL_Log("Failure in event handling: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

SDL_AppResult SDL_AppIterate(void* appState)
{
    try
    {
        SDL_AppResult appResult = SDL_APP_CONTINUE;
        auto result = static_cast<AppWrap*>(appState)->app->RunFrame();
        if (result == FrameResult::Quit)
        {
            appResult = SDL_APP_SUCCESS;
        }
        else if (result == FrameResult::Reload)
        {
            static_cast<AppWrap*>(appState)->app.reset(new Application(EngineApp_GetApplicationInfo(), EngineApp_CreateGameLogic()));
        }
        return appResult;
    }
    catch(std::exception& e)
    {
        SDL_Log("Failure in frame execution: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

void SDL_AppQuit(void* appState, SDL_AppResult result)
{
    delete static_cast<AppWrap*>(appState);
}

