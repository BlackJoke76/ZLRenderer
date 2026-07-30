# Decision 0010: Use Frame-Local Uniform Descriptor Sets First

## Status

Accepted.

## Context

The first Slang triangle used only shader-local constants. M3 needs one real
resource binding so the path from CPU data, through a Vulkan descriptor, to a
shader read is visible before mesh and material abstractions arrive.

The renderer already has two frame slots. A buffer written for one slot must
not be overwritten while that slot's prior GPU submission can still read it.

## Decision

The triangle pipeline owns one fragment uniform binding at `set = 0`,
`binding = 0`.

- Create one `VkDescriptorSetLayout` and one fixed-size descriptor pool when
  the device is initialized.
- Give every `FrameResources` slot its own host-visible, host-coherent uniform
  buffer and one descriptor set allocated from that pool.
- Update the mapped buffer only after `beginFrame()` waits for that slot's
  `inFlightFence`.
- Bind the current slot's descriptor set immediately before its triangle draw.
- Allocate and write descriptors at setup time, not once per draw or once per
  frame.

The descriptor layout survives swapchain recreation. The swapchain-dependent
pipeline layout is rebuilt with the same descriptor-set layout.

## Consequences

Positive:

- CPU-to-shader data flow and frame-slot lifetime are explicit.
- Descriptor allocation and `vkUpdateDescriptorSets` stay off the draw path.
- The first binding uses the same update-frequency grouping that later frame
  and view data can use.

Negative:

- The descriptor layout is manually coupled to the Slang declaration until
  shader reflection exists.
- The buffer carries only one triangle tint and is not yet a material system.
- Host-coherent memory keeps this slice simple but is not a final upload
  strategy for large resource data.

## Guardrails

- Do not write a frame slot's uniform buffer before its fence has been waited.
- Do not add a general descriptor allocator or RHI descriptor API until a
  second caller makes their ownership and update frequency necessary.
- Do not call `vkUpdateDescriptorSets` from the triangle draw path.
- Keep the uniform buffer outside Render Graph resource tracking for now: it
  has one CPU producer and one graphics draw consumer, both owned by the frame
  slot.
