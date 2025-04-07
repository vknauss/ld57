#include "renderer.hpp"
#include "engine.hpp"
#include "swapchain.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace eng;

constexpr uint32_t MAX_GEOMETRY = 2048;
constexpr uint32_t MAX_LAYERS = 24;

namespace UniformBlockSize
{
    constexpr uint32_t VertexShader = 3 * sizeof(glm::mat4);
    constexpr uint32_t FragmentShader = 2 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4);
    constexpr uint32_t Combined = VertexShader + FragmentShader;
}

constexpr vk::Extent2D ShadowMapSize { 1024, 1024 };
constexpr uint32_t MaxPointLightShadows = 16;

namespace DescriptorSetLayoutIDs
{
    enum DescriptorSetLayoutIDs
    {
        BindlessTextureArray,
        SceneUniformData,
        VertexInstanceData,
        GBuffer,
        SingleTexture,
        PointShadowMapArray,
        CountOfElements // LAST
    };
};

namespace FrameDataDescriptorSetIDs
{
    enum DescriptorSetIDs
    {
        SceneUniformData,
        SpriteInstanceBuffer,
        GeometryInstanceBuffer,
        DecalInstanceBuffer,
        CountOfElements // LAST
    };
}

template<typename DescriptorSetBindingsArrayType,
    decltype(std::declval<DescriptorSetBindingsArrayType>().size(), std::declval<DescriptorSetBindingsArrayType>().data(), 0) = 0>
static vk::raii::DescriptorSetLayout createDescriptorSetLayout(const vk::raii::Device& device, DescriptorSetBindingsArrayType&& bindings)
{
    return vk::raii::DescriptorSetLayout(device, vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        });
}

static vk::raii::DescriptorSetLayout createDescriptorSetLayout(const vk::raii::Device& device, const vk::DescriptorSetLayoutBinding& binding)
{
    return vk::raii::DescriptorSetLayout(device, vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 1,
            .pBindings = &binding,
        });
}

static std::vector<vk::raii::DescriptorSetLayout> createDescriptorSetLayouts(const vk::raii::Device& device, const uint32_t numBindlessTextures)
{
    std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.reserve(DescriptorSetLayoutIDs::CountOfElements);
    for (int i = 0; i < DescriptorSetLayoutIDs::CountOfElements; ++i)
    {
        switch (static_cast<DescriptorSetLayoutIDs::DescriptorSetLayoutIDs>(i))
        {
            case DescriptorSetLayoutIDs::BindlessTextureArray:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = numBindlessTextures,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            }));
                break;
            case DescriptorSetLayoutIDs::SceneUniformData:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, std::array {
                            vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                            },
                            vk::DescriptorSetLayoutBinding {
                                .binding = 1,
                                .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                            vk::DescriptorSetLayoutBinding {
                                .binding = 2,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                        }));
                break;
            case DescriptorSetLayoutIDs::VertexInstanceData:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                            }));
                break;
            case DescriptorSetLayoutIDs::GBuffer:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, std::array {
                            vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                            vk::DescriptorSetLayoutBinding {
                                .binding = 1,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                            vk::DescriptorSetLayoutBinding {
                                .binding = 2,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                        }));
                break;
            case DescriptorSetLayoutIDs::SingleTexture:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            }));
                break;
            case DescriptorSetLayoutIDs::PointShadowMapArray:
                descriptorSetLayouts.push_back(createDescriptorSetLayout(device, vk::DescriptorSetLayoutBinding {
                                .binding = 0,
                                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                .descriptorCount = MaxPointLightShadows,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            }));
                break;
            default:
                throw std::runtime_error("No initializer for descriptor set layout index: " + std::to_string(i));
        };
    }

    return descriptorSetLayouts;
}

static vk::raii::PipelineLayout createPipelineLayout(const vk::raii::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {}, const std::vector<vk::PushConstantRange>& pushConstantRanges = {})
{
    return vk::raii::PipelineLayout(device, vk::PipelineLayoutCreateInfo {
            .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
            .pPushConstantRanges = pushConstantRanges.data(),
        });
}

static vk::raii::ShaderModule loadShaderModule(const vk::raii::Device& device, const std::string& filePath)
{
    if (auto fileStream = std::ifstream(filePath, std::ios::binary))
    {
        std::vector<char> code { std::istreambuf_iterator<char>(fileStream), {} };
        return vk::raii::ShaderModule(device, vk::ShaderModuleCreateInfo {
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(code.data()),
            });
    }
    throw std::runtime_error("Failed to open file: " + filePath);
}

struct PipelineDescription
{
    vk::PipelineLayout layout;
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
    std::vector<vk::VertexInputBindingDescription> vertexBindings;
    vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
    std::vector<vk::Format> colorAttachmentFormats;
    vk::Format depthAttachmentFormat = vk::Format::eUndefined;
};

static constexpr std::vector<vk::VertexInputAttributeDescription> getGeometryVertexAttributes()
{
    return {
        vk::VertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 0,
        },
        vk::VertexInputAttributeDescription {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = 3 * sizeof(float),
        },
        vk::VertexInputAttributeDescription {
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 5 * sizeof(float),
        },
    };
}

static constexpr std::vector<vk::VertexInputBindingDescription> getGeometryVertexBindings()
{
    return {
        vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = 8 * sizeof(float),
            .inputRate = vk::VertexInputRate::eVertex,
        }
    };
}

static constexpr std::vector<vk::VertexInputAttributeDescription> getDecalVertexAttributes()
{
    return {
        vk::VertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 0,
        },
    };
}

static constexpr std::vector<vk::VertexInputBindingDescription> getDecalVertexBindings()
{
    return {
        vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = 3 * sizeof(float),
            .inputRate = vk::VertexInputRate::eVertex,
        }
    };
}

static vk::raii::Pipeline createPipeline(const vk::raii::Device& device, const PipelineDescription& description)
{
    auto vertexShaderModule = loadShaderModule(device, description.vertexShaderPath);
    const bool useFragmentShader = !description.fragmentShaderPath.empty();
    auto fragmentShaderModule = useFragmentShader ? loadShaderModule(device, description.fragmentShaderPath) : nullptr;
    const std::array stages = {
        vk::PipelineShaderStageCreateInfo {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertexShaderModule,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragmentShaderModule,
            .pName = "main",
        }
    };

    const vk::PipelineVertexInputStateCreateInfo vertexInputState {
        .vertexBindingDescriptionCount = static_cast<uint32_t>(description.vertexBindings.size()),
        .pVertexBindingDescriptions = description.vertexBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(description.vertexAttributes.size()),
        .pVertexAttributeDescriptions = description.vertexAttributes.data(),
    };

    const vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState {
        .topology = description.primitiveTopology,
    };

    const vk::PipelineViewportStateCreateInfo viewportState {
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const vk::PipelineRasterizationStateCreateInfo rasterizationState {
        .cullMode = vk::CullModeFlagBits::eNone,
        .lineWidth = 1.0f,
    };

    const vk::PipelineMultisampleStateCreateInfo multisampleState {
    };

    const vk::PipelineDepthStencilStateCreateInfo depthStencilState {
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
    };

   std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(description.colorAttachmentFormats.size(),
        vk::PipelineColorBlendAttachmentState {
            .blendEnable = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags,
        });

    const vk::PipelineColorBlendStateCreateInfo colorBlendState {
        .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
        .pAttachments = colorBlendAttachments.data(),
    };

    const std::array dynamicStates { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eDepthTestEnable, vk::DynamicState::eDepthWriteEnable };
    const vk::PipelineDynamicStateCreateInfo dynamicState {
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    const vk::PipelineRenderingCreateInfo renderingInfo {
        .colorAttachmentCount = static_cast<uint32_t>(description.colorAttachmentFormats.size()),
        .pColorAttachmentFormats = description.colorAttachmentFormats.data(),
        .depthAttachmentFormat = description.depthAttachmentFormat,
    };

    return vk::raii::Pipeline(device, nullptr, vk::GraphicsPipelineCreateInfo {
            .pNext = &renderingInfo,
            .stageCount = static_cast<uint32_t>(useFragmentShader ? stages.size() : stages.size() - 1),
            .pStages = stages.data(),
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &inputAssemblyState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizationState,
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &depthStencilState,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = &dynamicState,
            .layout = description.layout
        });
}

static vk::raii::DescriptorPool createDescriptorPool(const vk::raii::Device& device, const uint32_t numBindlessTextures, const uint32_t numFramesInFlight)
{
    const std::array poolSizes = {
        vk::DescriptorPoolSize { vk::DescriptorType::eUniformBufferDynamic, 2 * numFramesInFlight },
        vk::DescriptorPoolSize { vk::DescriptorType::eCombinedImageSampler, 4 + numBindlessTextures + MaxPointLightShadows },
        vk::DescriptorPoolSize { vk::DescriptorType::eStorageBuffer, 4 * numFramesInFlight },
    };

    return vk::raii::DescriptorPool(device, vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 4 + 4 * numFramesInFlight,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data(),
        });
}

static vk::raii::DescriptorSet createDescriptorSet(const vk::raii::Device& device, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout)
{
    return std::move(device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptorSetLayout,
            }).front());
}

