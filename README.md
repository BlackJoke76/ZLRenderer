# ZLRenderer

ZLRenderer is a learning-focused Vulkan renderer. The goal is to expose the
engine concepts behind commercial renderers: RHI, frame context, render graph,
resource lifetime, synchronization, shader/pipeline binding, and later GAMES202
techniques.

## Build

The current Windows setup uses Visual Studio 2022 Community, the bundled Visual
Studio CMake, and the Vulkan SDK.

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64
& $cmake --build build --config Debug
```

Run:

```powershell
.\build\Debug\zl_renderer.exe
```

## Current Slice

The first implemented slice is M0 + M1:

- local project skeleton;
- GLFW window;
- Vulkan-backed RHI boundary;
- frame context with frames in flight;
- swapchain acquire, clear, submit, and present.
