#include "texture_loader.hpp"
#include "loader_utility.hpp"

using eng::TextureLoader;

TextureLoader::TextureLoader(const vk::raii::Device& device, const vma::Allocator& allocator, LoaderUtility& loaderUtility) :
    device(device),
    allocator(allocator),
    loaderUtility(loaderUtility)
{
}

eng::Texture TextureLoader::loadTexture(const char* bytes, const vk::DeviceSize size, const vk::Format format, const vk::Extent2D extent)

{
    auto& [stagingBuffer, stagingBufferAllocation, allocationInfo] = loaderUtility.createStagingBuffer(size);
    std::memcpy(allocationInfo.pMappedData, bytes, size);

    auto [image, allocation] = allocator.createImageUnique(vk::ImageCreateInfo {
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = vk::Extent3D{ extent.width, extent.height, 1 },
            .mipLevels = 1, /* TODO: mip mapping */
            .arrayLayers = 1,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .initialLayout = vk::ImageLayout::eUndefined,
        }, vma::AllocationCreateInfo {
            .usage = vma::MemoryUsage::eAuto,
        });

    const vk::ImageMemoryBarrier2 initialImageMemoryBarrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .image = *image,
        .subresourceRange = vk::ImageSubresourceRange {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount= 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    loaderUtility.commandBuffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &initialImageMemoryBarrier,
        });

    loaderUtility.commandBuffer.copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, vk::BufferImageCopy {
            .imageSubresource = vk::ImageSubresourceLayers {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageExtent = vk::Extent3D{ extent.width, extent.height, 1 },
        });

    const vk::ImageMemoryBarrier2 finalImageMemoryBarrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = *image,
        .subresourceRange = vk::ImageSubresourceRange {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount= 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    loaderUtility.commandBuffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &finalImageMemoryBarrier,
        });

    vk::raii::ImageView imageView(device, vk::ImageViewCreateInfo {
            .image = *image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        });

    return { std::move(image), std::move(allocation), std::move(imageView) };
}
