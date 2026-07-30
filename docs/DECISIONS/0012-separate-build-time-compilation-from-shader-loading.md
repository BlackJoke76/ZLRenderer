# Decision 0012: Separate Build-Time Compilation From Shader Loading

## Status

Accepted.

## Context

The renderer already compiles Slang to SPIR-V through CMake. Pipeline creation
still contained its own file lookup and binary loading helpers inside
`VulkanDevice`, while the roadmap named a runtime `IShaderCompiler` before any
hot-reload or editor caller existed.

Adding a runtime compiler now would duplicate the working build-time path and
introduce process execution, dependency tracking, diagnostics, and cache
invalidation without a current runtime use case.

## Decision

Keep shader compilation at build time and make both sides explicit:

1. `zl_compile_slang_shader` is the reusable CMake boundary that turns a Slang
   source entry point into a compiled SPIR-V artifact.
2. `CompiledShaderLibrary` is a backend-neutral RHI helper that loads a named
   artifact and preserves its stage and entry point.
3. `VulkanDevice` converts the loaded SPIR-V words into temporary
   `VkShaderModule` objects during cached pipeline creation.

A runtime compiler interface will be introduced only when hot reload, editor
iteration, or runtime shader generation provides a concrete caller.

## Consequences

Positive:

- Vulkan pipeline code no longer owns filesystem parsing.
- The build and runtime halves of the shader path can be tested independently.
- Runtime compilation machinery is deferred until its diagnostics and caching
  requirements are visible.

Negative:

- Shader changes still require the build system to regenerate SPIR-V.
- The descriptor layout still mirrors shader declarations manually because
  reflection is not implemented yet.

## Guardrails

- Do not invoke `slangc` from the render loop or pipeline-cache lookup path.
- Keep pipeline creation outside the draw path.
- Add a runtime compiler boundary together with its first real caller and
  explicit error-reporting behavior.
