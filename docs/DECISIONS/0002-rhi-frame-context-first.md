# Decision 0002: Add RHI And FrameContext Before Render Graph Execution

## Status

Accepted.

## Context

The renderer is meant to teach commercial game engine structure, not only Vulkan
API calls. A Render Graph should not talk directly to Vulkan handles, and a
future RenderScene needs a stable frame loop to feed it.

## Decision

Implement M1 as a small RHI and FrameContext slice before moving clear into the
Render Graph.

The first backend is Vulkan-only. The RHI exists to create a readable boundary,
not to support multiple graphics APIs immediately.

## Consequences

Positive:

- Application code does not own Vulkan handles.
- Frame synchronization and command-pool reset have one home.
- Swapchain-image render-finished semaphores make presentation ownership
  explicit.
- Render Graph can later execute through `RHICommandList`.
- The architecture is closer to commercial engine renderers.

Negative:

- The first Render Graph pass arrives one milestone later.
- The early RHI must stay small to avoid fake portability abstractions.

## Guardrails

- Add RHI methods only when the current milestone uses them.
- Keep Vulkan-specific capability checks in `VulkanRHI`.
- Do not add D3D12 or Metal backend abstractions in M1.
