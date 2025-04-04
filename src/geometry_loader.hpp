#pragma once

#include "common_definitions.hpp"
#include <glm/glm.hpp>

namespace eng
{
    struct LoaderUtility;

    struct GeometryLoader
    {
        GeometryLoader(const vk::raii::Device& device, const vma::Allocator& allocator, LoaderUtility& loaderUtility);

        RenderGeometry createGeometry(const std::vector<glm::vec3>& positions, const std::vector<glm::vec2>& texCoords, const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices);
        std::pair<AllocatedBuffer, AllocatedBuffer> createGeometryVertexAndIndexBuffers();

        const vk::raii::Device& device;
        const vma::Allocator& allocator;
        LoaderUtility& loaderUtility;

        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        std::vector<std::pair<vk::Buffer, vk::BufferCopy>> vertexBufferCopies;
        std::vector<std::pair<vk::Buffer, vk::BufferCopy>> indexBufferCopies;
    };
}
