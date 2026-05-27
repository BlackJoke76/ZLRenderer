# Renderer Architecture

## High-Level Shape

```text
Application / Platform
    -> Demo World / Game World
        -> RenderScene
            -> Renderer Frontend
                -> RenderGraph
                    -> RHI
                        -> VulkanRHI
```

## Layer Roles

Application / Platform owns the window, input, and main loop.

Demo World creates simple learning scenes.

RenderScene stores renderer-facing scene data such as camera/view, mesh
instances, material instances, lights, and later draw lists.

Renderer Frontend gathers frame data and builds the Render Graph.

Render Graph declares pass/resource reads and writes, derives dependencies,
emits barriers/layout transitions, and executes through RHI command lists.

RHI defines GPU resource, shader, pipeline, binding, command list, and swapchain
interfaces. It is intentionally small and Vulkan-only for now.

VulkanRHI owns Vulkan handles and implements the RHI.

## Current M1 Flow

M1 intentionally clears through RHI before Render Graph exists:

```text
beginFrame
acquireSwapchainImage
reset per-frame command pool
begin command buffer
RHICommandList.clearSwapchainImage
submit
present
endFrame
```

M2 moves `clearSwapchainImage` behind a Render Graph clear pass.

Render-finished semaphores are indexed by swapchain image, not by frame slot.
The present operation may still own a semaphore until that image is acquired
again, so reusing a frame-slot render-finished semaphore can trip Vulkan
validation.

## RHI Direction

The first RHI surface is:

```text
RHIDevice
RHISwapchain
RHICommandList
RHITexture
RHIBuffer
RHIShader
RHIPipeline
RHIBindGroupLayout
RHIBindGroup
```

Only `Device`, `CommandList`, handle stubs, `ResourceState`, and `FrameContext`
exist in M1. More interfaces are added when the next milestone uses them.

## Render Graph Direction

The first Render Graph should solve only:

- resource naming;
- pass read/write declarations;
- conservative state transitions;
- graph execution through `rhi::CommandList`;
- graph dump for learning.

It should not start with transient aliasing, async compute scheduling, automatic
pass merging, bindless, or multiple RHI backends.

## Shader, Pipeline, And Binding Direction

Slang is the shader language.

M3 introduces:

```text
IShaderCompiler
SlangcShaderCompiler
ShaderModule
ShaderReflection placeholder
PipelineLayout
PipelineKey
PipelineCache
BindGroupLayout
BindGroup
```

Descriptor/binding convention:

```text
set 0: frame/view global data
set 1: material resources
set 2: object or dynamic data
```

Pipeline creation must not happen repeatedly in the draw hot path.

## Long-Term Project Memory

Chat context is not project memory. The durable state is:

```text
AGENTS.md
docs/STATUS.md
docs/ROADMAP.md
docs/ARCHITECTURE.md
docs/DECISIONS/
docs/NOTES/
git history
```

Each task should update `docs/STATUS.md`. Architecture changes should update
this file and add a decision note.
