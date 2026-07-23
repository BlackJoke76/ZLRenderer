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

## Current M2 Flow

M2 now builds a small Render Graph each frame:

```text
beginFrame
import SwapchainImage as graph texture with tracked active state
add ClearSwapchain transfer pass
declare SwapchainImage write as TransferDst
compile graph declarations and validate resource usage
execute graph transitions and pass callbacks through RHICommandList
submit
present
endFrame
```

The Render Graph owns pass/resource declarations, compile-time validation,
transition planning, and linear pass execution. `VulkanDevice` still owns Vulkan
handles. The application imports the active swapchain image with the state
tracked by `VulkanDevice`, so a first-use image can start from `Undefined` while
a previously presented image can start from `Present`.

`RenderGraph::execute` now applies compiled swapchain transitions through
`RHICommandList::transitionSwapchainImage` before the consuming pass and after
the final pass. `RHICommandList::clearSwapchainImage` only records the clear and
expects the graph to have transitioned the active swapchain image to
`TransferDst`.

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

The first Render Graph solves only:

- resource naming;
- pass read/write declarations;
- declared resource usage validation;
- a compile-time transition plan;
- runtime transition execution for imported swapchain textures;
- graph execution through `rhi::CommandList`;
- graph dump for learning.

The current graph is linear and declaration-first. It compiles the intended
state transitions for learning and validation, then emits the swapchain
transitions through RHI. General texture barriers are still future work because
the renderer does not yet allocate graph-owned textures.

It should not start with transient aliasing, async compute scheduling,
automatic pass merging, bindless, or multiple RHI backends.

## Current M3 Flow

M3 starts with a build-time Slang triangle:

```text
CMake
    -> slangc shaders/triangle.slang
        -> build/shaders/triangle.vert.spv
        -> build/shaders/triangle.frag.spv

Application frame
    -> import active SwapchainImage
    -> add Triangle graphics pass
    -> graph transitions SwapchainImage to RenderTarget
    -> RHICommandList.drawTriangleToSwapchain
        -> begin swapchain render pass
        -> bind cached triangle graphics pipeline
        -> vkCmdDraw(3)
        -> end render pass
    -> graph transitions SwapchainImage to Present
```

The triangle uses `SV_VertexID` in Slang and does not use vertex buffers,
descriptors, push constants, or material state. `VulkanDevice` enables
`shaderDrawParameters` because Slang emits the SPIR-V `DrawParameters`
capability for this built-in.

The first pipeline cache is `VulkanPipelineCache`, a small Vulkan-backend
module. It owns the driver `VkPipelineCache` and a
`GraphicsPipelineKey -> VkPipeline` map. The triangle pipeline is created during
device/swapchain setup and recreated on swapchain recreation because it depends
on the swapchain render pass. Pipeline creation is not on the draw path.

The current cache is intentionally single-threaded. The renderer does not have
parallel command recording yet, so immutable/mutable cache layers would add
coordination machinery without removing a current bottleneck.

## Shader, Pipeline, And Binding Direction

Slang is the shader language.

M3 introduces these concepts in small slices:

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

Current M3 status:

- build-time Slang compilation exists through CMake and `slangc`;
- a first `ShaderModule` concept exists as Vulkan shader module creation inside
  `VulkanDevice`, with temporary shader modules cleaned up by RAII;
- a first pipeline cache exists as the Vulkan-backend `VulkanPipelineCache`
  module;
- a first Vulkan-internal `GraphicsPipelineKey` exists for the triangle
  pipeline's shader identity, topology, pipeline layout, render pass, color
  format, and subpass;
- runtime `IShaderCompiler`, shader reflection, persistent pipeline cache data,
  and bind groups are still pending.

Descriptor/binding convention:

```text
set 0: frame/view global data
set 1: material resources
set 2: object or dynamic data
```

Pipeline creation must not happen repeatedly in the draw hot path.

## Task System And Parallel Recording Direction

The CPU task system is renderer infrastructure, not part of VulkanRHI. It owns
worker threads and CPU task dependencies, but it does not own Vulkan handles,
resource states, barriers, or queue submissions.

The first task-system use belongs in M4:

```text
RenderScene snapshot
    -> parallel scene gathering and culling tasks
        -> deterministic DrawList merge
            -> RenderGraph construction and compile
```

The initial task surface should stay small:

```text
TaskSystem
TaskGroup
TaskHandle
WorkerIndex
```

It needs fixed workers, task-group dependencies, completion handles, stable
worker indices, and a deterministic single-thread fallback. It does not start
with coroutines, work stealing, background priorities, or Vulkan-specific task
types. A small composer-style helper may later express prepare -> record ->
submit stages, but it remains a thin client of `TaskSystem`.

Parallel command recording is a separate M5 integration step:

```text
RenderGraph compile on the controlling thread
    -> resolve resource states, barriers, and recording batches
        -> record independent command buffers on worker threads
            -> order and batch command buffers on the controlling thread
                -> submit and present through RHI
```

Vulkan recording resources follow explicit ownership:

```text
FrameResources[frame]
    -> QueueFamilyResources[queue]
        -> WorkerCommandPool[worker]
            -> reusable primary and optional secondary command buffers
```

Each command pool belongs to one recording worker for one frame-in-flight and
one queue family. A pool is reset only after the frame fence proves that its
command buffers are no longer pending. Worker tasks never share a mutable
`RHICommandList` or independently decide image layouts and barriers.

The first parallel recording slice uses coarse, independent physical passes and
primary command buffers. Secondary command buffers are reserved for a later
case where one large graphics pass contains enough draw work to amortize their
inheritance and execution overhead. CPU task granularity must not force one GPU
queue submission per task.

`VulkanPipelineCache` remains single-threaded until parallel recording creates
a real concurrent lookup requirement. Existing pipelines should be prepared
before recording; background pipeline compilation and immutable/mutable cache
layers are separate future decisions.

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