static vk::raii::DescriptorSet createTextureDescriptorSet(const vk::raii::Device& device, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::Sampler& textureSampler, const std::vector<std::tuple<vma::UniqueImage, vma::UniqueAllocation, vk::raii::ImageView>>& textures)
{
    auto descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);

    std::vector<vk::DescriptorImageInfo> imageInfos;
    imageInfos.reserve(textures.size());
    for (auto&& [image, allocation, imageView] : textures)
    {
        imageInfos.push_back(vk::DescriptorImageInfo {
                .sampler = textureSampler,
                .imageView = imageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            });
    }

    device.updateDescriptorSets(vk::WriteDescriptorSet {
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = imageInfos.data(),
        }, {});

    return descriptorSet;
}

static void writeGBufferDescriptorSet(const vk::raii::Device& device, const vk::Sampler& textureSampler, const GBuffer& gBuffer, const vk::DescriptorSet descriptorSet)
{
    const std::array imageInfos {
        vk::DescriptorImageInfo {
            .sampler = textureSampler,
            .imageView = std::get<2>(gBuffer.colorTexture),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        },
        vk::DescriptorImageInfo {
            .sampler = textureSampler,
            .imageView = std::get<2>(gBuffer.normalTexture),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        },
        vk::DescriptorImageInfo {
            .sampler = textureSampler,
            .imageView = std::get<2>(gBuffer.depthTexture),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        },
    };

    device.updateDescriptorSets(vk::WriteDescriptorSet {
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = imageInfos.data(),
        }, {});
}

static vk::raii::DescriptorSet createGBufferDescriptorSet(const vk::raii::Device& device, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::Sampler& textureSampler, const GBuffer& gBuffer)
{
    auto descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);
    writeGBufferDescriptorSet(device, textureSampler, gBuffer, descriptorSet);
    return descriptorSet;
}

static void writeSingleTextureDescriptorSet(const vk::raii::Device& device, const vk::Sampler& textureSampler, const vk::ImageView& imageView, const vk::DescriptorSet& descriptorSet, const vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal)
{
    vk::DescriptorImageInfo imageInfo {
        .sampler = textureSampler,
        .imageView = imageView,
        .imageLayout = imageLayout,
    };

    device.updateDescriptorSets(vk::WriteDescriptorSet {
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo,
        }, {});
}

static vk::raii::DescriptorSet createSingleTextureDescriptorSet(const vk::raii::Device& device, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::Sampler& textureSampler, const vk::ImageView& imageView, const vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal)
{
    auto descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);
    writeSingleTextureDescriptorSet(device, textureSampler, imageView, descriptorSet, imageLayout);
    return descriptorSet;
}

static vk::raii::DescriptorSet createCubemapArrayDescriptorSet(const vk::raii::Device& device, const vk::DescriptorPool& descriptorPool, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::Sampler& textureSampler, const std::vector<CubeMap>& cubeMaps)
{
    auto descriptorSet = std::move(device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptorSetLayout,
            }).front());

    std::vector<vk::DescriptorImageInfo> imageInfos;
    imageInfos.reserve(cubeMaps.size());
    for (const auto& cubeMap : cubeMaps)
    {
        imageInfos.push_back(vk::DescriptorImageInfo {
                .sampler = textureSampler,
                .imageView = cubeMap.cubeImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            });
    }

    device.updateDescriptorSets(vk::WriteDescriptorSet {
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = imageInfos.data(),
        }, {});

    return descriptorSet;
}

