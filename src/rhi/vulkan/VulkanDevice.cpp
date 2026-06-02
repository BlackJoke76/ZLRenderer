#include "zl/rhi/vulkan/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

#ifndef ZL_SHADER_DIR
#define ZL_SHADER_DIR "."
#endif

namespace zl::rhi::vulkan {
namespace {

constexpr std::array validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
    if (callbackData != nullptr && callbackData->pMessage != nullptr) {
        fprintf(stderr, "Vulkan validation: %s\n", callbackData->pMessage);
    }

    return VK_FALSE;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* messenger)
{
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (function == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, createInfo, allocator, messenger);
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* allocator)
{
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function != nullptr) {
        function(instance, messenger, allocator);
    }
}

VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    return createInfo;
}

void checkVk(VkResult result, const char* message)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

std::vector<std::uint32_t> readSpirvFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SPIR-V file: " + path.string());
    }

    const auto fileSize = static_cast<std::streamoff>(file.tellg());
    if (fileSize < 0 || fileSize % 4 != 0) {
        throw std::runtime_error("SPIR-V file size is invalid: " + path.string());
    }

    std::vector<std::uint32_t> code(static_cast<std::size_t>(fileSize) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));
    if (!file) {
        throw std::runtime_error("Failed to read SPIR-V file: " + path.string());
    }

    return code;
}

std::filesystem::path shaderPath(const char* fileName)
{
    return std::filesystem::path{ZL_SHADER_DIR} / fileName;
}

VkImageLayout imageLayoutForResourceState(ResourceState state)
{
    switch (state) {
    case ResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::RenderTarget:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ResourceState::ShaderRead:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ResourceState::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ResourceState::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ResourceState::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ResourceState::DepthWrite:
    case ResourceState::ShaderWrite:
        break;
    }

    throw std::runtime_error("Unsupported swapchain resource state.");
}

ResourceState resourceStateForImageLayout(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return ResourceState::Undefined;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return ResourceState::RenderTarget;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return ResourceState::ShaderRead;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return ResourceState::TransferSrc;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return ResourceState::TransferDst;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return ResourceState::Present;
    default:
        break;
    }

    throw std::runtime_error("Unsupported tracked swapchain image layout.");
}

VkAccessFlags accessMaskForLayout(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return 0;
    default:
        break;
    }

    throw std::runtime_error("Unsupported access mask for swapchain image layout.");
}

VkPipelineStageFlags pipelineStageForLayout(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        break;
    }

    throw std::runtime_error("Unsupported pipeline stage for swapchain image layout.");
}

VkShaderStageFlagBits shaderStageFlag(ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    throw std::runtime_error("Unsupported shader stage.");
}

VkPrimitiveTopology primitiveTopology(PrimitiveTopology topology)
{
    switch (topology) {
    case PrimitiveTopology::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    throw std::runtime_error("Unsupported primitive topology.");
}

class ScopedShaderModule {
public:
    ScopedShaderModule(VkDevice device, VkShaderModule module)
        : device_(device)
        , module_(module)
    {
    }

    ~ScopedShaderModule()
    {
        if (module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module_, nullptr);
        }
    }

    ScopedShaderModule(const ScopedShaderModule&) = delete;
    ScopedShaderModule& operator=(const ScopedShaderModule&) = delete;
    ScopedShaderModule(ScopedShaderModule&&) = delete;
    ScopedShaderModule& operator=(ScopedShaderModule&&) = delete;

    VkShaderModule get() const
    {
        return module_;
    }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkShaderModule module_ = VK_NULL_HANDLE;
};

class ScopedPipelineLayout {
public:
    ScopedPipelineLayout(VkDevice device, VkPipelineLayout layout)
        : device_(device)
        , layout_(layout)
    {
    }

    ~ScopedPipelineLayout()
    {
        if (layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, layout_, nullptr);
        }
    }

    ScopedPipelineLayout(const ScopedPipelineLayout&) = delete;
    ScopedPipelineLayout& operator=(const ScopedPipelineLayout&) = delete;
    ScopedPipelineLayout(ScopedPipelineLayout&&) = delete;
    ScopedPipelineLayout& operator=(ScopedPipelineLayout&&) = delete;

    VkPipelineLayout get() const
    {
        return layout_;
    }

    VkPipelineLayout release()
    {
        return std::exchange(layout_, VK_NULL_HANDLE);
    }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
};

} // namespace

