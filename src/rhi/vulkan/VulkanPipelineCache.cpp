#include "zl/rhi/vulkan/VulkanPipelineCache.hpp"

#include <stdexcept>
#include <utility>

namespace zl::rhi::vulkan {
namespace {

template <typename T>
void hashCombine(std::size_t& seed, const T& value)
{
    seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
}

} // namespace

std::size_t GraphicsPipelineKeyHash::operator()(const GraphicsPipelineKey& key) const
{
    std::size_t seed = 0;
    hashCombine(seed, static_cast<std::uint32_t>(key.topology));
    hashCombine(seed, key.vertexShader);
    hashCombine(seed, key.fragmentShader);
    hashCombine(seed, key.colorFormat);
    hashCombine(seed, key.pipelineLayout);
    hashCombine(seed, key.renderPass);
    hashCombine(seed, key.subpass);
    return seed;
}

VulkanPipelineCache::VulkanPipelineCache(VkDevice device)
    : device_(device)
{
    if (device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanPipelineCache requires a valid device.");
    }

    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    if (vkCreatePipelineCache(device_, &createInfo, nullptr, &driverCache_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan pipeline cache.");
    }
}

VulkanPipelineCache::~VulkanPipelineCache()
{
    clearPipelines();

    if (driverCache_ != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device_, driverCache_, nullptr);
    }
}

VkPipeline VulkanPipelineCache::getOrCreate(
    const GraphicsPipelineKey& key,
    const PipelineFactory& factory)
{
    if (const auto found = pipelines_.find(key); found != pipelines_.end()) {
        return found->second;
    }
    if (!factory) {
        throw std::runtime_error("VulkanPipelineCache requires a pipeline factory on cache miss.");
    }

    const auto pipeline = factory(driverCache_);
    if (pipeline == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanPipelineCache factory returned a null pipeline.");
    }

    pipelines_.emplace(key, pipeline);
    return pipeline;
}

void VulkanPipelineCache::clearPipelines()
{
    for (const auto& [key, pipeline] : pipelines_) {
        static_cast<void>(key);
        vkDestroyPipeline(device_, pipeline, nullptr);
    }
    pipelines_.clear();
}

} // namespace zl::rhi::vulkan
