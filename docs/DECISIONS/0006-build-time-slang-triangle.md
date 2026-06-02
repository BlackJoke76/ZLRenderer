# Decision 0006: Start M3 With Build-Time Slang Triangle

## Status

Accepted.

## Context

M3 needs to introduce Slang, shader modules, pipeline creation, a pipeline
cache, and the first graphics pass through the Render Graph. Doing all shader
reflection, descriptor binding, and pipeline-key abstraction at once would hide
the core learning step: Slang source becomes SPIR-V, SPIR-V becomes shader
modules, shader modules become a cached graphics pipeline, and the graph drives
the pass.

## Decision

Start M3 with a build-time Slang triangle.

CMake invokes `slangc` from the Vulkan SDK to compile
`shaders/triangle.slang` into:

```text
build/shaders/triangle.vert.spv
build/shaders/triangle.frag.spv
```

`VulkanDevice` loads those SPIR-V files, creates shader modules, creates an
empty pipeline layout, creates a `VkPipelineCache`, and creates one cached
triangle graphics pipeline during device/swapchain setup. The application now
adds a `Triangle` graphics pass to the Render Graph; the pass writes the
swapchain image as `RenderTarget` and records `vkCmdDraw(3)`.

The Slang vertex shader uses `SV_VertexID` instead of a vertex buffer. The
Vulkan device enables `shaderDrawParameters` because Slang emits the SPIR-V
`DrawParameters` capability for that built-in.

## Consequences

Positive:

- The renderer now has its first graphics pipeline through Render Graph.
- Slang is the shader source of truth from the first triangle.
- Pipeline creation happens during setup/recreation, not during draw.
- No descriptors, vertex buffers, or materials are needed for this learning
  slice.

Negative:

- Shader compilation is build-time only.
- The pipeline cache is not persisted to disk yet.
- `PipelineKey`, shader reflection, and bind group abstractions are still
  pending M3 work.

## Guardrails

- Do not add descriptor systems before a shader actually needs resources.
- Keep the triangle pipeline cached and out of the draw path.
- Add explicit `PipelineKey` and shader reflection only when a second pipeline
  or binding layout makes the need visible.