bool VulkanDevice::QueueFamilyIndices::complete() const
{
    return graphicsFamily.has_value() && presentFamily.has_value();
}

VulkanDevice::VulkanCommandList::VulkanCommandList(VulkanDevice& device)
    : device_(device)
{
}

void VulkanDevice::VulkanCommandList::transitionSwapchainImage(ResourceState state)
{
    device_.transitionActiveSwapchainImage(state);
}

void VulkanDevice::VulkanCommandList::clearSwapchainImage(
    float red,
    float green,
    float blue,
    float alpha)
{
    VkClearColorValue clearColor{};
    clearColor.float32[0] = red;
    clearColor.float32[1] = green;
    clearColor.float32[2] = blue;
    clearColor.float32[3] = alpha;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(
        device_.activeCommandBuffer(),
        device_.activeSwapchainImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1,
        &range);
}

void VulkanDevice::VulkanCommandList::drawTriangleToSwapchain()
{
    VkClearValue clearValue{};
    clearValue.color.float32[0] = 0.03f;
    clearValue.color.float32[1] = 0.06f;
    clearValue.color.float32[2] = 0.09f;
    clearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = device_.swapchainRenderPass_;
    beginInfo.framebuffer = device_.activeSwapchainFramebuffer();
    beginInfo.renderArea.offset = {0, 0};
    beginInfo.renderArea.extent = device_.swapchainExtent_;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(device_.activeCommandBuffer(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(device_.swapchainExtent_.width);
    viewport.height = static_cast<float>(device_.swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = device_.swapchainExtent_;

    vkCmdSetViewport(device_.activeCommandBuffer(), 0, 1, &viewport);
    vkCmdSetScissor(device_.activeCommandBuffer(), 0, 1, &scissor);
    vkCmdBindPipeline(device_.activeCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, device_.trianglePipeline_);
    vkCmdDraw(device_.activeCommandBuffer(), 3, 1, 0, 0);

    vkCmdEndRenderPass(device_.activeCommandBuffer());
}

VulkanDevice::VulkanDevice(const VulkanDeviceCreateInfo& createInfo)
    : window_(createInfo.window)
    , validationEnabled_(createInfo.enableValidation)
    , commandList_(*this)
{
    if (window_ == nullptr) {
        throw std::runtime_error("VulkanDevice requires a valid GLFW window.");
    }

    if (validationEnabled_ && !validationLayersAvailable()) {
        validationEnabled_ = false;
    }

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createSwapchainImageViews();
    createSwapchainRenderPass();
    createSwapchainFramebuffers();
    createPipelineCache();
    createTrianglePipeline();
    createFrameResources();

    frameContext_.framesInFlight = framesInFlight;
}

VulkanDevice::~VulkanDevice()
{
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    cleanupSwapchain();
    pipelineCache_.reset();

    for (auto& frame : frames_) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device_, frame.inFlightFence, nullptr);
        }
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, frame.imageAvailableSemaphore, nullptr);
        }
        if (frame.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, frame.commandPool, nullptr);
        }
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (debugMessenger_ != VK_NULL_HANDLE) {
        destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

bool VulkanDevice::beginFrame()
{
    if (frameActive_) {
        throw std::runtime_error("beginFrame called while a frame is active.");
    }

    auto& frame = frames_[currentFrame_];
    checkVk(vkWaitForFences(device_, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX), "Failed to wait for frame fence.");

    auto acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        frame.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &frameContext_.swapchainImageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return false;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    const auto imageIndex = frameContext_.swapchainImageIndex;
    if (swapchainImageFences_[imageIndex] != VK_NULL_HANDLE) {
        checkVk(
            vkWaitForFences(device_, 1, &swapchainImageFences_[imageIndex], VK_TRUE, UINT64_MAX),
            "Failed to wait for swapchain image fence.");
    }
    swapchainImageFences_[imageIndex] = frame.inFlightFence;

    checkVk(vkResetFences(device_, 1, &frame.inFlightFence), "Failed to reset frame fence.");
    checkVk(vkResetCommandPool(device_, frame.commandPool, 0), "Failed to reset command pool.");

    frameContext_.frameIndex = currentFrame_;
    beginActiveCommandBuffer();
    frameActive_ = true;

    return true;
}

CommandList& VulkanDevice::commandList()
{
    return commandList_;
}

const FrameContext& VulkanDevice::frameContext() const
{
    return frameContext_;
}

ResourceState VulkanDevice::activeSwapchainImageState() const
{
    if (!frameActive_) {
        throw std::runtime_error("activeSwapchainImageState called without an active frame.");
    }

    return resourceStateForImageLayout(swapchainImageLayouts_[frameContext_.swapchainImageIndex]);
}

void VulkanDevice::endFrame()
{
    if (!frameActive_) {
        throw std::runtime_error("endFrame called without an active frame.");
    }

    auto& frame = frames_[currentFrame_];
    endActiveCommandBuffer();

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores =
        &swapchainImageRenderFinishedSemaphores_[frameContext_.swapchainImageIndex];

    checkVk(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, frame.inFlightFence), "Failed to submit command buffer.");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores =
        &swapchainImageRenderFinishedSemaphores_[frameContext_.swapchainImageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &frameContext_.swapchainImageIndex;

    const auto presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    currentFrame_ = (currentFrame_ + 1) % framesInFlight;
    frameActive_ = false;
}

void VulkanDevice::waitIdle()
{
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

void VulkanDevice::notifyFramebufferResized()
{
    framebufferResized_ = true;
}

void VulkanDevice::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ZLRenderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "ZLRenderer";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    auto extensions = requiredInstanceExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    auto debugCreateInfo = debugMessengerCreateInfo();
    if (validationEnabled_) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.pNext = &debugCreateInfo;
    }

    checkVk(vkCreateInstance(&createInfo, nullptr, &instance_), "Failed to create Vulkan instance.");
}