static std::vector<FrameData> createFrameData(const vk::raii::Device& device, const uint32_t queueFamilyIndex, const vma::Allocator& allocator, const vk::raii::DescriptorPool& descriptorPool, const std::vector<vk::raii::DescriptorSetLayout>& descriptorSetLayouts, const uint32_t numFramesInFlight)
{
    std::vector<FrameData> frameData;
    frameData.reserve(numFramesInFlight);

    std::array<vk::DescriptorSetLayout, FrameDataDescriptorSetIDs::CountOfElements> layouts;
    for (int i = 0; i < FrameDataDescriptorSetIDs::CountOfElements; ++i)
    {
        switch (static_cast<FrameDataDescriptorSetIDs::DescriptorSetIDs>(i))
        {
            case FrameDataDescriptorSetIDs::SceneUniformData:
                layouts[i] = descriptorSetLayouts[DescriptorSetLayoutIDs::SceneUniformData];
                break;
            case FrameDataDescriptorSetIDs::SpriteInstanceBuffer:
            case FrameDataDescriptorSetIDs::GeometryInstanceBuffer:
            case FrameDataDescriptorSetIDs::DecalInstanceBuffer:
                layouts[i] = descriptorSetLayouts[DescriptorSetLayoutIDs::VertexInstanceData];
                break;
            default:
                throw std::runtime_error("No initializer for frame descriptor set index: " + std::to_string(i));
        }
    }

    for (uint32_t i = 0; i < numFramesInFlight; ++i)
    {
        vk::raii::CommandPool commandPool(device, vk::CommandPoolCreateInfo {
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = queueFamilyIndex,
            });

        vk::raii::CommandBuffers commandBuffers(device, vk::CommandBufferAllocateInfo {
                .commandPool = commandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            });

        vk::raii::DescriptorSets descriptorSets(device, vk::DescriptorSetAllocateInfo {
                .descriptorPool = descriptorPool,
                .descriptorSetCount = layouts.size(),
                .pSetLayouts = layouts.data(),
            });

        vma::AllocationInfo uniformBufferAllocationInfo;
        auto [uniformBuffer, uniformBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = MAX_LAYERS * UniformBlockSize::Combined,
                .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            }, vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                .usage = vma::MemoryUsage::eAuto,
            }, uniformBufferAllocationInfo);

        vma::AllocationInfo spriteInstanceBufferAllocationInfo;
        auto [spriteInstanceBuffer, spriteInstanceBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = 16777216,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            }, vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                .usage = vma::MemoryUsage::eAuto,
            }, spriteInstanceBufferAllocationInfo);

        vma::AllocationInfo geometryInstanceBufferAllocationInfo;
        auto [geometryInstanceBuffer, geometryInstanceBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = 16777216,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            }, vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                .usage = vma::MemoryUsage::eAuto,
            }, geometryInstanceBufferAllocationInfo);

        vma::AllocationInfo lightsBufferAllocationInfo;
        auto [lightsBuffer, lightsBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = 65536,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            }, vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                .usage = vma::MemoryUsage::eAuto,
            }, lightsBufferAllocationInfo);

        vma::AllocationInfo decalsBufferAllocationInfo;
        auto [decalsBuffer, decalsBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = 65536,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            }, vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                .usage = vma::MemoryUsage::eAuto,
            }, decalsBufferAllocationInfo);

        const std::array bufferInfos {
            vk::DescriptorBufferInfo {
                .buffer = *uniformBuffer,
                .range = UniformBlockSize::VertexShader,
            },
            vk::DescriptorBufferInfo {
                .buffer = *spriteInstanceBuffer,
                .range = vk::WholeSize,
            },
            vk::DescriptorBufferInfo {
                .buffer = *geometryInstanceBuffer,
                .range = vk::WholeSize,
            },
            vk::DescriptorBufferInfo {
                .buffer = *lightsBuffer,
                .range = vk::WholeSize,
            },
            vk::DescriptorBufferInfo {
                .buffer = *uniformBuffer,
                .range = UniformBlockSize::FragmentShader,
            },
            vk::DescriptorBufferInfo {
                .buffer = *decalsBuffer,
                .range = vk::WholeSize,
            },
        };

        device.updateDescriptorSets({
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                    .pBufferInfo = &bufferInfos[0],
                },
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::SpriteInstanceBuffer],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfos[1],
                },
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::GeometryInstanceBuffer],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfos[2],
                },
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfos[3],
                },
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                    .pBufferInfo = &bufferInfos[4],
                },
                vk::WriteDescriptorSet {
                    .dstSet = descriptorSets[FrameDataDescriptorSetIDs::DecalInstanceBuffer],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfos[5],
                },
            }, {});

        frameData.push_back(FrameData {
                .inFlightFence = vk::raii::Fence(device, vk::FenceCreateInfo {
                        .flags = vk::FenceCreateFlagBits::eSignaled,
                    }),
                .imageAcquiredSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo {}),
                .renderFinishedSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo {}),
                .commandPool = std::move(commandPool),
                .commandBuffers = std::move(commandBuffers),
                .descriptorSets = std::move(descriptorSets),
                .uniformBuffer = {
                    std::move(uniformBuffer),
                    std::move(uniformBufferAllocation),
                    std::move(uniformBufferAllocationInfo),
                },
                .spriteInstanceBuffer = {
                    std::move(spriteInstanceBuffer),
                    std::move(spriteInstanceBufferAllocation),
                    std::move(spriteInstanceBufferAllocationInfo),
                },
                .geometryInstanceBuffer = {
                    std::move(geometryInstanceBuffer),
                    std::move(geometryInstanceBufferAllocation),
                    std::move(geometryInstanceBufferAllocationInfo),
                },
                .lightsBuffer = {
                    std::move(lightsBuffer),
                    std::move(lightsBufferAllocation),
                    std::move(lightsBufferAllocationInfo),
                },
                .decalsBuffer = {
                    std::move(decalsBuffer),
                    std::move(decalsBufferAllocation),
                    std::move(decalsBufferAllocationInfo),
                },
            });
    }

    return frameData;
}

struct TextureCreateInfo
{
    vk::ImageType imageType = vk::ImageType::e2D;
    vk::Format format = vk::Format::eUndefined;
    vk::Extent3D extent;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    vk::ImageUsageFlags usage;
    vk::ImageViewType viewType = vk::ImageViewType::e2D;
    vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
};

static eng::Texture createTexture(const vk::raii::Device& device, const vma::Allocator& allocator, const TextureCreateInfo& textureCreateInfo)
{
    auto [ image, allocation ] = allocator.createImageUnique(vk::ImageCreateInfo {
                .imageType = textureCreateInfo.imageType,
                .format = textureCreateInfo.format,
                .extent = textureCreateInfo.extent,
                .mipLevels = textureCreateInfo.mipLevels,
                .arrayLayers = textureCreateInfo.arrayLayers,
                .usage = textureCreateInfo.usage,
            }, vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eAuto,
            });

    vk::raii::ImageView imageView(device, vk::ImageViewCreateInfo {
            .image = *image,
            .viewType = textureCreateInfo.viewType,
            .format = textureCreateInfo.format,
            .subresourceRange = {
                .aspectMask = textureCreateInfo.aspect,
                .baseMipLevel = 0,
                .levelCount = textureCreateInfo.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = textureCreateInfo.arrayLayers,
            },
        });

    return { std::move(image), std::move(allocation), std::move(imageView) };
}

static eng::Texture createTexture(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Extent3D& extent, const vk::Format format, const vk::ImageUsageFlags usage, const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor)
{
    return createTexture(device, allocator, TextureCreateInfo {
            .format = format,
            .extent = extent,
            .usage = usage,
            .aspect = aspect,
        });
}

static CubeMap createCubemapTexture(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Extent3D& extent, const vk::Format format, const vk::ImageUsageFlags usage, const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor)
{
    auto [ image, allocation ] = allocator.createImageUnique(vk::ImageCreateInfo {
                .flags = vk::ImageCreateFlagBits::eCubeCompatible,
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = extent,
                .mipLevels = 1,
                .arrayLayers = 6,
                .usage = usage,
            }, vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eAuto,
            });

    vk::Image imageHandle = *image; // get before move

    return {
        .image = std::move(image),
        .allocation = std::move(allocation),
        .cubeImageView { device, vk::ImageViewCreateInfo {
            .image = imageHandle,
            .viewType = vk::ImageViewType::eCube,
            .format = format,
            .subresourceRange = { aspect, 0, 1, 0, 6 }
        }},
        .faceImageViews {
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 0, 1 }
            }},
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 1, 1 }
            }},
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 2, 1 }
            }},
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 3, 1 }
            }},
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 4, 1 }
            }},
            vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                .image = imageHandle,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = { aspect, 0, 1, 5, 1 }
            }},
        },
    };
}

static std::vector<CubeMap> createCubeMaps(const vk::raii::Device& device, const vma::Allocator& allocator, const uint32_t count, const vk::Extent3D& extent, const vk::Format format, const vk::ImageUsageFlags usage, const vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor)
{
    std::vector<CubeMap> cubeMaps;
    cubeMaps.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        cubeMaps.push_back(createCubemapTexture(device, allocator, extent, format, usage, aspect));
    }
    return cubeMaps;
}

namespace
{
    template<typename T, typename Enable = void>
    struct WriteDataHelper
    {
        static void writeData(char*& writePointer, T&& value)
        {
            std::memcpy(writePointer, &value, sizeof(T));
            writePointer += sizeof(T);
        }
    };

    template<typename T>
    struct WriteDataHelper<T, decltype(glm::value_ptr(std::declval<T>()))>
    {
        static void writeData(char*& writePointer, T&& value)
        {
            std::memcpy(writePointer, glm::value_ptr(value), sizeof(T));
            writePointer += sizeof(T);
        }
    };
}

template<typename T> 
static void writeData(char*& writePointer, T&& value)
{
    WriteDataHelper<T>::writeData(writePointer, std::forward<T>(value));
}

