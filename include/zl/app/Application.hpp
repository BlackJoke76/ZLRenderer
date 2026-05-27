#pragma once

#include <cstdint>
#include <memory>

struct GLFWwindow;

namespace zl::rhi::vulkan {
class VulkanDevice;
}

namespace zl::app {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run(std::uint32_t maxFrames = 0);

private:
    void initWindow();
    void initRenderer();
    void shutdown();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<rhi::vulkan::VulkanDevice> device_;
    bool framebufferResized_ = false;
};

} // namespace zl::app
