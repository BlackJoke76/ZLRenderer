#pragma once

#include "zl/rhi/RHI.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace zl::render_graph {

struct TextureHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

struct PassHandle {
    static constexpr std::uint32_t invalidIndex = 0xffffffffu;

    std::uint32_t index = invalidIndex;
};

enum class PassType {
    Graphics,
    Compute,
    Transfer
};

class RenderGraph {
public:
    using ExecuteCallback = std::function<void(rhi::CommandList&, const rhi::FrameContext&)>;

    struct CompileTransition {
        TextureHandle texture;
        PassHandle pass;
        rhi::ResourceState before = rhi::ResourceState::Undefined;
        rhi::ResourceState after = rhi::ResourceState::Undefined;
        bool finalTransition = false;
    };

    struct CompileResult {
        std::vector<std::string> errors;
        std::vector<CompileTransition> transitions;

        bool succeeded() const;
    };

    class PassBuilder {
    public:
        PassBuilder(RenderGraph& graph, PassHandle pass);

        void readTexture(TextureHandle texture, rhi::ResourceState state);
        void writeTexture(TextureHandle texture, rhi::ResourceState state);

    private:
        RenderGraph& graph_;
        PassHandle pass_;
    };

    TextureHandle importSwapchainTexture(
        std::string name,
        rhi::ResourceState initialState,
        rhi::ResourceState finalState);

    PassHandle addPass(
        std::string name,
        PassType type,
        const std::function<void(PassBuilder&)>& build,
        ExecuteCallback execute);

    CompileResult compile() const;
    void execute(rhi::CommandList& commandList, const rhi::FrameContext& frameContext) const;
    std::string dump() const;
    void clear();

private:
    enum class TextureKind {
        Swapchain
    };

    struct TextureResource {
        std::string name;
        TextureKind kind = TextureKind::Swapchain;
        bool imported = false;
        rhi::ResourceState initialState = rhi::ResourceState::Undefined;
        rhi::ResourceState finalState = rhi::ResourceState::Undefined;
    };

    struct TextureAccess {
        TextureHandle texture;
        rhi::ResourceState state = rhi::ResourceState::Undefined;
        bool write = false;
    };

    struct Pass {
        std::string name;
        PassType type = PassType::Graphics;
        std::vector<TextureAccess> textureAccesses;
        ExecuteCallback execute;
    };

    void addTextureAccess(
        PassHandle pass,
        TextureHandle texture,
        rhi::ResourceState state,
        bool write);
    void executeTransition(rhi::CommandList& commandList, const CompileTransition& transition) const;

    bool valid(TextureHandle texture) const;
    bool valid(PassHandle pass) const;

    std::vector<TextureResource> textures_;
    std::vector<Pass> passes_;
};

} // namespace zl::render_graph
