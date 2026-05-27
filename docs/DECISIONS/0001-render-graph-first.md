# Decision 0001: Build Render Graph First

## Status

Accepted.

## Context

The project is a learning renderer intended to help the owner understand
commercial engine architecture. It must later host GAMES202 assignments and
experiments with compute shaders, mesh shaders, Vulkan ray tracing, and
AI-related passes.

If these features are added to an ad hoc render loop first, each technique will
bring its own resource lifetime and synchronization rules. That makes the code
harder to learn from and harder to compare with commercial engines.

## Decision

Start the renderer with a minimal Render Graph.

The first visible frame should already be produced through graph execution. The
graph will initially support explicit resource declarations, pass read/write
declarations, simple ordering, conservative Vulkan state transitions, and pass
callbacks.

## Consequences

Positive:

- GAMES202 assignments can be added as pass chains.
- Compute, mesh shader, ray tracing, and AI experiments can share one dependency
  model.
- Vulkan barriers and layout transitions have a central home.
- The project teaches commercial-engine structure earlier.

Negative:

- The first triangle takes longer than a direct Vulkan sample.
- Early code needs more naming discipline.
- The graph must stay small to avoid becoming a fake production engine.

## Guardrails

- Do not add transient aliasing in the first version.
- Do not add async compute scheduling in the first version.
- Do not add automatic pass merging in the first version.
- Do not add a large descriptor system before Slang reflection and real pass
  needs exist.