static AllocatedBuffer createDecalGeometryBuffer(const vk::raii::Device& device, const vk::raii::Queue& queue, const uint32_t queueFamilyIndex, const vma::Allocator& allocator)
{
    constexpr vk::DeviceSize size = 108 * sizeof(float); // 12 tris, 9 floats per tri
    vma::AllocationInfo allocationInfo;
    auto [ buffer, allocation ] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = size,
                .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            },
            vma::AllocationCreateInfo {
                .usage = vma::MemoryUsage::eAuto,
            },
            allocationInfo);

    vma::AllocationInfo stagingAllocationInfo;
    auto [ stagingBuffer, stagingAllocation ] = allocator.createBufferUnique(vk::BufferCreateInfo {
                .size = size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
            },
            vma::AllocationCreateInfo {
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
            },
            stagingAllocationInfo);

    char* writePointer = static_cast<char*>(stagingAllocationInfo.pMappedData);
    for (int axis = 0; axis < 3; ++axis)
    {
        glm::mat3 basis(0);
        basis[0][(axis + 1) % 3] = 1;
        basis[1][(axis + 2) % 3] = 1;
        basis[2][axis] = 1;
        for (int direction = 0; direction < 2; ++direction)
        {
            writeData(writePointer, 0.5f * basis * glm::vec3(-1, -1, 1));
            writeData(writePointer, 0.5f * basis * glm::vec3(1, -1, 1));
            writeData(writePointer, 0.5f * basis * glm::vec3(-1, 1, 1));
            writeData(writePointer, 0.5f * basis * glm::vec3(-1, 1, 1));
            writeData(writePointer, 0.5f * basis * glm::vec3(1, -1, 1));
            writeData(writePointer, 0.5f * basis * glm::vec3(1, 1, 1));
            basis = -basis;
        }
    }

    vk::raii::CommandPool commandPool(device, vk::CommandPoolCreateInfo {
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = queueFamilyIndex,
            });

    vk::raii::Fence fence(device, vk::FenceCreateInfo {});

    auto commandBuffer = std::move(device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
                    .commandPool = commandPool,
                    .level = vk::CommandBufferLevel::ePrimary,
                    .commandBufferCount = 1,
                }).front());

    commandBuffer.begin(vk::CommandBufferBeginInfo {
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            });

    commandBuffer.copyBuffer(*stagingBuffer, *buffer, vk::BufferCopy { .size = size });

    vk::BufferMemoryBarrier2KHR bufferMemoryBarrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eVertexInput,
        .dstAccessMask = vk::AccessFlagBits2::eVertexAttributeRead,
        .buffer = *buffer,
        .offset = 0,
        .size = size,
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferMemoryBarrier,
    });

    commandBuffer.end();

    const vk::CommandBufferSubmitInfo commandBufferSubmitInfo {
        .commandBuffer = commandBuffer,
    };

    queue.submit2(vk::SubmitInfo2 {
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &commandBufferSubmitInfo,
            }, fence);

    if (auto result = device.waitForFences(*fence, vk::True, std::numeric_limits<uint64_t>::max());
            result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Unexpected return from vkWaitForFences");
    }

    commandPool.reset();

    return { std::move(buffer), std::move(allocation), std::move(allocationInfo) };
}

GBuffer::GBuffer(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Format depthFormat, const vk::Extent2D& extent) :
    extent(extent),
    colorFormat(vk::Format::eR8G8B8A8Unorm),
    normalFormat(vk::Format::eR16G16B16A16Sfloat),
    depthFormat(depthFormat),
    colorTexture(createTexture(device, allocator, vk::Extent3D{ extent.width, extent.height, 1 }, colorFormat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)),
    normalTexture(createTexture(device, allocator, vk::Extent3D{ extent.width, extent.height, 1 }, normalFormat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)),
    depthTexture(createTexture(device, allocator, TextureCreateInfo {
                .format = depthFormat,
                .extent = vk::Extent3D{ extent.width, extent.height, 1 },
                .mipLevels =1,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
                .aspect = vk::ImageAspectFlagBits::eDepth,
            }))
{
}

void GBuffer::recreate(const vk::raii::Device& device, const vma::Allocator& allocator, const vk::Extent2D& extent)
{
    this->extent = extent;
    colorTexture = createTexture(device, allocator, vk::Extent3D{ extent.width, extent.height, 1 }, colorFormat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
    normalTexture = createTexture(device, allocator, vk::Extent3D{ extent.width, extent.height, 1 }, normalFormat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
    depthTexture = createTexture(device, allocator, TextureCreateInfo {
            .format = depthFormat,
            .extent = vk::Extent3D{ extent.width, extent.height, 1 },
            .mipLevels = 1,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
            .aspect = vk::ImageAspectFlagBits::eDepth,
        });
}

Renderer::Renderer(const vk::raii::Device& device, const vk::raii::Queue& queue, const uint32_t queueFamilyIndex, const vma::Allocator& allocator, const std::vector<Texture>& textures, const vk::Buffer geometryVertexBuffer, const vk::Buffer geometryIndexBuffer, const uint32_t numFramesInFlight, const vk::Format colorAttachmentFormat, const vk::Format depthAttachmentFormat, const vk::Extent2D& framebufferExtent) :
    device(device),
    queue(queue),
    allocator(allocator),
    gBuffer(device, allocator, depthAttachmentFormat, framebufferExtent),
    geometryVertexBuffer(geometryVertexBuffer),
    geometryIndexBuffer(geometryIndexBuffer),
    decalGeometryBuffer(createDecalGeometryBuffer(device, queue, queueFamilyIndex, allocator)),
    textureSampler(device, vk::SamplerCreateInfo {}),
    descriptorPool(createDescriptorPool(device, textures.size(), numFramesInFlight)),
    ambientOcclusionTextureExtent(framebufferExtent.width / 4, framebufferExtent.height / 4),
    ambientOcclusionTexture(createTexture(device, allocator,
                { ambientOcclusionTextureExtent.width, ambientOcclusionTextureExtent.height, 1},
                vk::Format::eR16Sfloat,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)),
    shadowCubeMaps(createCubeMaps(device, allocator,
                MaxPointLightShadows,
                { ShadowMapSize.width, ShadowMapSize.height, 1 },
                depthAttachmentFormat,
                vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlagBits::eDepth)),
    descriptorSetLayouts(createDescriptorSetLayouts(device, textures.size())),
    pipelineLayouts {
        .gBuffer = createPipelineLayout(device, {
                descriptorSetLayouts[DescriptorSetLayoutIDs::BindlessTextureArray],
                descriptorSetLayouts[DescriptorSetLayoutIDs::SceneUniformData],
                descriptorSetLayouts[DescriptorSetLayoutIDs::VertexInstanceData],
            }),
        .deferred = createPipelineLayout(device, {
                descriptorSetLayouts[DescriptorSetLayoutIDs::GBuffer],
                descriptorSetLayouts[DescriptorSetLayoutIDs::SceneUniformData],
                descriptorSetLayouts[DescriptorSetLayoutIDs::SingleTexture],
                descriptorSetLayouts[DescriptorSetLayoutIDs::PointShadowMapArray],
            }),
        .decal = createPipelineLayout(device, {
                descriptorSetLayouts[DescriptorSetLayoutIDs::GBuffer],
                descriptorSetLayouts[DescriptorSetLayoutIDs::SceneUniformData],
                descriptorSetLayouts[DescriptorSetLayoutIDs::VertexInstanceData],
                descriptorSetLayouts[DescriptorSetLayoutIDs::BindlessTextureArray],
            }),
        .ssao = createPipelineLayout(device, {
                descriptorSetLayouts[DescriptorSetLayoutIDs::GBuffer],
                descriptorSetLayouts[DescriptorSetLayoutIDs::SceneUniformData],
            }),
        .shadowDepth = createPipelineLayout(device, {
                descriptorSetLayouts[DescriptorSetLayoutIDs::VertexInstanceData]
            }, {
                vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4) }
            }),
    },
    pipelines {
        .sprite = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.gBuffer,
                .vertexShaderPath = "shaders/sprite.vs.spv",
                .fragmentShaderPath = "shaders/g_buffer.fs.spv",
                .primitiveTopology = vk::PrimitiveTopology::eTriangleStrip,
                .colorAttachmentFormats = { gBuffer.colorFormat, gBuffer.normalFormat },
                .depthAttachmentFormat = gBuffer.depthFormat,
            }),
        .geometry = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.gBuffer,
                .vertexShaderPath = "shaders/geometry.vs.spv",
                .fragmentShaderPath = "shaders/g_buffer.fs.spv",
                .vertexAttributes = getGeometryVertexAttributes(),
                .vertexBindings = getGeometryVertexBindings(),
                .colorAttachmentFormats = { gBuffer.colorFormat, gBuffer.normalFormat },
                .depthAttachmentFormat = gBuffer.depthFormat,
            }),
        .spriteOverlay = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.gBuffer,
                .vertexShaderPath = "shaders/sprite_overlay.vs.spv",
                .fragmentShaderPath = "shaders/g_buffer.fs.spv",
                .primitiveTopology = vk::PrimitiveTopology::eTriangleStrip,
                .colorAttachmentFormats = { gBuffer.colorFormat, gBuffer.normalFormat },
                .depthAttachmentFormat = gBuffer.depthFormat,
            }),
        .deferred = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.deferred,
                .vertexShaderPath = "shaders/fullscreen.vs.spv",
                .fragmentShaderPath = "shaders/deferred.fs.spv",
                .colorAttachmentFormats = { colorAttachmentFormat },
                .depthAttachmentFormat = vk::Format::eUndefined,
            }),
        .decal = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.decal,
                .vertexShaderPath = "shaders/decal.vs.spv",
                .fragmentShaderPath = "shaders/decal.fs.spv",
                .vertexAttributes = getDecalVertexAttributes(),
                .vertexBindings = getDecalVertexBindings(),
                .colorAttachmentFormats = { gBuffer.colorFormat },
                .depthAttachmentFormat = vk::Format::eUndefined,
            }),
        .ssao = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.ssao,
                .vertexShaderPath = "shaders/fullscreen.vs.spv",
                .fragmentShaderPath = "shaders/ssao.fs.spv",
                .colorAttachmentFormats = { vk::Format::eR16Sfloat },
                .depthAttachmentFormat = vk::Format::eUndefined,
            }),
        .geometryDepth = createPipeline(device, PipelineDescription {
                .layout = pipelineLayouts.shadowDepth,
                .vertexShaderPath = "shaders/geometry_depth.vs.spv",
                .vertexAttributes = getGeometryVertexAttributes(),
                .vertexBindings = getGeometryVertexBindings(),
                .depthAttachmentFormat = depthAttachmentFormat,
            }),
    },
    descriptorSets {
        .textureArray = createTextureDescriptorSet(device, descriptorPool,
                descriptorSetLayouts[DescriptorSetLayoutIDs::BindlessTextureArray],
                textureSampler,
                textures),
        .gBuffer = createGBufferDescriptorSet(device, descriptorPool,
                descriptorSetLayouts[DescriptorSetLayoutIDs::GBuffer],
                textureSampler,
                gBuffer),
        .ambientOcclusionTexture = createSingleTextureDescriptorSet(device, descriptorPool,
                descriptorSetLayouts[DescriptorSetLayoutIDs::SingleTexture],
                textureSampler,
                std::get<2>(ambientOcclusionTexture)),
        .shadowCubeMapArray = createCubemapArrayDescriptorSet(device, descriptorPool,
                descriptorSetLayouts[DescriptorSetLayoutIDs::PointShadowMapArray],
                textureSampler,
                shadowCubeMaps),
    },
    frameData(createFrameData(device, queueFamilyIndex, allocator, descriptorPool, descriptorSetLayouts, numFramesInFlight))
{
}

