#pragma once

#include "vulkan_includes.hpp"

namespace eng
{
    using Texture = std::tuple<vma::UniqueImage, vma::UniqueAllocation, vk::raii::ImageView>;
    using AllocatedBuffer = std::tuple<vma::UniqueBuffer, vma::UniqueAllocation, vma::AllocationInfo>;

    struct RenderGeometry
    {
        uint32_t numIndices;
        uint32_t firstIndex;
        int32_t vertexOffset;
    };
}