void VulkanDevice::setupDebugMessenger()
{
    if (!validationEnabled_) {
        return;
    }

    auto createInfo = debugMessengerCreateInfo();
    checkVk(
        createDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_),
        "Failed to create Vulkan debug messenger.");
}

void VulkanDevice::createSurface()
{
    checkVk(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_), "Failed to create window surface.");
}

void VulkanDevice::pickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (auto device : devices) {
        if (physicalDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable Vulkan physical device found.");
    }
}

void VulkanDevice::createLogicalDevice()
{
    const auto indices = findQueueFamilies(physicalDevice_);
    const std::set uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (const auto family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.pNext = &vulkan11Features;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pNext = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (validationEnabled_) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    checkVk(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "Failed to create logical device.");

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

void VulkanDevice::createSwapchain()
{
    const auto support = querySwapchainSupport(physicalDevice_);
    const auto surfaceFormat = chooseSurfaceFormat(support.formats);
    const auto presentMode = choosePresentMode(support.presentModes);
    const auto extent = chooseSwapchainExtent(support.capabilities);

    auto imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const auto indices = findQueueFamilies(physicalDevice_);
    const std::array queueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilies.size());
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    checkVk(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "Failed to create swapchain.");

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    swapchainImageLayouts_.assign(imageCount, VK_IMAGE_LAYOUT_UNDEFINED);
    swapchainImageFences_.assign(imageCount, VK_NULL_HANDLE);
    swapchainImageRenderFinishedSemaphores_.resize(imageCount);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& semaphore : swapchainImageRenderFinishedSemaphores_) {
        checkVk(
            vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &semaphore),
            "Failed to create swapchain-image render-finished semaphore.");
    }
}

void VulkanDevice::createSwapchainImageViews()
{
    swapchainImageViews_.resize(swapchainImages_.size());

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        checkVk(
            vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]),
            "Failed to create swapchain image view.");
    }
}

void VulkanDevice::createSwapchainRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    checkVk(vkCreateRenderPass(device_, &createInfo, nullptr, &swapchainRenderPass_), "Failed to create swapchain render pass.");
}

void VulkanDevice::createSwapchainFramebuffers()
{
    swapchainFramebuffers_.resize(swapchainImageViews_.size());

    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        const VkImageView attachments[] = {
            swapchainImageViews_[i],
        };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = swapchainRenderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;

        checkVk(
            vkCreateFramebuffer(device_, &createInfo, nullptr, &swapchainFramebuffers_[i]),
            "Failed to create swapchain framebuffer.");
    }
}

void VulkanDevice::createPipelineCache()
{
    pipelineCache_ = std::make_unique<VulkanPipelineCache>(device_);
}