void Renderer::beginFrame()
{
    if (auto result = device.waitForFences(*frameData[frameIndex].inFlightFence, vk::True, std::numeric_limits<uint64_t>::max()); result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Unexpected return from waitForFences");
    }
    device.resetFences({ frameData[frameIndex].inFlightFence });

    frameData[frameIndex].commandPool.reset();
    frameData[frameIndex].toDelete.clear();
}

void Renderer::updateFrame(SceneInterface& scene, const std::vector<RenderGeometry>& renderGeometry)
{
    uint32_t uniformBufferOffset = 0;
    uint32_t spriteInstanceIndex = 0;
    uint32_t geometryInstanceIndex = 0;
    uint32_t indirectBufferIndex = 0;
    uint32_t lightOffset = 0;
    uint32_t decalInstanceIndex = 0;

    auto uniformBufferWritePointer = static_cast<char*>(std::get<2>(frameData[frameIndex].uniformBuffer).pMappedData);
    auto spriteInstanceWritePointer = static_cast<char*>(std::get<2>(frameData[frameIndex].spriteInstanceBuffer).pMappedData);
    auto geometryInstanceWritePointer = static_cast<char*>(std::get<2>(frameData[frameIndex].geometryInstanceBuffer).pMappedData);
    auto lightsWritePointer = static_cast<char*>(std::get<2>(frameData[frameIndex].lightsBuffer).pMappedData);
    auto decalWritePointer = static_cast<char*>(std::get<2>(frameData[frameIndex].decalsBuffer).pMappedData);

    layerDrawInfos.clear();
    pointShadowPositions.clear();
    for (const auto& sceneLayer : scene.layers())
    {
        const uint32_t requiredIndirectBuffers = (sceneLayer.geometryInstances.size() + MAX_GEOMETRY - 1) / MAX_GEOMETRY;

        layerDrawInfos.push_back(LayerDrawInfo {
                .viewport = vk::Viewport {
                    .x = sceneLayer.viewport.offset.x,
                    .y = sceneLayer.viewport.offset.y,
                    .width = sceneLayer.viewport.extent.x,
                    .height = sceneLayer.viewport.extent.y,
                    .minDepth = 0,
                    .maxDepth = 1,
                },
                .scissor = vk::Rect2D {
                    .offset = { sceneLayer.scissor.offset.x, sceneLayer.scissor.offset.y },
                    .extent = { sceneLayer.scissor.extent.x, sceneLayer.scissor.extent.y },
                },
                .uniformBufferOffset = uniformBufferOffset,
                .spriteInstanceCount = static_cast<uint32_t>(sceneLayer.spriteInstances.size()),
                .spriteFirstInstanceIndex = spriteInstanceIndex,
                .geometryInstanceCount = static_cast<uint32_t>(sceneLayer.geometryInstances.size()),
                .overlaySpriteInstanceCount = static_cast<uint32_t>(sceneLayer.overlaySpriteInstances.size()),
                .overlaySpriteFirstInstanceIndex = spriteInstanceIndex + static_cast<uint32_t>(sceneLayer.spriteInstances.size()),
                .indirectBuffersCount = requiredIndirectBuffers,
                .firstIndirectBufferIndex = indirectBufferIndex,
                .decalsCount = static_cast<uint32_t>(sceneLayer.decals.size()),
                .decalFirstInstanceIndex = decalInstanceIndex,
                .firstPointShadowPos = static_cast<uint32_t>(pointShadowPositions.size()),
                .pointShadowsCount = static_cast<uint32_t>(std::min<size_t>(pointShadowPositions.size() + sceneLayer.lights.size(), MaxPointLightShadows) - pointShadowPositions.size()),
            });

        writeData(uniformBufferWritePointer, sceneLayer.projection);
        writeData(uniformBufferWritePointer, sceneLayer.view);
        const float aspectRatio = sceneLayer.viewport.extent.x / sceneLayer.viewport.extent.y;
        writeData(uniformBufferWritePointer, glm::orthoRH_ZO<float>(-aspectRatio, aspectRatio, -1, 1, 0, 1));

        writeData(uniformBufferWritePointer, glm::inverse(sceneLayer.projection));
        writeData(uniformBufferWritePointer, sceneLayer.projection);
        writeData(uniformBufferWritePointer, sceneLayer.ambientLight);
        writeData(uniformBufferWritePointer, lightOffset);
        writeData(uniformBufferWritePointer, static_cast<uint32_t>(sceneLayer.lights.size()));
        writeData<float>(uniformBufferWritePointer, 0);
        writeData(uniformBufferWritePointer, gBuffer.extent.width);
        writeData(uniformBufferWritePointer, gBuffer.extent.height);

        const std::array spriteInstanceVectors { &sceneLayer.spriteInstances, &sceneLayer.overlaySpriteInstances };
        for (const auto& pInstanceVector : spriteInstanceVectors)
        {
            for (const auto& instance : *pInstanceVector)
            {
                writeData(spriteInstanceWritePointer, instance.position);
                writeData(spriteInstanceWritePointer, 0.0f); // padding
                writeData(spriteInstanceWritePointer, instance.scale);
                writeData(spriteInstanceWritePointer, 0.0f); // padding
                writeData(spriteInstanceWritePointer, instance.minTexCoord);
                writeData(spriteInstanceWritePointer, instance.texCoordScale);
                writeData(spriteInstanceWritePointer, glm::vec2(glm::cos(instance.angle), glm::sin(instance.angle)));
                writeData(spriteInstanceWritePointer, instance.textureIndex);
                writeData(spriteInstanceWritePointer, 0.0f); // padding
                writeData(spriteInstanceWritePointer, instance.tintColor);
            }
        }

        const uint32_t nextIndirectBuffersInUse = indirectBufferIndex + requiredIndirectBuffers;
        if (nextIndirectBuffersInUse > frameData[frameIndex].drawIndirectBuffers.size())
        {
            const auto count = nextIndirectBuffersInUse - frameData[frameIndex].drawIndirectBuffers.size();
            frameData[frameIndex].drawIndirectBuffers.reserve(nextIndirectBuffersInUse);
            for (uint32_t i = 0; i < count; ++i)
            {
                vma::AllocationInfo indirectCommandsBufferAllocationInfo;
                auto [indirectCommandsBuffer, indirectCommandsBufferAllocation] = allocator.createBufferUnique(vk::BufferCreateInfo {
                        .size = sizeof(vk::DrawIndexedIndirectCommand) * MAX_GEOMETRY,
                        .usage = vk::BufferUsageFlagBits::eIndirectBuffer,
                    }, vma::AllocationCreateInfo {
                        .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                        .usage = vma::MemoryUsage::eAuto,
                    }, indirectCommandsBufferAllocationInfo);

                frameData[frameIndex].drawIndirectBuffers.emplace_back(std::move(indirectCommandsBuffer),
                        std::move(indirectCommandsBufferAllocation),
                        std::move(indirectCommandsBufferAllocationInfo));
            }
        }

        for (uint32_t i = 0; i < requiredIndirectBuffers; ++i)
        {
            const uint32_t bufferIndex = indirectBufferIndex + i;
            auto indirectCommandsWritePointer = static_cast<vk::DrawIndexedIndirectCommand*>(std::get<2>(frameData[frameIndex].drawIndirectBuffers.at(bufferIndex)).pMappedData);
            for (uint32_t j = i * MAX_GEOMETRY; j < (i + 1) * MAX_GEOMETRY && j < sceneLayer.geometryInstances.size(); ++j)
            {
                const auto& instance = sceneLayer.geometryInstances[j];
                if (instance.geometryIndex >= renderGeometry.size())
                {
                    throw std::runtime_error("geometry index out of bounds");
                }

                const auto& geometry = renderGeometry[instance.geometryIndex];
                *indirectCommandsWritePointer = vk::DrawIndexedIndirectCommand {
                    .indexCount = geometry.numIndices,
                    .instanceCount = 1,
                    .firstIndex = geometry.firstIndex,
                    .vertexOffset = geometry.vertexOffset,
                    .firstInstance = geometryInstanceIndex + j,
                };
                ++indirectCommandsWritePointer;
            }
        }

        for (const auto& instance : sceneLayer.geometryInstances)
        {
            const auto model = glm::scale(glm::translate(glm::mat4(1), instance.position) * glm::mat4_cast(instance.rotation), instance.scale);
            writeData(geometryInstanceWritePointer, sceneLayer.view * model);
            writeData(geometryInstanceWritePointer, instance.texCoordOffset);
            writeData(geometryInstanceWritePointer, instance.textureIndex);
            writeData(geometryInstanceWritePointer, 0.0f);
            writeData(geometryInstanceWritePointer, instance.tintColor);
        }

        for (const auto& light : sceneLayer.lights)
        {
            glm::vec4 viewPos = sceneLayer.view * glm::vec4(light.position, 1);
            writeData(lightsWritePointer, viewPos);
            writeData(lightsWritePointer, light.intensity);
            if (pointShadowPositions.size() < MaxPointLightShadows)
            {
                writeData<int>(lightsWritePointer, pointShadowPositions.size());
                pointShadowPositions.push_back(glm::vec3(viewPos));
            }
            else
            {
                writeData<int>(lightsWritePointer, -1);
            }
        }

        for (const auto& decal : sceneLayer.decals)
        {
            // this is probably kind of expensive. if it seems like perf is an issue probably can avoid recalc here
            const auto model = glm::scale(glm::translate(glm::mat4(1), decal.position) * glm::mat4_cast(decal.rotation), decal.scale);
            const auto modelView = sceneLayer.view * model;
            writeData(decalWritePointer, modelView);
            writeData(decalWritePointer, glm::inverse(modelView));
            writeData(decalWritePointer, decal.textureIndex);
            writeData(decalWritePointer, glm::vec3(0));
        }

        uniformBufferOffset += UniformBlockSize::Combined;
        spriteInstanceIndex += sceneLayer.spriteInstances.size() + sceneLayer.overlaySpriteInstances.size();
        geometryInstanceIndex += sceneLayer.geometryInstances.size();
        indirectBufferIndex += requiredIndirectBuffers;
        lightOffset += sceneLayer.lights.size();
        decalInstanceIndex += sceneLayer.decals.size();
    }
}

