# Decision 0008: Extract A Single-Threaded Vulkan Pipeline Cache

## Status

Accepted.

## Context

The first triangle pipeline used a `VkPipelineCache` directly inside
`VulkanDevice`. M3 now needs a visible `PipelineKey -> VkPipeline` lookup shape
without turning one triangle into a production-scale pipeline database.

The renderer does not record command buffers in parallel yet. Adding immutable
and mutable cache layers now would introduce synchronization and merge behavior
before the code has a contention problem.

## Decision

Extract `VulkanPipelineCache` as a Vulkan-backend module.

The module owns:

- the driver `VkPipelineCache`;
- a single-threaded `GraphicsPipelineKey -> VkPipeline` map;
- pipeline destruction when swapchain-dependent resources are rebuilt or the
  device shuts down.

The key currently includes shader identity, topology, pipeline layout, render
pass, color format, and subpass. `VulkanDevice` remains responsible for
describing and creating the triangle pipeline on a cache miss.

## Consequences

Positive:

- Pipeline lookup and pipeline ownership have one backend home.
- `VulkanDevice` no longer directly owns the driver pipeline cache.
- Cached pipelines are destroyed before dependent pipeline layouts and render
  passes.
- The structure can grow when a second pipeline or parallel recording makes a
  new requirement visible.

Negative:

- The cache is single-threaded.
- Driver pipeline cache data is not persisted to disk.
- The cache is cleared during swapchain recreation because the first pipeline
  depends on the swapchain render pass.

## Guardrails

- Do not add immutable/mutable cache layers before parallel command recording
  exists.
- Do not compile pipelines from the draw path.
- Extend `GraphicsPipelineKey` whenever a new pipeline variant introduces a
  state dimension that affects compatibility.
- Keep Vulkan handles inside the Vulkan backend.
