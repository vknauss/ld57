#pragma once

#include "vulkan_includes.hpp"

namespace eng
{
    struct Swapchain
    {
        explicit Swapchain(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surfaceFormat, const vk::Extent2D& extent);

        void recreate(const vk::Extent2D& extent);

        const vk::raii::Device& device;
        const vk::raii::PhysicalDevice& physicalDevice;
        const vk::SurfaceKHR& surface;
        const vk::SurfaceFormatKHR surfaceFormat;
        vk::raii::SwapchainKHR swapchain;
        std::vector<vk::Image> images;
        std::vector<vk::raii::ImageView> imageViews;
        vk::Extent2D extent;
    };
}
