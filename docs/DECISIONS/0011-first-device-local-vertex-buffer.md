# Decision 0011: Upload The First Vertex Buffer To Device-Local Memory

## Status

Accepted.

## Context

The first triangle used `SV_VertexID` to index shader-local arrays. That made
the draw call work, but it hid the CPU data -> GPU buffer -> vertex input path
that real meshes use.

Granite keeps mesh vertex buffers in device-oriented memory and uploads data
through its buffer system. This renderer needs the same visible data flow, not
Granite's full resource manager or streaming infrastructure.

## Decision

The triangle owns one static, device-local vertex buffer with interleaved
`float2 position` and `float3 color` data.

- Create a temporary host-visible, host-coherent staging buffer.
- Copy the three CPU vertices into that staging memory.
- Record one `vkCmdCopyBuffer` into a transient command buffer and wait for the
  graphics queue during device initialization.
- Make the copy's transfer write visible to later vertex-input reads with an
  explicit buffer memory barrier.
- Destroy the staging allocation after the copy completes.
- Declare two matching vertex attributes in the graphics pipeline and bind the
  device-local vertex buffer before `vkCmdDraw`.

The vertex buffer survives swapchain recreation. The pipeline is recreated
with the same vertex layout when the swapchain format changes.

## Consequences

Positive:

- The first mesh-data path is visible without a general mesh abstraction.
- Static geometry does not remain in host-visible memory during rendering.
- The shader no longer requires `shaderDrawParameters` or `SV_VertexID`.

Negative:

- Queue-idle waiting is acceptable only because this is a one-time startup
  upload.
- The buffer has no index data, ownership API, batching, or streaming path.
- CPU and shader vertex layouts remain manually coupled until reflection and a
  mesh format description exist.

## Guardrails

- Do not use `vkQueueWaitIdle()` for per-frame or runtime asset uploads.
- Do not add a general RHI buffer or mesh class until a second real mesh caller
  demonstrates its required lifetime and binding surface.
- Keep runtime vertex-buffer access in the draw path; initialization upload
  synchronization is not a Render Graph pass.
