# 0013 - The renderer sees a surface provider, never a window

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

A Vulkan renderer needs three things from a window: a native handle, a width and
a height. It does not need the event pump or the close flag - but linking the
window module gets them anyway, and the dependency becomes real.

## Decision

`core/platform/Surface.h` declares `NativeWindowHandle` and `ISurfaceProvider`,
which exposes exactly `NativeWindow()`, `Width()`, `Height()`. `RendererDesc`
takes one. `Window` implements it. The renderer does not link the window module,
and GLFW never appears in a renderer translation unit. Surface creation itself
lives in `VulkanSurface<Platform>.cpp`, selected by `ENGINE_PLATFORM`.

## Consequences

- Anything that can produce a native handle can drive the renderer - a test
  harness, an editor host, another windowing library.
- Replacing GLFW touches one module.
- Two interfaces where one type would do, and `Window` inherits an interface it
  does not use itself.
- Anything else the renderer needs later (DPI scale, HDR) must be added here
  deliberately, which is the point.

## Alternatives considered

- **Pass `Window` directly** - one less type, and a renderer-to-windowing
  dependency that would be very hard to remove later.
- **Pass a raw handle plus dimensions** - no interface, but the renderer could
  not re-query size after a resize.
