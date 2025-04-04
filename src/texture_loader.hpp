#pragma  once

#include "common_definitions.hpp"

namespace eng
{
    struct LoaderUtility;

    struct TextureLoader
    {
        explicit TextureLoader(const vk::raii::Device& device, const vma::Allocator& allocator, LoaderUtility& loaderUtility);

        Texture loadTexture(const char* bytes, const vk::DeviceSize size, const vk::Format format, const vk::Extent2D extent);

        const vk::raii::Device& device;
        const vma::Allocator& allocator;
        LoaderUtility& loaderUtility;
    };
}
