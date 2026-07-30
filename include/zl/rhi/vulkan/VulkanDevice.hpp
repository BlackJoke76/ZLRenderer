#pragma once

#include "zl/rhi/CompiledShaderLibrary.hpp"
#include "zl/rhi/RHI.hpp"
#include "zl/rhi/vulkan/VulkanPipelineCache.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zl::rhi::vulkan {

struct VulkanDeviceCreateInfo {
    GLFWwindow* window = nullptr;
    const char* applicationName = "ZLRenderer";
    bool enableValidation = true;
};

class VulkanDevice final : public Device {
public:
    explicit VulkanDevice(const VulkanDeviceCreateInfo& createInfo);
    ~VulkanDevice() override;

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    bool beginFrame() override;
    CommandList& commandList() override;
    const FrameContext& frameContext() const override;
    ResourceState activeSwapchainImageState() const override;
    void endFrame() override;
    void waitIdle() override;

    void notifyFramebufferResized();

private:
    class VulkanCommandList final : public CommandList {
    public:
        explicit VulkanCommandList(VulkanDevice& device);

        void transitionSwapchainImage(ResourceState state) override;
        void clearSwapchainImage(float red, float green, float blue, float alpha) override;
        void drawTriangleToSwapchain() override;

    private:
        VulkanDevice& device_;
    };

    struct QueueFamilyIndices {
        std::optional<std::uint32_t> graphicsFamily;
        std::optional<std::uint32_t> presentFamily;

        bool complete() const;
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct BufferAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    struct FrameResources {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
        BufferAllocation triangleUniformBuffer;
        void* triangleUniformMapped = nullptr;
        VkDescriptorSet triangleDescriptorSet = VK_NULL_HANDLE;
    };

    static constexpr std::uint32_t framesInFlight = 2;

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createSwapchainImageViews();
    void createSwapchainRenderPass();
    void createSwapchainFramebuffers();
    void createPipelineCache();
    void createTriangleDescriptorSetLayout();
    void createTriangleDescriptorPool();
    void createTriangleVertexBuffer();
    void createTrianglePipeline();
    void createFrameResources();
    void recreateSwapchain();
    void cleanupSwapchain();

    bool validationLayersAvailable() const;
    std::vector<const char*> requiredInstanceExtensions() const;
    bool physicalDeviceSuitable(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool deviceExtensionSupported(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags requiredProperties) const;
    BufferAllocation createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags requiredProperties) const;
    void destroyBuffer(BufferAllocation& allocation) const;
    void uploadBufferToVertexInput(VkBuffer source, VkBuffer destination, VkDeviceSize size) const;

    void beginActiveCommandBuffer();
    void endActiveCommandBuffer();
    void transitionActiveSwapchainImage(ResourceState newState);
    void updateTriangleUniformBuffer(FrameResources& frame);

    GraphicsPipelineKey trianglePipelineKey(VkPipelineLayout pipelineLayout) const;
    VkShaderModule createShaderModule(const std::vector<std::uint32_t>& code) const;
    VkCommandBuffer activeCommandBuffer() const;
    VkImage activeSwapchainImage() const;
    VkFramebuffer activeSwapchainFramebuffer() const;

    GLFWwindow* window_ = nullptr;
    bool validationEnabled_ = false;
    bool framebufferResized_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    std::vector<VkImageLayout> swapchainImageLayouts_;
    std::vector<VkSemaphore> swapchainImageRenderFinishedSemaphores_;

    VkRenderPass swapchainRenderPass_ = VK_NULL_HANDLE;
    std::unique_ptr<VulkanPipelineCache> pipelineCache_;
    VkDescriptorSetLayout triangleDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool triangleDescriptorPool_ = VK_NULL_HANDLE;
    VkPipelineLayout trianglePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline trianglePipeline_ = VK_NULL_HANDLE;
    BufferAllocation triangleVertexBuffer_;

    std::array<FrameResources, framesInFlight> frames_{};
    std::uint32_t currentFrame_ = 0;
    FrameContext frameContext_{};
    bool frameActive_ = false;

    CompiledShaderLibrary compiledShaderLibrary_;
    VulkanCommandList commandList_;
};

} // namespace zl::rhi::vulkan
