# 0012 - Vulkan 1.3 core feature baseline via volk and VMA

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

A Vulkan renderer either supports a wide device range through extension
fallbacks, or picks a modern baseline and refuses anything below it. Every
fallback is code to write, test on hardware nobody has, and maintain.

## Decision

Target Vulkan 1.3, requiring `dynamicRendering`, `synchronization2`,
`timelineSemaphore` and `shaderDrawParameters` as core features; a device missing
any is rejected during selection with a log line naming what was missing. So: no
render passes or framebuffers, and synchronization2 barriers only. Frame pacing
uses one timeline semaphore with `kFramesInFlight = 2`, its counter starting above
the frame count so the wait value cannot underflow.

volk loads entry points and is compiled from source in the renderer module so the
platform defines are ours. VMA owns device memory, configured with
`VMA_STATIC_VULKAN_FUNCTIONS=0` and `VMA_DYNAMIC_VULKAN_FUNCTIONS=1` so it fetches
its pointers through volk.

## Consequences

- Much less code: no render pass or framebuffer objects, no fence pools.
- Anything older than roughly 2020 hardware, or a stale driver, will not run.
  Acceptable for a project that is not shipping to consumers.
- Validation, debug messenger and object naming are wired for RenderDoc.
- A second backend would need an interface extracted first; `Renderer.h` is the
  unused placeholder.

## Alternatives considered

- **Vulkan 1.0/1.1 with extension fallbacks** - broadest support, a second code
  path per feature.
- **Link the loader directly instead of volk** - simpler, but per-call dispatch
  overhead and no clean per-device function tables.
- **Hand-rolled allocator** - a project of its own.
