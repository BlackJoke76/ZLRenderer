# Project Status

## Current State

- Local git has been initialized on `main`.
- M0 project skeleton is implemented.
- M1 RHI + frame loop + swapchain clear code is implemented.
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

## Next Step

1. Open the bootstrap draft PR.
2. Start M2: move the clear pass behind Render Graph.
3. Add graph resource/pass handles and graph dump.

## Build Command

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64
& $cmake --build build --config Debug
```

## Validation

- Passed: CMake configure with Visual Studio 17 2022 x64.
- Passed: Debug build.
- Passed: `.\build\Debug\zl_renderer.exe --frames 3`.
- Note: Vulkan loader prints a duplicate Epic overlay layer warning; no app validation errors remain in the 3-frame run.

## Known Issues

- `cmake` is not on PATH; use the Visual Studio bundled CMake path above.
- M1 still uses direct RHI clear. M2 will move clear into Render Graph.

## Important Decisions

- [0001: Build Render Graph First](DECISIONS/0001-render-graph-first.md)
- [0002: Add RHI And FrameContext Before Render Graph Execution](DECISIONS/0002-rhi-frame-context-first.md)
