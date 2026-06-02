#include "zl/render_graph/RenderGraph.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

class RecordingCommandList final : public zl::rhi::CommandList {
public:
    explicit RecordingCommandList(std::vector<std::string>& events)
        : events_(events)
    {
    }

    void transitionSwapchainImage(zl::rhi::ResourceState state) override
    {
        transitions.push_back(state);
        events_.push_back("transition");
    }

    void clearSwapchainImage(float, float, float, float) override
    {
        ++clearCount;
        events_.push_back("clear");
    }

    void drawTriangleToSwapchain() override
    {
        ++drawTriangleCount;
        events_.push_back("triangle");
    }

    std::vector<zl::rhi::ResourceState> transitions;
    int clearCount = 0;
    int drawTriangleCount = 0;

private:
    std::vector<std::string>& events_;
};

void noOpExecute(zl::rhi::CommandList&, const zl::rhi::FrameContext&)
{
}

bool expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool containsError(const zl::render_graph::RenderGraph::CompileResult& result, std::string_view text)
{
    for (const auto& error : result.errors) {
        if (error.find(text) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool validClearGraphCompiles()
{
    zl::render_graph::RenderGraph graph;
    const auto swapchain = graph.importSwapchainTexture(
        "SwapchainImage",
        zl::rhi::ResourceState::Present,
        zl::rhi::ResourceState::Present);
    graph.addPass(
        "ClearSwapchain",
        zl::render_graph::PassType::Transfer,
        [swapchain](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.writeTexture(swapchain, zl::rhi::ResourceState::TransferDst);
        },
        noOpExecute);

    const auto result = graph.compile();
    bool ok = true;
    ok = expect(result.succeeded(), "valid clear graph should compile") && ok;
    ok = expect(result.transitions.size() == 2, "clear graph should expose two transitions") && ok;
    if (result.transitions.size() == 2) {
        ok = expect(result.transitions[0].before == zl::rhi::ResourceState::Present, "first transition starts from Present") && ok;
        ok = expect(result.transitions[0].after == zl::rhi::ResourceState::TransferDst, "first transition goes to TransferDst") && ok;
        ok = expect(!result.transitions[0].finalTransition, "first transition belongs to the clear pass") && ok;
        ok = expect(result.transitions[1].before == zl::rhi::ResourceState::TransferDst, "final transition starts from TransferDst") && ok;
        ok = expect(result.transitions[1].after == zl::rhi::ResourceState::Present, "final transition returns to Present") && ok;
        ok = expect(result.transitions[1].finalTransition, "second transition is the final graph transition") && ok;
    }

    return ok;
}

bool readBeforeWriteFails()
{
    zl::render_graph::RenderGraph graph;
    const auto texture = graph.importSwapchainTexture(
        "UninitializedTexture",
        zl::rhi::ResourceState::Undefined,
        zl::rhi::ResourceState::Undefined);
    graph.addPass(
        "SampleTexture",
        zl::render_graph::PassType::Graphics,
        [texture](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.readTexture(texture, zl::rhi::ResourceState::ShaderRead);
        },
        noOpExecute);

    const auto result = graph.compile();
    bool ok = true;
    ok = expect(!result.succeeded(), "read-before-write graph should fail") && ok;
    ok = expect(containsError(result, "before any pass writes it"), "read-before-write error should explain ownership") && ok;
    return ok;
}

bool conflictingPassAccessFails()
{
    zl::render_graph::RenderGraph graph;
    const auto texture = graph.importSwapchainTexture(
        "HistoryTexture",
        zl::rhi::ResourceState::ShaderRead,
        zl::rhi::ResourceState::ShaderRead);
    graph.addPass(
        "ReadModifyTexture",
        zl::render_graph::PassType::Graphics,
        [texture](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.readTexture(texture, zl::rhi::ResourceState::ShaderRead);
            builder.writeTexture(texture, zl::rhi::ResourceState::RenderTarget);
        },
        noOpExecute);

    const auto result = graph.compile();
    bool ok = true;
    ok = expect(!result.succeeded(), "conflicting same-pass texture access should fail") && ok;
    ok = expect(containsError(result, "conflicting accesses"), "conflict error should name the access problem") && ok;
    return ok;
}

bool undefinedAccessStateFails()
{
    zl::render_graph::RenderGraph graph;
    const auto texture = graph.importSwapchainTexture(
        "SwapchainImage",
        zl::rhi::ResourceState::Present,
        zl::rhi::ResourceState::Present);
    graph.addPass(
        "MissingState",
        zl::render_graph::PassType::Transfer,
        [texture](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.writeTexture(texture, zl::rhi::ResourceState::Undefined);
        },
        noOpExecute);

    const auto result = graph.compile();
    bool ok = true;
    ok = expect(!result.succeeded(), "undefined access state should fail") && ok;
    ok = expect(containsError(result, "Undefined state"), "undefined-state error should be explicit") && ok;
    return ok;
}

bool executeClearGraphRunsTransitionsAroundPass()
{
    zl::render_graph::RenderGraph graph;
    const auto swapchain = graph.importSwapchainTexture(
        "SwapchainImage",
        zl::rhi::ResourceState::Present,
        zl::rhi::ResourceState::Present);
    graph.addPass(
        "ClearSwapchain",
        zl::render_graph::PassType::Transfer,
        [swapchain](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.writeTexture(swapchain, zl::rhi::ResourceState::TransferDst);
        },
        [](zl::rhi::CommandList& commandList, const zl::rhi::FrameContext&) {
            commandList.clearSwapchainImage(0.03f, 0.06f, 0.09f, 1.0f);
        });

    std::vector<std::string> events;
    RecordingCommandList commandList(events);
    graph.execute(commandList, zl::rhi::FrameContext{});

    bool ok = true;
    ok = expect(commandList.clearCount == 1, "clear pass should execute exactly once") && ok;
    ok = expect(commandList.transitions.size() == 2, "execute should submit two swapchain transitions") && ok;
    if (commandList.transitions.size() == 2) {
        ok = expect(commandList.transitions[0] == zl::rhi::ResourceState::TransferDst, "first runtime transition targets TransferDst") && ok;
        ok = expect(commandList.transitions[1] == zl::rhi::ResourceState::Present, "final runtime transition targets Present") && ok;
    }
    ok = expect(events.size() == 3, "runtime events should contain transition, clear, transition") && ok;
    if (events.size() == 3) {
        ok = expect(events[0] == "transition", "first runtime event should be the pass transition") && ok;
        ok = expect(events[1] == "clear", "second runtime event should be clear") && ok;
        ok = expect(events[2] == "transition", "third runtime event should be the final transition") && ok;
    }

    return ok;
}

bool executeTriangleGraphRunsRenderTargetTransitions()
{
    zl::render_graph::RenderGraph graph;
    const auto swapchain = graph.importSwapchainTexture(
        "SwapchainImage",
        zl::rhi::ResourceState::Undefined,
        zl::rhi::ResourceState::Present);
    graph.addPass(
        "Triangle",
        zl::render_graph::PassType::Graphics,
        [swapchain](zl::render_graph::RenderGraph::PassBuilder& builder) {
            builder.writeTexture(swapchain, zl::rhi::ResourceState::RenderTarget);
        },
        [](zl::rhi::CommandList& commandList, const zl::rhi::FrameContext&) {
            commandList.drawTriangleToSwapchain();
        });

    std::vector<std::string> events;
    RecordingCommandList commandList(events);
    graph.execute(commandList, zl::rhi::FrameContext{});

    bool ok = true;
    ok = expect(commandList.drawTriangleCount == 1, "triangle pass should execute exactly once") && ok;
    ok = expect(commandList.transitions.size() == 2, "triangle graph should submit two swapchain transitions") && ok;
    if (commandList.transitions.size() == 2) {
        ok = expect(commandList.transitions[0] == zl::rhi::ResourceState::RenderTarget, "first triangle transition targets RenderTarget") && ok;
        ok = expect(commandList.transitions[1] == zl::rhi::ResourceState::Present, "final triangle transition targets Present") && ok;
    }
    ok = expect(events.size() == 3, "triangle events should contain transition, triangle, transition") && ok;
    if (events.size() == 3) {
        ok = expect(events[0] == "transition", "first triangle event should be the pass transition") && ok;
        ok = expect(events[1] == "triangle", "second triangle event should draw") && ok;
        ok = expect(events[2] == "transition", "third triangle event should be the final transition") && ok;
    }

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok = validClearGraphCompiles() && ok;
    ok = readBeforeWriteFails() && ok;
    ok = conflictingPassAccessFails() && ok;
    ok = undefinedAccessStateFails() && ok;
    ok = executeClearGraphRunsTransitionsAroundPass() && ok;
    ok = executeTriangleGraphRunsRenderTargetTransitions() && ok;

    return ok ? 0 : 1;
}
