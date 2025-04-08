#pragma once

#include "common_definitions.hpp"
#include <glm/glm.hpp>
#include <map>

namespace eng
{
    struct Swapchain;
    struct SceneInterface;

    struct Deletable
    {
        virtual ~Deletable() = default;
    };

    template<class A>
    struct Deleter: Deletable
    {
        explicit Deleter(A&& a) : a(std::move(a))
        {}

        Deleter(Deleter<A>&& d) = default;
        Deleter& operator=(Deleter<A>&& d) = default;
        
        A a;
    };

    struct FrameData
    {
        vk::raii::Fence inFlightFence;
        vk::raii::Semaphore imageAcquiredSemaphore;
        vk::raii::Semaphore renderFinishedSemaphore;
        vk::raii::CommandPool commandPool;
        vk::raii::CommandBuffers commandBuffers;
        vk::raii::DescriptorSets descriptorSets;
        AllocatedBuffer uniformBuffer;
        AllocatedBuffer spriteInstanceBuffer;
        AllocatedBuffer geometryInstanceBuffer;
        AllocatedBuffer lightsBuffer;
        AllocatedBuffer decalsBuffer;
        std::vector<AllocatedBuffer> drawIndirectBuffers;
        std::vector<std::unique_ptr<Deletable>> toDelete;
    };

    struct LayerDrawInfo
    {
        vk::Viewport viewport;
        vk::Rect2D scissor;
        uint32_t uniformBufferOffset;
        uint32_t spriteInstanceCount;
        uint32_t spriteFirstInstanceIndex;
        uint32_t geometryInstanceCount;
        uint32_t overlaySpriteInstanceCount;
        uint32_t overlaySpriteFirstInstanceIndex;
        uint32_t indirectBuffersCount;
        uint32_t firstIndirectBufferIndex;
        uint32_t decalsCount;
        uint32_t decalFirstInstanceIndex;
        uint32_t firstPointShadowPos;
        uint32_t pointShadowsCount;
    };

    struct GBuffer
    {
        GBuffer(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Format depthFormat, const vk::Extent2D& extent);

        void recreate(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Extent2D& extent);

        vk::Extent2D extent;
        vk::Format colorFormat;
        vk::Format normalFormat;
        vk::Format depthFormat;
        Texture colorTexture;
        Texture normalTexture;
        Texture depthTexture;
    };

    struct CubeMap
    {
        vma::UniqueImage image;
        vma::UniqueAllocation allocation;
        vk::raii::ImageView cubeImageView;
        std::array<vk::raii::ImageView, 6> faceImageViews;
    };

    struct Renderer
    {

        explicit Renderer(const vk::raii::Device& device, const vk::raii::Queue& queue, const uint32_t queueFamilyIndex, const vma::Allocator& allocator, const std::vector<Texture>& textures, const vk::Buffer geometryVertexBuffer, const vk::Buffer geometryIndexBuffer, const uint32_t numFramesInFlight, const vk::Format colorAttachmentFormat, const vk::Format depthAttachmentFormat, const vk::Extent2D& framebufferExtent, const uint32_t minUniformBufferOffsetAlignment);

        void beginFrame();
        void updateFrame(SceneInterface& scene, const std::vector<RenderGeometry>& geometry);
        void drawFrame(const Swapchain& swapchain, const glm::vec2& viewportExtent);
        void nextFrame();

        void updateFramebufferExtent(const vk::Extent2D& framebufferExtent);

        void renderLayerGBuffer(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData);
        void renderLayerSSAO(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData);
        void renderLayerShadowMap(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData);

        const vk::raii::Device& device;
        const vk::raii::Queue& queue;
        const vma::Allocator& allocator;
        GBuffer gBuffer;
        const vk::Buffer geometryVertexBuffer;
        const vk::Buffer geometryIndexBuffer;
        const AllocatedBuffer decalGeometryBuffer;
        const vk::raii::Sampler textureSampler;
        const vk::raii::DescriptorPool descriptorPool;
        const uint32_t uniformBufferAlignedSizeVertex;
        const uint32_t uniformBufferAlignedSizeFragment;
        vk::Extent2D ambientOcclusionTextureExtent;
        Texture ambientOcclusionTexture;
        std::vector<CubeMap> shadowCubeMaps;

        const std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts;

        struct {
            const vk::raii::PipelineLayout gBuffer;
            const vk::raii::PipelineLayout deferred;
            const vk::raii::PipelineLayout decal;
            const vk::raii::PipelineLayout ssao;
            const vk::raii::PipelineLayout shadowDepth;
        } pipelineLayouts;

        struct {
            const vk::raii::Pipeline sprite;
            const vk::raii::Pipeline geometry;
            const vk::raii::Pipeline spriteOverlay;
            const vk::raii::Pipeline deferred;
            const vk::raii::Pipeline decal;
            const vk::raii::Pipeline ssao;
            const vk::raii::Pipeline geometryDepth;
        } pipelines;

        struct {
            const vk::raii::DescriptorSet textureArray;
            const vk::raii::DescriptorSet gBuffer;
            const vk::raii::DescriptorSet ambientOcclusionTexture;
            const vk::raii::DescriptorSet shadowCubeMapArray;
        } descriptorSets;

        std::vector<FrameData> frameData;
        std::vector<LayerDrawInfo> layerDrawInfos;
        uint32_t frameIndex = 0;
        std::map<uint32_t, std::vector<uint32_t>> geometryInstances;
        std::vector<glm::vec3> pointShadowPositions;
    };
}
