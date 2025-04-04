#include "swapchain.hpp"

using eng::Swapchain;

static vk::raii::SwapchainKHR createSwapchain(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surfaceFormat, const vk::Extent2D& extent, const vk::SwapchainKHR& oldSwapchain)
{
    const auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    uint32_t minImageCount = std::max(surfaceCapabilities.minImageCount, 4u);
    if (surfaceCapabilities.maxImageCount > 0)
    {
        minImageCount = std::min(minImageCount, surfaceCapabilities.maxImageCount);
    }

    return vk::raii::SwapchainKHR(device, vk::SwapchainCreateInfoKHR {
        .surface = surface,
        .minImageCount = minImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = vk::True,
        .oldSwapchain = oldSwapchain,
    });
}

Swapchain::Swapchain(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surfaceFormat, const vk::Extent2D& extent) :
    device(device),
    physicalDevice(physicalDevice),
    surface(surface),
    surfaceFormat(surfaceFormat),
    swapchain(createSwapchain(device, physicalDevice, surface, surfaceFormat, extent, vk::SwapchainKHR{})),
    images(swapchain.getImages()),
    extent(extent)
{
    imageViews.reserve(images.size());
    for (const auto& image : images)
    {
        imageViews.push_back(vk::raii::ImageView(device, vk::ImageViewCreateInfo {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = surfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        }));
    }
}

void Swapchain::recreate(const vk::Extent2D& extent)
{
    this->extent = extent;
    swapchain = createSwapchain(device, physicalDevice, surface, surfaceFormat, extent, swapchain);
    images = swapchain.getImages();
    imageViews.clear();
    imageViews.reserve(images.size());
    for (const auto& image : images)
    {
        imageViews.push_back(vk::raii::ImageView(device, vk::ImageViewCreateInfo {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = surfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        }));
    }
}
