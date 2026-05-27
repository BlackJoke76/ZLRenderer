# Renderer Project Agent Guide

## Project Intent

This renderer is a learning project first.

The goal is not to build a complete commercial engine. The goal is to make the
core ideas readable enough that the owner can later understand commercial engine
rendering code: RHI, render graphs, frame context, render scene, pass/resource
lifetime, Vulkan synchronization, shader compilation, descriptor binding,
pipeline creation, and GPU feature integration.

When a feature can be implemented in a smaller educational form, prefer that
over a production-scale system.

## Core Direction

- Build the renderer around a minimal RHI + Render Graph architecture from the start.
- Use Vulkan as the backend API.
- Use Slang as the shader language and shader interface source of truth.
- Use ImGui early for debugging and inspection, not for product UI.
- Keep GAMES202 assignments as technique milestones.
- Keep future support in mind for compute shaders, mesh shaders, Vulkan ray
  tracing, and AI-related passes.

## Collaboration Workflow

For non-trivial work, use this loop:

1. Inspect the current code and docs.
2. State the smallest useful plan.
3. Implement the next runnable slice.
4. Build or run the cheapest meaningful verification.
5. Update `docs/STATUS.md` and add a decision note when architecture changes.

Do not rely on chat history as project memory. Durable project state belongs in
repo files.

## Skill Usage

Use existing Codex skills during programming:

- `vulkan-renderer-optimization` for Render Graph, RHI/VulkanRHI,
  synchronization, command buffers, render passes, pipelines, descriptors, and
  Vulkan performance risks.
- `karpathy-guidelines` to keep changes surgical and avoid speculative systems.
- `cpp-pro` for C++20/23 interfaces, RAII ownership, CMake, memory safety, and
  build/tooling decisions.
- `game-developer` for RenderScene, draw lists, frame loop shape, profiling, and
  game-engine style renderer boundaries.
- `github:yeet` only when publishing local changes to GitHub with branch,
  commit, push, and draft PR flow.

Before introducing a major renderer abstraction, write down what current
complexity it removes or what dependency/lifetime problem it makes visible.

## GitHub Sync Policy

This project should be synchronized through GitHub once a local git repository
and GitHub remote exist.

Expected flow:

1. Work on a `codex/...` branch for assistant-authored changes.
2. Keep commits small and tied to one milestone or decision.
3. Push the branch to GitHub.
4. Open a draft PR for review when the change is more than a tiny doc edit.

Never stage unrelated local changes silently.

## Formatting

- One indentation level is 4 spaces.
- Use spaces for indentation in source and docs.
- Do not use tab characters for manual indentation.
- Match existing file style when editing generated or third-party files.

## Architecture Bias

Prefer visible dependencies over hidden state.

For RHI work, Vulkan handles must stay inside `VulkanRHI` implementation files
unless a milestone explicitly expands the abstraction.

For Render Graph work, every pass should declare:

- resources it reads;
- resources it writes;
- intended usage/layout;
- queue or pass type;
- execution callback.

Avoid scattering Vulkan barriers inside unrelated helpers. The Render Graph or
an explicit resource state tracker should be the place where producer/consumer
relationships become visible.
