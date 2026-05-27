#pragma once

#include <cstdint>

namespace zl::rhi {

class Device;
class Swapchain;
class CommandList;

struct TextureHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

struct BufferHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

struct ShaderHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

struct PipelineHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

struct BindGroupHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

enum class ResourceState {
    Undefined,
    RenderTarget,
    DepthWrite,
    ShaderRead,
    ShaderWrite,
    TransferSrc,
    TransferDst,
    Present
};

struct FrameContext {
    std::uint32_t frameIndex = 0;
    std::uint32_t swapchainImageIndex = 0;
    std::uint32_t framesInFlight = 0;
};

class CommandList {
public:
    virtual ~CommandList() = default;

    virtual void clearSwapchainImage(float red, float green, float blue, float alpha) = 0;
};

class Device {
public:
    virtual ~Device() = default;

    virtual bool beginFrame() = 0;
    virtual CommandList& commandList() = 0;
    virtual const FrameContext& frameContext() const = 0;
    virtual void endFrame() = 0;
    virtual void waitIdle() = 0;
};

} // namespace zl::rhi