void VulkanDevice::createTrianglePipeline()
{
    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    checkVk(
        vkCreatePipelineLayout(device_, &layoutCreateInfo, nullptr, &pipelineLayout),
        "Failed to create triangle pipeline layout.");
    ScopedPipelineLayout scopedPipelineLayout(device_, pipelineLayout);

    const auto key = trianglePipelineKey(scopedPipelineLayout.get());
    if (key.colorFormat != swapchainImageFormat_) {
        throw std::runtime_error("Triangle pipeline key does not match the active swapchain format.");
    }

    const std::array shaderDescs = {
        ShaderModuleDesc{
            .stage = ShaderStage::Vertex,
            .spirvFileName = key.vertexShader.c_str(),
            .entryPoint = "main",
        },
        ShaderModuleDesc{
            .stage = ShaderStage::Fragment,
            .spirvFileName = key.fragmentShader.c_str(),
            .entryPoint = "main",
        },
    };

    const auto vertexShader = readSpirvFile(shaderPath(shaderDescs[0].spirvFileName));
    const auto fragmentShader = readSpirvFile(shaderPath(shaderDescs[1].spirvFileName));
    const ScopedShaderModule vertexShaderModule(device_, createShaderModule(vertexShader));
    const ScopedShaderModule fragmentShaderModule(device_, createShaderModule(fragmentShader));

    VkPipelineShaderStageCreateInfo vertexStage{};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = shaderStageFlag(shaderDescs[0].stage);
    vertexStage.module = vertexShaderModule.get();
    vertexStage.pName = shaderDescs[0].entryPoint;

    VkPipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = shaderStageFlag(shaderDescs[1].stage);
    fragmentStage.module = fragmentShaderModule.get();
    fragmentStage.pName = shaderDescs[1].entryPoint;

    const std::array shaderStages = {
        vertexStage,
        fragmentStage,
    };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = primitiveTopology(key.topology);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInput;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = key.pipelineLayout;
    pipelineCreateInfo.renderPass = key.renderPass;
    pipelineCreateInfo.subpass = key.subpass;

    trianglePipeline_ = pipelineCache_->getOrCreate(
        key,
        [this, &pipelineCreateInfo](VkPipelineCache driverCache) {
            VkPipeline pipeline = VK_NULL_HANDLE;
            checkVk(
                vkCreateGraphicsPipelines(device_, driverCache, 1, &pipelineCreateInfo, nullptr, &pipeline),
                "Failed to create triangle graphics pipeline.");
            return pipeline;
        });
    trianglePipelineLayout_ = scopedPipelineLayout.release();
}

void VulkanDevice::createFrameResources()
{
    const auto indices = findQueueFamilies(physicalDevice_);

    for (auto& frame : frames_) {
        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();

        checkVk(vkCreateCommandPool(device_, &poolCreateInfo, nullptr, &frame.commandPool), "Failed to create command pool.");

        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = frame.commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        checkVk(vkAllocateCommandBuffers(device_, &allocateInfo, &frame.commandBuffer), "Failed to allocate command buffer.");

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        checkVk(
            vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frame.imageAvailableSemaphore),
            "Failed to create image-available semaphore.");

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        checkVk(vkCreateFence(device_, &fenceCreateInfo, nullptr, &frame.inFlightFence), "Failed to create frame fence.");
    }
}

void VulkanDevice::recreateSwapchain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapchain();
    createSwapchain();
    createSwapchainImageViews();
    createSwapchainRenderPass();
    createSwapchainFramebuffers();
    createTrianglePipeline();
}

