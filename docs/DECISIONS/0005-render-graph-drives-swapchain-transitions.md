# Decision 0005: Let Render Graph Drive Swapchain Transitions

## Status

Accepted.

## Context

After `RenderGraph::compile()` was added, the graph could describe the intended
swapchain transitions, but `RHICommandList::clearSwapchainImage` still performed
the Vulkan layout transitions internally. That kept the visible dependency graph
and the executed barriers partly separate.

The renderer only has one real image resource today: the active swapchain image.
A generic texture barrier system would be premature before graph-owned textures
exist.

## Decision

Add a swapchain-specific RHI transition hook:

```text
RHICommandList::transitionSwapchainImage(ResourceState)
```

`RenderGraph::execute` now applies compiled transitions through that hook before
the pass that needs the state and after all passes for final-state transitions.
`RHICommandList::clearSwapchainImage` no longer performs hidden layout
transitions; it assumes the active swapchain image is already in
`TransferDst`.

The application imports the active swapchain image using the tracked state from
the RHI device. This keeps the first frame honest: a newly created swapchain
image starts from `Undefined`, not from `Present`.

## Consequences

Positive:

- The graph declaration, compile plan, and executed Vulkan barrier now line up
  for swapchain clear.
- The first frame's `Undefined -> TransferDst -> Present` flow is visible.
- The clear helper no longer hides synchronization behavior.
- The RHI still keeps Vulkan handles private.

Negative:

- The transition hook is swapchain-only.
- The submit wait stage is still tuned for the transfer clear path.
- General graph-owned texture barriers still need a later RHI surface.

## Guardrails

- Do not expose Vulkan images to the Render Graph.
- Keep this hook swapchain-specific until a milestone introduces graph-owned
  textures.
- Revisit submit wait stages when the first graphics pipeline pass replaces or
  follows the transfer clear path.
