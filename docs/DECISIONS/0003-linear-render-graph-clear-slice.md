# Decision 0003: Introduce Linear Render Graph Clear Slice

## Status

Accepted.

## Context

M1 cleared the swapchain directly from application code through
`RHICommandList`. That proved the Vulkan frame loop, but the frame dependency
was still hidden as an application call instead of being visible as a renderer
pass.

M2 needs the smallest useful Render Graph step: resource names, pass handles,
declared resource usage, execution through RHI, and a readable dump.

## Decision

Add a linear `RenderGraph` that owns texture handles, pass handles, pass
read/write declarations, and pass execution callbacks.

The application now imports `SwapchainImage`, adds a `ClearSwapchain` transfer
pass, declares the swapchain write as `TransferDst`, dumps the graph once, and
executes the pass through `rhi::CommandList`.

Vulkan handles stay inside `VulkanDevice`. Swapchain layout transitions remain
inside `RHICommandList::clearSwapchainImage` until the graph has an explicit
compile step or resource-state tracker that can emit RHI barrier calls.

## Consequences

Positive:

- The first visible frame is now produced through Render Graph execution.
- Pass/resource dependencies are visible in code and in the dump.
- The graph remains small enough to inspect while learning.
- The next state-tracking step has a concrete producer/consumer declaration to
  build on.

Negative:

- Graph declarations and actual Vulkan transitions are not unified yet.
- The graph is rebuilt every frame and executes linearly.
- The current dump is console-only until ImGui debug UI exists.

## Guardrails

- Keep Render Graph v0 linear until multiple passes require scheduling.
- Do not expose Vulkan image handles through the graph.
- Do not add transient aliasing or async compute before real resource lifetime
  pressure exists.
- Move barriers into a graph compile/state-tracker layer before adding
  multi-pass techniques that depend on image layout correctness.
