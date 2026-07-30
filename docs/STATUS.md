# Project Status

## Current State

- Local git has been initialized on `main`.
- M0 project skeleton is implemented.
- M1 RHI + frame loop + swapchain clear code is implemented.
- M2 Render Graph clear, compile-validation, and swapchain transition execution slices are implemented.
- M3 Slang triangle, build-time shader compiler boundary, first uniform-buffer descriptor, and device-local vertex-buffer slices are implemented.
- The renderer currently targets Windows, VS 2022 Community, MSVC x64, Vulkan SDK, GLFW, and Dear ImGui.
- GitHub sync is active at `https://github.com/BlackJoke76/ZLRenderer`.
- `main` contains the bootstrap root commit.
- `codex/bootstrap-renderer-contract` is pushed and tracks origin.
- Draft PR [#1](https://github.com/BlackJoke76/ZLRenderer/pull/1) contains the current M2 + M3 slices.

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
- Replaced the empty triangle pipeline layout with a `set = 0`, `binding = 0` fragment uniform-buffer layout.
- Added one persistently mapped, host-coherent triangle uniform buffer and descriptor set per frame slot; data is updated only after that slot's fence is signaled.
- Bind the current frame slot's descriptor set before the triangle draw; descriptor sets are allocated and updated once during device setup.
- Replaced shader-local triangle vertex arrays with a device-local vertex buffer and a one-time staging upload.
- Declared position and color attributes in the graphics pipeline, then bind the vertex buffer before the triangle draw.
- Removed the no-longer-needed `shaderDrawParameters` device feature requirement.
- Added first RHI shader stage and primitive topology names.
- Refactored triangle pipeline creation around an internal `GraphicsPipelineKey`.
- Wrapped temporary Vulkan shader modules with RAII during pipeline creation.
- Extracted `VulkanPipelineCache` as a backend module that owns the driver cache and cached Vulkan pipelines.
- Route triangle pipeline creation through `GraphicsPipelineKey -> VkPipeline` lookup.
- Clear cached pipelines before swapchain-dependent pipeline layouts and render passes are destroyed.
- Added a VSCode-openable binary semaphore submit-order example under `docs/NOTES`.
- Centralized build-time Slang compilation in `zl_compile_slang_shader`.
- Added backend-neutral `CompiledShaderLibrary` loading with focused success and failure tests.
- Removed SPIR-V filesystem parsing from `VulkanDevice`; Vulkan now consumes compiled shader records during cached pipeline creation.

## Next Step

1. Start M4 with a minimal CPU `TaskSystem`: fixed workers, task groups, dependencies, completion handles, stable worker indices, tests, and a deterministic single-thread fallback.
2. Make RenderScene gathering/culling and DrawList construction the first real task-system workload; keep Render Graph command recording serial in this slice.
3. Add an index-buffer slice only when the first DrawList caller needs indexed geometry.
4. Start M5 only after DrawList produces enough recording work: compile explicit recording batches, add per-frame/per-worker/per-queue-family command pools, and record independent command buffers in parallel while keeping barriers and submission ordering centralized.
5. Add secondary command buffers only for a measured large-pass draw workload; do not make CPU task granularity dictate GPU submission granularity.

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
- Passed: current triangle SPIR-V modules with `spirv-val`; the fragment module declares `DescriptorSet 0`, `Binding 0` for the uniform buffer.
- Passed: current Debug code linked as `zl_renderer_verify.exe` and ran `--frames 3` with Render Graph validation and no Vulkan validation output.
- Passed: current Debug build and `zl_renderer.exe --frames 3` after moving triangle data into the device-local vertex buffer.
- Passed: current vertex SPIR-V module with `spirv-val`; position and color inputs use Locations 0 and 1, matching the Vulkan vertex attributes.
- Passed: current Debug build, both CTest targets, `zl_renderer.exe --frames 3`, and both SPIR-V modules after extracting `CompiledShaderLibrary`.
- Note: Vulkan loader prints a duplicate Epic overlay layer warning; no app validation errors remain in the frame runs.

## Known Issues

- `cmake` is not on PATH; use the Visual Studio bundled CMake path above.
- Render Graph execution is currently linear and only emits runtime transitions for imported swapchain textures.
- The submit wait stage now matches the graphics triangle path. Future mixed transfer/graphics graph execution should derive it from the first real wait consumer.
- M3 compiles Slang at build time and loads artifacts through `CompiledShaderLibrary`. Runtime compilation remains deferred until hot reload or editor iteration has a caller; shader reflection is not implemented, so the Vulkan descriptor layout manually mirrors `triangle.slang`.
- The first uniform descriptor is deliberately triangle-specific. A general RHI descriptor API, material binding model, dynamic offsets, and per-draw descriptor updates are not implemented yet.
- The first vertex buffer is deliberately triangle-specific. There is no RHI buffer API, mesh object, index buffer, or streaming upload queue yet.
- The static vertex upload waits for the graphics queue during device initialization. Runtime streaming must use a frame-owned upload path instead.
- The current `GraphicsPipelineKey` is Vulkan-internal and covers shader identity, topology, pipeline layout, render pass, color format, and subpass.
- `VulkanPipelineCache` is intentionally single-threaded. Add immutable/mutable cache layers only when parallel command recording creates a measured synchronization need.
- No CPU task system exists yet. M4 introduces it for DrawList preparation before M5 connects it to Vulkan command recording.
- Parallel recording will require one externally owned command pool per recording worker, per frame-in-flight, per queue family; the current single command list cannot be shared across worker tasks.

## Important Decisions

- [0001: Build Render Graph First](DECISIONS/0001-render-graph-first.md)
- [0002: Add RHI And FrameContext Before Render Graph Execution](DECISIONS/0002-rhi-frame-context-first.md)
- [0003: Introduce Linear Render Graph Clear Slice](DECISIONS/0003-linear-render-graph-clear-slice.md)
- [0004: Compile Render Graph Declarations Before Emitting Barriers](DECISIONS/0004-compile-render-graph-before-barriers.md)
- [0005: Let Render Graph Drive Swapchain Transitions](DECISIONS/0005-render-graph-drives-swapchain-transitions.md)
- [0006: Start M3 With Build-Time Slang Triangle](DECISIONS/0006-build-time-slang-triangle.md)
- [0007: Name The First Pipeline Key Without Generalizing Descriptors](DECISIONS/0007-first-pipeline-key-without-descriptors.md)
- [0008: Extract A Single-Threaded Vulkan Pipeline Cache](DECISIONS/0008-single-threaded-vulkan-pipeline-cache.md)
- [0009: Stage Task System Before Parallel Command Recording](DECISIONS/0009-stage-task-system-before-parallel-recording.md)
- [0010: Use Frame-Local Uniform Descriptor Sets First](DECISIONS/0010-frame-local-uniform-descriptor-sets.md)
- [0011: Upload The First Vertex Buffer To Device-Local Memory](DECISIONS/0011-first-device-local-vertex-buffer.md)
- [0012: Separate Build-Time Compilation From Shader Loading](DECISIONS/0012-separate-build-time-compilation-from-shader-loading.md)
