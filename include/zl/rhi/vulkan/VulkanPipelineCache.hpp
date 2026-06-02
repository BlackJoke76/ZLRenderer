#pragma once

#include "zl/rhi/RHI.hpp"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace zl::rhi::vulkan {

struct GraphicsPipelineKey {
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    std::string vertexShader;
    std::string fragmentShader;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::uint32_t subpass = 0;

    bool operator==(const GraphicsPipelineKey&) const = default;
};

struct GraphicsPipelineKeyHash {
    std::size_t operator()(const GraphicsPipelineKey& key) const;
};

class VulkanPipelineCache {
public:
    using PipelineFactory = std::function<VkPipeline(VkPipelineCache)>;

    explicit VulkanPipelineCache(VkDevice device);
    ~VulkanPipelineCache();

    VulkanPipelineCache(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache(VulkanPipelineCache&&) = delete;
    VulkanPipelineCache& operator=(VulkanPipelineCache&&) = delete;

    VkPipeline getOrCreate(const GraphicsPipelineKey& key, const PipelineFactory& factory);
    void clearPipelines();

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineCache driverCache_ = VK_NULL_HANDLE;
    std::unordered_map<GraphicsPipelineKey, VkPipeline, GraphicsPipelineKeyHash> pipelines_;
};

} // namespace zl::rhi::vulkan
