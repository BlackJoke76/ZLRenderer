# Decision 0004: Compile Render Graph Declarations Before Emitting Barriers

## Status

Accepted.

## Context

The first M2 Render Graph moved swapchain clear behind a pass callback, but the
declared texture state and the actual Vulkan layout transitions were still two
separate ideas. Jumping straight to Vulkan barrier emission would make it harder
to see whether the graph declarations were correct.

## Decision

Add a `RenderGraph::compile()` step before adding RHI barrier emission.

The compile step validates declared texture usage and produces a transition
plan. Execution now refuses to run a graph with invalid declarations. The graph
dump includes validation status and the planned transitions so the learning path
can show producer/consumer state changes before hiding them behind Vulkan calls.

The first validation rules are intentionally small:

- a pass must declare texture accesses;
- an access must use a concrete state instead of `Undefined`;
- a pass cannot declare conflicting accesses to the same texture;
- a pass cannot read a texture before any pass writes it or imports defined
  contents.

## Consequences

Positive:

- The swapchain clear graph now visibly compiles to
  `Present -> TransferDst -> Present`.
- Bad graph declarations fail before command execution.
- Render Graph tests can run without creating a Vulkan device or window.
- Barrier emission has a concrete plan to consume in the next RHI step.

Negative:

- The graph is compiled each time it is dumped or executed.
- Transition planning and actual Vulkan transitions are temporarily duplicated.
- The validation rules are conservative and may need relaxing when true
  read-modify or subpass-style behavior exists.

## Guardrails

- Keep compile output readable enough to teach resource flow.
- Do not add scheduling, aliasing, or async compute while the clear graph only
  needs validation and a linear transition plan.
- Do not let graph tests depend on Vulkan runtime state.
