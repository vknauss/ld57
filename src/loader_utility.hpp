#pragma once

#include "common_definitions.hpp"

namespace eng
{
    struct LoaderUtility
    {
        explicit LoaderUtility(const vk::raii::Device& device, const vk::raii::Queue& queue, const uint32_t queueFamilyIndex, const vma::Allocator& allocator);

        const AllocatedBuffer& createStagingBuffer(const vk::DeviceSize size);
        void commit();
        void finalize();

        const vk::raii::Device& device;
        const vk::raii::Queue& queue;
        const vma::Allocator& allocator;
        const vk::raii::CommandPool commandPool;
        const vk::raii::CommandBuffer commandBuffer;
        const vk::raii::Fence fence;
        std::vector<AllocatedBuffer> stagingBuffers;
    };
}
