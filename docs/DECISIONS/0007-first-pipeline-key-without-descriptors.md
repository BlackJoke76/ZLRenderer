# Decision 0007: Name The First Pipeline Key Without Generalizing Descriptors

## Status

Accepted.

## Context

The build-time Slang triangle introduced a real graphics pipeline. The first
implementation created the Vulkan shader modules, pipeline layout, pipeline
cache, and graphics pipeline directly inside `createTrianglePipeline`.

M3 needs the owner to see the pipeline-cache shape, but the shader still has no
descriptors, vertex buffers, push constants, or material state. A full pipeline
database would be speculative.

## Decision

Introduce the smallest useful pipeline vocabulary now:

- `ShaderStage` and `PrimitiveTopology` names in the RHI layer;
- a Vulkan-internal `ShaderModuleDesc` for the current SPIR-V files;
- a Vulkan-internal `GraphicsPipelineKey` for topology, color format, render
  pass, and subpass;
- RAII cleanup for temporary Vulkan shader modules during pipeline creation.

The key is intentionally local to `VulkanDevice` until a second graphics
pipeline or shader layout makes a broader cache interface worthwhile.

## Consequences

Positive:

- Pipeline creation now has an explicit key shape.
- Temporary shader modules are cleaned up if pipeline creation fails.
- The code names the M3 concepts without pretending descriptors exist.

Negative:

- The key is not hashable or stored in a general map yet.
- Pipeline cache data is not persisted to disk.
- Shader reflection and bind group layout generation are still future work.

## Guardrails

- Add descriptor-related key fields only when a shader uses descriptors.
- Add a general pipeline-cache lookup only when there is more than one pipeline
  or a real cache miss path.
- Keep pipeline creation out of pass execution and draw recording.
