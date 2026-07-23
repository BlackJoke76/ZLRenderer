# Renderer Roadmap

## Guiding Principle

ZLRenderer is a learning renderer. Each milestone should reveal one commercial
engine concept clearly instead of chasing feature completeness.

## M0: Project Bootstrap And GitHub Sync

- Initialize git and project structure.
- Add CMake, C++20, Vulkan SDK detection, GLFW, and Dear ImGui.
- Keep `AGENTS.md`, `docs/STATUS.md`, `docs/ROADMAP.md`, and `docs/ARCHITECTURE.md` current.
- Create and push to the public GitHub repository after GitHub CLI auth is ready.

Success signal: a new session can read `AGENTS.md` and `docs/STATUS.md` and know
the next action.

## M1: RHI, Frame Loop, And Swapchain Clear

- Add a small RHI boundary.
- Implement `VulkanRHI` through `VulkanDevice`.
- Add `FrameContext` and frames-in-flight.
- Run acquire -> clear -> submit -> present.
- Use frame-slot image-available semaphores and swapchain-image render-finished
  semaphores.

Success signal: a GLFW window clears through `RHICommandList` with Vulkan
validation enabled.

## M2: Render Graph Clear

- Add `RenderGraph`, resource handles, and pass builder.
- Import the swapchain image as a graph resource.
- Move the clear pass from direct RHI use into Render Graph execution.
- Emit a readable graph dump.

Success signal: clear is produced by Render Graph, not direct application code.

## M3: Slang Triangle And Pipeline Cache

- Add `IShaderCompiler` and `SlangcShaderCompiler`.
- Add `ShaderModule`, `PipelineLayout`, `PipelineKey`, and `PipelineCache`.
- Draw a triangle through Render Graph.

Success signal: Slang compiles to SPIR-V and the triangle pass reuses cached
pipeline state.

## M4: Task System, RenderScene, And DrawList

- Add a minimal CPU `TaskSystem` with fixed workers, task groups, dependencies,
  completion handles, and stable worker indices.
- Add `RenderScene`, camera/view, mesh instance, material instance, light stub,
  and draw list.
- Use the task system first for scene gathering, culling, and DrawList building.
- Keep the first culling implementation simple, but preserve the parallel work
  boundary and a deterministic single-thread fallback.
- Feed Render Graph passes from DrawList.

Success signal: the code shows world -> scene -> parallel DrawList preparation
-> graph -> RHI, with task dependencies covered by tests.

## M5: Parallel Command Recording

- Extend compiled Render Graph output with explicit recording batches derived
  from pass/resource dependencies.
- Keep resource-state resolution, barrier placement, command-buffer ordering,
  queue submission, and presentation centralized.
- Add one command pool per recording worker, per frame-in-flight, per queue
  family; reset pools only after that frame has completed.
- Record sufficiently large independent physical passes in parallel primary
  command buffers and submit compatible buffers in coarse batches.
- Add secondary command buffers only when one large graphics pass has enough
  draw work to justify splitting it across workers.
- Measure serial and parallel CPU recording time before making parallel
  recording the default path.

Success signal: at least two independent non-trivial recording jobs execute on
different workers, Vulkan validation remains clean, command-buffer submission
follows the compiled graph order, and profiling demonstrates useful CPU work.

## M6: ImGui Debug, Graph Dump, And Profiling

- Add ImGui debug overlay.
- Show frame context, worker/recording timing, draw list, Render Graph passes,
  recording batches, and resources.
- Add RHI debug names, GPU marker scopes, and initial CPU pass timing.

Success signal: RenderDoc and ImGui both show useful renderer structure.

## M7: GAMES202 Pass Chains

- Map each assignment into pass chains such as shadow, environment/precompute,
  screen-space GI/SSR, PBR/high-quality shading, and ray tracing/denoising.

Success signal: each assignment has a pass/resource diagram and can be toggled
from debug UI.

## M8: Compute, Mesh Shader, Ray Tracing, And AI Passes

- Add compute, mesh shader, ray tracing, and AI/external pass support through
  Render Graph + RHI.

Success signal: new GPU features share the same dependency model and fail
cleanly when unsupported.
