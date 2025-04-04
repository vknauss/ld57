#include "geometry_loader.hpp"
#include "loader_utility.hpp"
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

using eng::GeometryLoader;

constexpr uint32_t VERTEX_SIZE = 8 * sizeof(float);

GeometryLoader::GeometryLoader(const vk::raii::Device& device, const vma::Allocator& allocator, LoaderUtility& loaderUtility) :
    device(device),
    allocator(allocator),
    loaderUtility(loaderUtility)
{
}

eng::RenderGeometry GeometryLoader::createGeometry(const std::vector<glm::vec3>& positions, const std::vector<glm::vec2>& texCoords, const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices)
{
    if (positions.size() != texCoords.size())
    {
        throw std::runtime_error("count of vertex positions and tex coords must match");
    }
    if (positions.size() != normals.size())
    {
        throw std::runtime_error("count of vertex positions and normals must match");
    }
    if (positions.empty())
    {
        throw std::runtime_error("positions must not be empty");
    }
    if (indices.empty())
    {
        throw std::runtime_error("indices must not be empty");
    }

    {
        const vk::DeviceSize vertexDataSize = VERTEX_SIZE * positions.size();
        auto& [vertexStagingBuffer, vertexStagingAllocation, vertexAllocationInfo] = loaderUtility.createStagingBuffer(vertexDataSize);

        auto writePointer = static_cast<char*>(vertexAllocationInfo.pMappedData);
        for (uint32_t i = 0; i < positions.size(); ++i)
        {
            std::memcpy(writePointer, glm::value_ptr(positions[i]), 3 * sizeof(float));
            writePointer += 3 * sizeof(float);
            std::memcpy(writePointer, glm::value_ptr(texCoords[i]), 2 * sizeof(float));
            writePointer += 2 * sizeof(float);
            std::memcpy(writePointer, glm::value_ptr(normals[i]), 3 * sizeof(float));
            writePointer += 3 * sizeof(float);
        }

        vertexBufferCopies.push_back({vertexStagingBuffer.get(), vk::BufferCopy {
                    .dstOffset = vertexOffset * VERTEX_SIZE,
                    .size = vertexDataSize,
                }});
    }

    {
        const vk::DeviceSize indexDataSize = sizeof(uint32_t) * indices.size();
        auto& [indexStagingBuffer, indexStagingAllocation, indexAllocationInfo] = loaderUtility.createStagingBuffer(indexDataSize);
        std::memcpy(indexAllocationInfo.pMappedData, indices.data(), indices.size() * sizeof(uint32_t));

        indexBufferCopies.emplace_back(indexStagingBuffer.get(), vk::BufferCopy {
                    .dstOffset = indexOffset * sizeof(uint32_t),
                    .size = indexDataSize,
                });
    }

    const RenderGeometry geometry {
        .numIndices = static_cast<uint32_t>(indices.size()),
        .firstIndex = indexOffset,
        .vertexOffset = static_cast<int32_t>(vertexOffset),
    };

    vertexOffset += positions.size();
    indexOffset += indices.size();

    return geometry;
}

std::pair<eng::AllocatedBuffer, eng::AllocatedBuffer> GeometryLoader::createGeometryVertexAndIndexBuffers()
{
    vma::AllocationInfo vertexAllocationInfo;
    auto [vertexBuffer, vertexBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = vertexOffset * VERTEX_SIZE,
                .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            }, vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eAuto,
            }, vertexAllocationInfo);

    vma::AllocationInfo indexAllocationInfo;
    auto [indexBuffer, indexBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = indexOffset * sizeof(uint32_t),
                .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            }, vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eAuto,
            }, indexAllocationInfo);

    for (const auto& [stagingBuffer, bufferCopy] : vertexBufferCopies)
    {
        loaderUtility.commandBuffer.copyBuffer(stagingBuffer, vertexBuffer.get(), bufferCopy);
    }
    vertexBufferCopies.clear();

    for (const auto& [stagingBuffer, bufferCopy] : indexBufferCopies)
    {
        loaderUtility.commandBuffer.copyBuffer(stagingBuffer, indexBuffer.get(), bufferCopy);
    }
    indexBufferCopies.clear();

    return {
        AllocatedBuffer { std::move(vertexBuffer), std::move(vertexBufferAllocation), std::move(vertexAllocationInfo) },
        AllocatedBuffer { std::move(indexBuffer), std::move(indexBufferAllocation), std::move(indexAllocationInfo) },
    };
}