void Renderer::renderLayerGBuffer(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData)
{
    const std::array initialImageMemoryBarriers {
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask = {},
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = *std::get<0>(gBuffer.colorTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask = {},
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = *std::get<0>(gBuffer.normalTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask = {},
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .image = *std::get<0>(gBuffer.depthTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 },
        },
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = initialImageMemoryBarriers.size(),
        .pImageMemoryBarriers = initialImageMemoryBarriers.data(),
    });

    const std::array colorAttachments = {
        vk::RenderingAttachmentInfo {
            .imageView = *std::get<2>(gBuffer.colorTexture),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue({ 0.0f, 0.0f, 0.0f, 0.0f }),
        },
        vk::RenderingAttachmentInfo {
            .imageView = *std::get<2>(gBuffer.normalTexture),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue({ 0.0f, 0.0f, 0.0f, 0.0f }),
        },
    };
    const vk::RenderingAttachmentInfo depthAttachmentInfo {
        .imageView = *std::get<2>(gBuffer.depthTexture),
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearValue({ 1.0f, 0 }),
    };
    commandBuffer.beginRendering(vk::RenderingInfo {
        .renderArea = vk::Rect2D { .extent = gBuffer.extent },
        .layerCount = 1,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = &depthAttachmentInfo,
    });
    commandBuffer.setViewport(0, vk::Viewport {
            .x = 0,
            .y = static_cast<float>(gBuffer.extent.height),
            .width = static_cast<float>(gBuffer.extent.width),
            .height = -static_cast<float>(gBuffer.extent.height),
            .minDepth = 0,
            .maxDepth = 1,
        });
    commandBuffer.setScissor(0, vk::Rect2D { .extent = gBuffer.extent });
    commandBuffer.setDepthTestEnable(vk::True);
    commandBuffer.setDepthWriteEnable(vk::True);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.gBuffer, 0, {
            descriptorSets.textureArray,
            frameData.descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
        }, { layerDrawInfo.uniformBufferOffset, layerDrawInfo.uniformBufferOffset + UniformBlockSize::VertexShader });

    if (layerDrawInfo.spriteInstanceCount > 0)
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.sprite);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.gBuffer, 2, {
                frameData.descriptorSets[FrameDataDescriptorSetIDs::SpriteInstanceBuffer],
            }, {});

        commandBuffer.draw(4, layerDrawInfo.spriteInstanceCount, 0, layerDrawInfo.spriteFirstInstanceIndex);
    }

    if (geometryVertexBuffer && geometryIndexBuffer && layerDrawInfo.geometryInstanceCount > 0)
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.geometry);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.gBuffer, 2, {
                frameData.descriptorSets[FrameDataDescriptorSetIDs::GeometryInstanceBuffer],
            }, {});

        commandBuffer.bindVertexBuffers(0, geometryVertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(geometryIndexBuffer, 0, vk::IndexType::eUint32);

        for (uint32_t i = 0; i < layerDrawInfo.indirectBuffersCount; ++i)
        {
            const uint32_t drawCount = (i + 1 < layerDrawInfo.indirectBuffersCount)
                ? MAX_GEOMETRY : layerDrawInfo.geometryInstanceCount % MAX_GEOMETRY;

            const auto indirectBuffer = *std::get<0>(frameData.drawIndirectBuffers[layerDrawInfo.firstIndirectBufferIndex + i]);
            commandBuffer.drawIndexedIndirect(indirectBuffer, 0, drawCount, sizeof(vk::DrawIndexedIndirectCommand));
        }
    }

    if (layerDrawInfo.overlaySpriteInstanceCount > 0)
    {
        commandBuffer.setDepthTestEnable(vk::False);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.spriteOverlay);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.gBuffer, 2, {
                frameData.descriptorSets[FrameDataDescriptorSetIDs::SpriteInstanceBuffer],
            }, {});

        commandBuffer.draw(4, layerDrawInfo.overlaySpriteInstanceCount, 0, layerDrawInfo.overlaySpriteFirstInstanceIndex);
    }

    commandBuffer.endRendering();

    const vk::ImageMemoryBarrier2KHR depthImageBarrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
        .oldLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = *std::get<0>(gBuffer.depthTexture),
        .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 },
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &depthImageBarrier,
    });

    const vk::RenderingAttachmentInfo colorAttachment {
        .imageView = *std::get<2>(gBuffer.colorTexture),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };

    commandBuffer.beginRendering(vk::RenderingInfo {
        .renderArea = vk::Rect2D { .extent = gBuffer.extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    });

    if (layerDrawInfo.decalsCount > 0)
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.decal);
        commandBuffer.setViewport(0, vk::Viewport {
                .x = 0,
                .y = static_cast<float>(gBuffer.extent.height),
                .width = static_cast<float>(gBuffer.extent.width),
                .height = -static_cast<float>(gBuffer.extent.height),
                .minDepth = 0,
                .maxDepth = 1,
            });
        commandBuffer.setScissor(0, vk::Rect2D { .extent = gBuffer.extent });
        commandBuffer.setDepthTestEnable(vk::False);
        commandBuffer.setDepthWriteEnable(vk::False);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.decal, 0, {
                descriptorSets.gBuffer,
                frameData.descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
                frameData.descriptorSets[FrameDataDescriptorSetIDs::DecalInstanceBuffer],
                descriptorSets.textureArray,
            }, {
                layerDrawInfo.uniformBufferOffset,
                layerDrawInfo.uniformBufferOffset + UniformBlockSize::VertexShader,
            });

        commandBuffer.bindVertexBuffers(0, *std::get<0>(decalGeometryBuffer), { 0 });
        commandBuffer.draw(36, layerDrawInfo.decalsCount, 0, layerDrawInfo.decalFirstInstanceIndex);
    }

    commandBuffer.endRendering();

    const std::array finalImageMemoryBarriers {
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .image = *std::get<0>(gBuffer.colorTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .image = *std::get<0>(gBuffer.normalTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = finalImageMemoryBarriers.size(),
        .pImageMemoryBarriers = finalImageMemoryBarriers.data(),
    });
}

