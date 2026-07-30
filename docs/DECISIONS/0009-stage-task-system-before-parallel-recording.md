# Decision 0009: Stage Task System Before Parallel Command Recording

## Status

Accepted.

## Context

The renderer currently rebuilds and executes a linear Render Graph into one
primary command buffer. M4 will add RenderScene, culling, materials, and a
DrawList, while later pass chains will create enough independent CPU work to
benefit from parallel preparation and command recording.

A Granite-style `TaskComposer` is useful for expressing pipelined CPU stages,
but it does not by itself make Vulkan command recording thread-safe. Vulkan
command pools require external synchronization, graph barriers must follow
resource dependencies, and CPU task granularity should not become GPU
submission granularity.

## Decision

Introduce concurrency in two runnable slices.

M4 adds a Vulkan-independent CPU `TaskSystem` with:

- fixed worker threads and stable worker indices;
- task groups and explicit group dependencies;
- waitable completion handles;
- a deterministic single-thread fallback;
- RenderScene gathering, culling, and DrawList construction as its first real
  workload.

M5 integrates the task system with Render Graph execution:

- Render Graph compile derives coarse recording batches from pass/resource
  dependencies;
- resource-state resolution, barriers, ordering, submission, and presentation
  remain centralized;
- each recording worker owns one command pool per frame-in-flight and queue
  family;
- independent physical passes may record primary command buffers in parallel;
- compatible command buffers are submitted in coarse graph-ordered batches;
- secondary command buffers are added only for a measured large graphics pass.

A composer-style stage helper may be added after the first task-system caller
shows the required prepare -> record -> submit shape. It remains independent of
Vulkan types.

## Consequences

Positive:

- The task system has a useful CPU workload before Vulkan concurrency is added.
- Vulkan command-pool ownership and frame lifetime remain visible.
- Render Graph remains the source of resource and execution dependencies.
- Serial and parallel recording paths can be compared with the same graph.

Negative:

- Parallel command recording arrives one milestone after the task system.
- DrawList merging must be deterministic across worker counts.
- Pipeline and descriptor lookup may later need explicit read-only concurrent
  paths when recording actually becomes parallel.

## Guardrails

- Do not share one mutable `RHICommandList` or `VkCommandPool` across workers.
- Do not let worker tasks choose barriers or final submission order.
- Do not allocate and free command buffers per pass in the hot path; reuse them
  through frame-owned command pools.
- Do not submit one command buffer per CPU task by default.
- Do not add work stealing, coroutines, priorities, or background pipeline
  compilation before a measured caller requires them.
- Keep the serial path available for validation and performance comparison.
