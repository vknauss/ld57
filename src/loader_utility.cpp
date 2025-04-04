#include "loader_utility.hpp"

using eng::LoaderUtility;

LoaderUtility::LoaderUtility(const vk::raii::Device& device, const vk::raii::Queue& queue, const uint32_t queueFamilyIndex, const vma::Allocator& allocator) :
    device(device),
    queue(queue),
    allocator(allocator),
    commandPool(device, vk::CommandPoolCreateInfo {
                .queueFamilyIndex = queueFamilyIndex,
            }),
    commandBuffer(std::move(device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
                        .commandPool = commandPool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = 1,
                    }).front())),
    fence(device, vk::FenceCreateInfo {})
{
    commandBuffer.begin(vk::CommandBufferBeginInfo {
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            });
}

const eng::AllocatedBuffer& LoaderUtility::createStagingBuffer(const vk::DeviceSize size)
{
    vma::AllocationInfo allocationInfo;
    auto [buffer, allocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                    .size = size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc,
                }, vma::AllocationCreateInfo {
                    .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                    .usage = vma::MemoryUsage::eAuto,
                }, allocationInfo);

    return stagingBuffers.emplace_back(std::move(buffer), std::move(allocation), std::move(allocationInfo));
}

void LoaderUtility::commit()
{
    commandBuffer.end();
    queue.submit(vk::SubmitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
        }, fence);
}

void LoaderUtility::finalize()
{
    if (auto result = device.waitForFences(*fence, vk::True, std::numeric_limits<uint64_t>::max()); result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Unexpected return from waitForFences");
    }

    stagingBuffers.clear();
    device.resetFences(*fence);
    commandPool.reset();
}