void Renderer::renderLayerSSAO(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData)
{
    const std::array initialImageMemoryBarriers {
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask = {},
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = *std::get<0>(ambientOcclusionTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = initialImageMemoryBarriers.size(),
        .pImageMemoryBarriers = initialImageMemoryBarriers.data(),
    });

    const std::array colorAttachments = {
        vk::RenderingAttachmentInfo {
            .imageView = *std::get<2>(ambientOcclusionTexture),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue({ 0.0f, 0.0f, 0.0f, 0.0f }),
        },
    };
    commandBuffer.beginRendering(vk::RenderingInfo {
        .renderArea = vk::Rect2D { .extent = ambientOcclusionTextureExtent },
        .layerCount = 1,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
    });

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.ssao);

    commandBuffer.setViewport(0, vk::Viewport {
            .x = 0,
            .y = static_cast<float>(ambientOcclusionTextureExtent.height),
            .width = static_cast<float>(ambientOcclusionTextureExtent.width),
            .height = -static_cast<float>(ambientOcclusionTextureExtent.height),
            .minDepth = 0,
            .maxDepth = 1,
        });
    commandBuffer.setScissor(0, vk::Rect2D { .extent = ambientOcclusionTextureExtent });
    commandBuffer.setDepthTestEnable(vk::False);
    commandBuffer.setDepthWriteEnable(vk::False);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.ssao, 0, {
            descriptorSets.gBuffer,
            frameData.descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
        }, {
            layerDrawInfo.uniformBufferOffset,
            layerDrawInfo.uniformBufferOffset + UniformBlockSize::VertexShader
        });

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRendering();

    const std::array finalImageMemoryBarriers {
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .image = *std::get<0>(ambientOcclusionTexture),
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = finalImageMemoryBarriers.size(),
        .pImageMemoryBarriers = finalImageMemoryBarriers.data(),
    });
}