void VulkanDevice::cleanupSwapchain()
{
    trianglePipeline_ = VK_NULL_HANDLE;
    if (pipelineCache_ != nullptr) {
        pipelineCache_->clearPipelines();
    }
    if (trianglePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, trianglePipelineLayout_, nullptr);
        trianglePipelineLayout_ = VK_NULL_HANDLE;
    }

    for (auto framebuffer : swapchainFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    swapchainFramebuffers_.clear();

    if (swapchainRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, swapchainRenderPass_, nullptr);
        swapchainRenderPass_ = VK_NULL_HANDLE;
    }

    for (auto imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    swapchainImageLayouts_.clear();
    swapchainImageFences_.clear();

    for (auto semaphore : swapchainImageRenderFinishedSemaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    swapchainImageRenderFinishedSemaphores_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

bool VulkanDevice::validationLayersAvailable() const
{
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* requiredLayer : validationLayers) {
        const auto found = std::ranges::any_of(availableLayers, [requiredLayer](const VkLayerProperties& layer) {
            return std::strcmp(requiredLayer, layer.layerName) == 0;
        });
        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanDevice::requiredInstanceExtensions() const
{
    std::uint32_t glfwExtensionCount = 0;
    const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        throw std::runtime_error("GLFW did not provide Vulkan instance extensions.");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (validationEnabled_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool VulkanDevice::physicalDeviceSuitable(VkPhysicalDevice device) const
{
    const auto indices = findQueueFamilies(device);
    const auto extensionsSupported = deviceExtensionSupported(device);
    const auto featuresSupported = physicalDeviceFeaturesSupported(device);

    bool swapchainAdequate = false;
    if (extensionsSupported) {
        const auto support = querySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.complete() && extensionsSupported && featuresSupported && swapchainAdequate;
}

bool VulkanDevice::physicalDeviceFeaturesSupported(VkPhysicalDevice device) const
{
    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &vulkan11Features;

    vkGetPhysicalDeviceFeatures2(device, &features);

    return vulkan11Features.shaderDrawParameters == VK_TRUE;
}

VulkanDevice::QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupported);
        if (presentSupported == VK_TRUE) {
            indices.presentFamily = i;
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool VulkanDevice::deviceExtensionSupported(VkPhysicalDevice device) const
{
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VulkanDevice::SwapchainSupportDetails VulkanDevice::querySwapchainSupport(VkPhysicalDevice device) const
{
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanDevice::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });

    return preferred != formats.end() ? *preferred : formats.front();
}

VkPresentModeKHR VulkanDevice::choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) const
{
    const auto mailbox = std::ranges::find(presentModes, VK_PRESENT_MODE_MAILBOX_KHR);
    if (mailbox != presentModes.end()) {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanDevice::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    VkExtent2D actualExtent{
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}

void VulkanDevice::beginActiveCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    checkVk(vkBeginCommandBuffer(activeCommandBuffer(), &beginInfo), "Failed to begin command buffer.");
}

void VulkanDevice::endActiveCommandBuffer()
{
    checkVk(vkEndCommandBuffer(activeCommandBuffer()), "Failed to end command buffer.");
}

void VulkanDevice::transitionActiveSwapchainImage(ResourceState newState)
{
    const auto imageIndex = frameContext_.swapchainImageIndex;
    const auto oldLayout = swapchainImageLayouts_[imageIndex];
    const auto newLayout = imageLayoutForResourceState(newState);
    if (oldLayout == newLayout) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = activeSwapchainImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = accessMaskForLayout(oldLayout);
    barrier.dstAccessMask = accessMaskForLayout(newLayout);

    const auto sourceStage = pipelineStageForLayout(oldLayout);
    const auto destinationStage = newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
        : pipelineStageForLayout(newLayout);

    vkCmdPipelineBarrier(
        activeCommandBuffer(),
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    swapchainImageLayouts_[imageIndex] = newLayout;
}

GraphicsPipelineKey VulkanDevice::trianglePipelineKey(VkPipelineLayout pipelineLayout) const
{
    return GraphicsPipelineKey{
        .topology = PrimitiveTopology::TriangleList,
        .vertexShader = "triangle.vert.spv",
        .fragmentShader = "triangle.frag.spv",
        .colorFormat = swapchainImageFormat_,
        .pipelineLayout = pipelineLayout,
        .renderPass = swapchainRenderPass_,
        .subpass = 0,
    };
}

VkShaderModule VulkanDevice::createShaderModule(const std::vector<std::uint32_t>& code) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(std::uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule), "Failed to create shader module.");
    return shaderModule;
}

VkCommandBuffer VulkanDevice::activeCommandBuffer() const
{
    return frames_[currentFrame_].commandBuffer;
}

VkImage VulkanDevice::activeSwapchainImage() const
{
    return swapchainImages_[frameContext_.swapchainImageIndex];
}

VkFramebuffer VulkanDevice::activeSwapchainFramebuffer() const
{
    return swapchainFramebuffers_[frameContext_.swapchainImageIndex];
}

} // namespace zl::rhi::vulkan
