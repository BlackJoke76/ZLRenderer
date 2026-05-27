#include "zl/app/Application.hpp"

#include "zl/rhi/vulkan/VulkanDevice.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace zl::app {
namespace {

constexpr int initialWindowWidth = 1280;
constexpr int initialWindowHeight = 720;

} // namespace

Application::Application()
{
    initWindow();
    initRenderer();
}

Application::~Application()
{
    shutdown();
}

void Application::run(std::uint32_t maxFrames)
{
    std::uint32_t renderedFrames = 0;
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        if (framebufferResized_) {
            device_->notifyFramebufferResized();
            framebufferResized_ = false;
        }

        if (!device_->beginFrame()) {
            continue;
        }

        auto& commandList = device_->commandList();
        commandList.clearSwapchainImage(0.03f, 0.06f, 0.09f, 1.0f);
        device_->endFrame();

        ++renderedFrames;
        if (maxFrames != 0 && renderedFrames >= maxFrames) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }

    device_->waitIdle();
}

void Application::initWindow()
{
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(initialWindowWidth, initialWindowHeight, "ZLRenderer", nullptr, nullptr);
    if (window_ == nullptr) {
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

void Application::initRenderer()
{
    rhi::vulkan::VulkanDeviceCreateInfo createInfo{};
    createInfo.window = window_;
    createInfo.applicationName = "ZLRenderer";
    createInfo.enableValidation = ZL_ENABLE_VALIDATION != 0;

    device_ = std::make_unique<rhi::vulkan::VulkanDevice>(createInfo);
}

void Application::shutdown()
{
    device_.reset();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void Application::framebufferResizeCallback(GLFWwindow* window, int, int)
{
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application != nullptr) {
        application->framebufferResized_ = true;
    }
}

} // namespace zl::app