void Renderer::renderLayerShadowMap(const vk::raii::CommandBuffer& commandBuffer, const LayerDrawInfo& layerDrawInfo, const FrameData& frameData)
{
    if (!(geometryVertexBuffer && geometryIndexBuffer && layerDrawInfo.geometryInstanceCount > 0))
    {
        return;
    }

    for (uint32_t i = 0; i < layerDrawInfo.pointShadowsCount; ++i)
    {
        const std::array initialImageMemoryBarriers {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .srcAccessMask = {},
                .dstStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests,
                .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .image = *shadowCubeMaps[i].image,
                .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 6 },
            },
        };

        commandBuffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = initialImageMemoryBarriers.size(),
            .pImageMemoryBarriers = initialImageMemoryBarriers.data(),
        });

        commandBuffer.setViewport(0, vk::Viewport {
                .x = 0,
                .y = 0,
                .width = static_cast<float>(ShadowMapSize.width),
                .height = static_cast<float>(ShadowMapSize.height),
                .minDepth = 0,
                .maxDepth = 1,
            });
        commandBuffer.setScissor(0, vk::Rect2D { .extent = ShadowMapSize });
        commandBuffer.setDepthTestEnable(vk::True);
        commandBuffer.setDepthWriteEnable(vk::True);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.geometryDepth);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.shadowDepth, 0, {
                frameData.descriptorSets[FrameDataDescriptorSetIDs::GeometryInstanceBuffer],
            }, { });

        commandBuffer.bindVertexBuffers(0, geometryVertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(geometryIndexBuffer, 0, vk::IndexType::eUint32);

        const auto projection = glm::perspectiveRH_ZO(0.5f * glm::pi<float>(), 1.0f, 0.1f, 100.f);
        const auto mzv = glm::translate(glm::mat4(1), -pointShadowPositions[layerDrawInfo.firstPointShadowPos + i]);
        // https://docs.vulkan.org/spec/latest/chapters/textures.html#_cube_map_face_selection_and_transformations
        const std::array faceMatrices {
            glm::mat4( 0, 0, -1, 0,   0, -1,  0, 0,  -1,  0,  0, 0,   0, 0, 0, 1) * mzv, // +X
            glm::mat4( 0, 0,  1, 0,   0, -1,  0, 0,   1,  0,  0, 0,   0, 0, 0, 1) * mzv, // -X
            glm::mat4( 1, 0,  0, 0,   0,  0, -1, 0,   0,  1,  0, 0,   0, 0, 0, 1) * mzv, // +Y
            glm::mat4( 1, 0,  0, 0,   0,  0,  1, 0,   0, -1,  0, 0,   0, 0, 0, 1) * mzv, // -Y
            glm::mat4( 1, 0,  0, 0,   0, -1,  0, 0,   0,  0, -1, 0,   0, 0, 0, 1) * mzv, // +Z
            glm::mat4(-1, 0,  0, 0,   0, -1,  0, 0,   0,  0,  1, 0,   0, 0, 0, 1) * mzv, // -Z
        };

        for (int j = 0; j < 6; ++j)
        {
            const vk::RenderingAttachmentInfo depthAttachmentInfo {
                .imageView = *shadowCubeMaps[i].faceImageViews[j],
                .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = vk::ClearValue({ 1.0f, 0 }),
            };

            commandBuffer.beginRendering(vk::RenderingInfo {
                .renderArea = vk::Rect2D { .extent = ShadowMapSize },
                .layerCount = 1,
                .pDepthAttachment = &depthAttachmentInfo,
            });

            commandBuffer.pushConstants(pipelineLayouts.shadowDepth, vk::ShaderStageFlagBits::eVertex, 0,
                    vk::ArrayProxy<const float>(16, glm::value_ptr(projection * faceMatrices[j])));

            for (uint32_t k = 0; k < layerDrawInfo.indirectBuffersCount; ++k)
            {
                const uint32_t drawCount = (k + 1 < layerDrawInfo.indirectBuffersCount)
                    ? MAX_GEOMETRY : layerDrawInfo.geometryInstanceCount % MAX_GEOMETRY;

                const auto indirectBuffer = *std::get<0>(frameData.drawIndirectBuffers[layerDrawInfo.firstIndirectBufferIndex + k]);
                commandBuffer.drawIndexedIndirect(indirectBuffer, 0, drawCount, sizeof(vk::DrawIndexedIndirectCommand));
            }

            commandBuffer.endRendering();
        }

        const std::array finalImageMemoryBarriers {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests,
                .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
                .oldLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .image = *shadowCubeMaps[i].image,
                .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 6 },
            }
        };

        commandBuffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = finalImageMemoryBarriers.size(),
            .pImageMemoryBarriers = finalImageMemoryBarriers.data(),
        });
    }
}

void Renderer::drawFrame(const Swapchain& swapchain, const glm::vec2& viewportExtent)
{
    auto [acquireResult, imageIndex] = swapchain.swapchain.acquireNextImage(std::numeric_limits<uint64_t>::max(), frameData[frameIndex].imageAcquiredSemaphore, nullptr);
    if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Unexpected return from acquireNextImage");
    }

    const auto& commandBuffer = frameData[frameIndex].commandBuffers.front();
    commandBuffer.begin(vk::CommandBufferBeginInfo {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    const std::array initialImageMemoryBarriers {
        vk::ImageMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask = {},
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = swapchain.images[imageIndex],
            .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        },
    };

    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
        .imageMemoryBarrierCount = initialImageMemoryBarriers.size(),
        .pImageMemoryBarriers = initialImageMemoryBarriers.data(),
    });

    bool first = true;
    for (const auto& layerDrawInfo : layerDrawInfos)
    {
        renderLayerGBuffer(commandBuffer, layerDrawInfo, frameData[frameIndex]);

        renderLayerShadowMap(commandBuffer, layerDrawInfo, frameData[frameIndex]);

        renderLayerSSAO(commandBuffer, layerDrawInfo, frameData[frameIndex]);

        const vk::RenderingAttachmentInfo renderingAttachmentInfo {
            .imageView = swapchain.imageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = first ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue({ 0.0f, 0.0f, 0.0f, 0.0f }),
        };

        commandBuffer.beginRendering(vk::RenderingInfo {
            .renderArea = vk::Rect2D { .extent = swapchain.extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &renderingAttachmentInfo,
        });

        commandBuffer.setViewport(0, layerDrawInfo.viewport);
        commandBuffer.setScissor(0, layerDrawInfo.scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.deferred);
        commandBuffer.setDepthTestEnable(vk::False);
        commandBuffer.setDepthWriteEnable(vk::False);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.deferred, 0, {
                descriptorSets.gBuffer,
                frameData[frameIndex].descriptorSets[FrameDataDescriptorSetIDs::SceneUniformData],
                descriptorSets.ambientOcclusionTexture,
                descriptorSets.shadowCubeMapArray,
            }, {
                layerDrawInfo.uniformBufferOffset,
                layerDrawInfo.uniformBufferOffset + UniformBlockSize::VertexShader
            });

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
        first = false;
    }

    const vk::ImageMemoryBarrier2 finalImageMemoryBarrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask = {},
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .image = swapchain.images[imageIndex],
        .subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &finalImageMemoryBarrier,
        });

    commandBuffer.end();

    const vk::CommandBufferSubmitInfo commandBufferSubmitInfo {
        .commandBuffer = commandBuffer,
    };

    const vk::SemaphoreSubmitInfo waitSemaphoreInfo {
        .semaphore = frameData[frameIndex].imageAcquiredSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };

    const vk::SemaphoreSubmitInfo signalSemaphoreInfo {
        .semaphore = frameData[frameIndex].renderFinishedSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
    };

    queue.submit2(vk::SubmitInfo2 {
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo,
        }, frameData[frameIndex].inFlightFence);

    if (auto result = queue.presentKHR(vk::PresentInfoKHR {
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*frameData[frameIndex].renderFinishedSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &*swapchain.swapchain,
                .pImageIndices = &imageIndex,
            });
            result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Unexpected return from presentKHR");
    }
    else if (result == vk::Result::eSuboptimalKHR)
    {
        // TODO: recreate swapchain
    }
}

void Renderer::nextFrame()
{
    frameIndex = (frameIndex + 1) % frameData.size();
}

void Renderer::updateFramebufferExtent(const vk::Extent2D& framebufferExtent)
{
    frameData[frameIndex].toDelete.emplace_back(new Deleter { std::move(gBuffer.colorTexture) });
    frameData[frameIndex].toDelete.emplace_back(new Deleter { std::move(gBuffer.normalTexture) });
    frameData[frameIndex].toDelete.emplace_back(new Deleter { std::move(gBuffer.depthTexture) });
    frameData[frameIndex].toDelete.emplace_back(new Deleter { std::move(ambientOcclusionTexture) });

    gBuffer.recreate(device, allocator, framebufferExtent);
    writeGBufferDescriptorSet(device, textureSampler, gBuffer, descriptorSets.gBuffer);

    ambientOcclusionTextureExtent = vk::Extent2D(framebufferExtent.width / 4, framebufferExtent.height / 4);
    ambientOcclusionTexture = createTexture(device, allocator, { ambientOcclusionTextureExtent.width, ambientOcclusionTextureExtent.height, 1 },
            vk::Format::eR16Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
    writeSingleTextureDescriptorSet(device, textureSampler, std::get<2>(ambientOcclusionTexture), descriptorSets.ambientOcclusionTexture);
}
