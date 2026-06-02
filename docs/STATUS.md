# Project Status

## Current State

- Local git has been initialized on `main`.
- M0 project skeleton is implemented.
- M1 RHI + frame loop + swapchain clear code is implemented.
- M2 Render Graph clear, compile-validation, and swapchain transition execution slices are implemented.
- M3 Slang triangle first slice is implemented.
- The renderer currently targets Windows, VS 2022 Community, MSVC x64, Vulkan SDK, GLFW, and Dear ImGui.
- GitHub sync is active at `https://github.com/BlackJoke76/ZLRenderer`.
- `main` contains the bootstrap root commit.
- `codex/bootstrap-renderer-contract` is pushed and tracks origin.

## Last Completed

- Added CMake project skeleton.
- Added GLFW and Dear ImGui FetchContent setup.
- Added `zl::rhi` interface types.
- Added `VulkanDevice` as the first VulkanRHI implementation.
- Added `FrameContext` with frame index, swapchain image index, and frames-in-flight.
- Added GLFW application loop that clears the swapchain through `RHICommandList`.
- Fixed render-finished semaphore reuse by allocating it per swapchain image.
- Added `RenderGraph` with texture handles, pass handles, pass read/write declarations, linear execution callbacks, and a graph dump.
- Moved the application clear path behind a `ClearSwapchain` Render Graph pass.
- Added `RenderGraph::compile()` validation for declared texture usage.
- Added compile-time transition plan output to the graph dump.
- Added `zl_render_graph_tests` and a CTest entry for graph validation rules.
- Added an RHI swapchain transition hook driven by compiled Render Graph transitions.
- Removed hidden swapchain layout transitions from `clearSwapchainImage`; clear now assumes the graph transitioned to `TransferDst`.
- Import the active swapchain image with its tracked RHI state so the first frame can correctly start from `Undefined`.
- Added `shaders/triangle.slang` and CMake `slangc` compilation to SPIR-V.
- Added a swapchain render pass, framebuffers, Vulkan pipeline cache, empty pipeline layout, and cached triangle graphics pipeline.
- Added `RHICommandList::drawTriangleToSwapchain` and moved the application graph pass from transfer clear to graphics triangle.
- Enabled `shaderDrawParameters` because the Slang vertex shader uses `SV_VertexID`.
- Added first RHI shader stage and primitive topology names.
- Refactored triangle pipeline creation around an internal `GraphicsPipelineKey`.
- Wrapped temporary Vulkan shader modules with RAII during pipeline creation.
- Extracted `VulkanPipelineCache` as a backend module that owns the driver cache and cached Vulkan pipelines.
- Route triangle pipeline creation through `GraphicsPipelineKey -> VkPipeline` lookup.
- Clear cached pipelines before swapchain-dependent pipeline layouts and render passes are destroyed.

## Next Step

1. Open or update the bootstrap draft PR.
2. Add a small shader compiler interface or documented build-time shader compiler boundary before adding shader reflection.
3. Decide whether the next runnable slice should introduce a second pipeline or the first resource-binding need.
4. Generalize the swapchain-only transition hook into normal texture barriers when graph-owned textures appear.

## Build Command

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64
& $cmake --build build --config Debug
```

## Validation

- Passed: CMake configure with Visual Studio 17 2022 x64.
- Passed: Debug build.
- Passed: `ctest --test-dir build -C Debug --output-on-failure`.
- Passed: `.\build\Debug\zl_renderer.exe --frames 3`, including one Render Graph dump with compile validation and transition plan.
- Passed: `.\build\Debug\zl_renderer.exe --frames 6`.
- Passed: Slang build outputs `build/shaders/triangle.vert.spv` and `build/shaders/triangle.frag.spv`.
- Note: Vulkan loader prints a duplicate Epic overlay layer warning; no app validation errors remain in the frame runs.

## Known Issues

- `cmake` is not on PATH; use the Visual Studio bundled CMake path above.
- Render Graph execution is currently linear and only emits runtime transitions for imported swapchain textures.
- The submit wait stage now matches the graphics triangle path. Future mixed transfer/graphics graph execution should derive it from the first real wait consumer.
- M3 currently compiles Slang at build time through CMake. A runtime `IShaderCompiler` boundary and shader reflection are not implemented yet.
- The current `GraphicsPipelineKey` is Vulkan-internal and covers shader identity, topology, pipeline layout, render pass, color format, and subpass.
- `VulkanPipelineCache` is intentionally single-threaded. Add immutable/mutable cache layers only when parallel command recording creates a measured synchronization need.

## Important Decisions

- [0001: Build Render Graph First](DECISIONS/0001-render-graph-first.md)
- [0002: Add RHI And FrameContext Before Render Graph Execution](DECISIONS/0002-rhi-frame-context-first.md)
- [0003: Introduce Linear Render Graph Clear Slice](DECISIONS/0003-linear-render-graph-clear-slice.md)
- [0004: Compile Render Graph Declarations Before Emitting Barriers](DECISIONS/0004-compile-render-graph-before-barriers.md)
- [0005: Let Render Graph Drive Swapchain Transitions](DECISIONS/0005-render-graph-drives-swapchain-transitions.md)
- [0006: Start M3 With Build-Time Slang Triangle](DECISIONS/0006-build-time-slang-triangle.md)
- [0007: Name The First Pipeline Key Without Generalizing Descriptors](DECISIONS/0007-first-pipeline-key-without-descriptors.md)
- [0008: Extract A Single-Threaded Vulkan Pipeline Cache](DECISIONS/0008-single-threaded-vulkan-pipeline-cache.md)
