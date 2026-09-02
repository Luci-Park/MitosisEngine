# window

- **Maintainer:** Sumin Park
- **Depends on:** `mts::core` publicly; `glfw` privately, desktop only
- **Public API:** `modules/window/include/window/`
- **Last reviewed:** 2026-09-01

## Purpose

Gets an OS window on screen, pumps its events, and reports whether it should
close. It also hands out the one thing a renderer needs from it - a native handle
and a size - through `ISurfaceProvider`, which is declared in `core`, not here
([0013](../decisions/0013-surface-provider.md)).

It does not do input, and it does not touch a graphics API. GLFW is created with
`GLFW_NO_API`: the renderer owns the graphics API entirely, and no GLFW type
appears in a public header or in any translation unit outside this module.

## Mental model

One public header declaring an interface, and a backend chosen at **build** time,
never at runtime.

`Window.h` declares `WindowDesc`, the abstract `Window`, and the static factory
`Window::Create`. Nothing defines `Create` in the public part of the module -
the definition lives in whichever backend `CMakeLists.txt` compiled. A caller
therefore holds a `unique_ptr<Window>` and never names a backend type.

The GLFW backend is split in two on purpose:

- `glfw/GLFWWindow.{h,cpp}` - everything portable: creation, the event pump, the
  close flag, framebuffer size tracking, and the GLFW init refcount.
- `glfw/GLFWWin32.cpp`, `glfw/GLFWLinux.cpp` - one function each,
  `GLFWWindow::NativeWindow()`, because native handle extraction is the only part
  that needs `glfw3native.h` and its per-platform `GLFW_EXPOSE_NATIVE_*` defines.

Selection is in `modules/window/CMakeLists.txt`: the whole backend is guarded by
`ENGINE_FAMILY_DESKTOP`, and within it `ENGINE_PLATFORM` picks the native file.

## Key types

| Type | Header | Role |
|---|---|---|
| `WindowDesc` | `window/Window.h` | Size, title, resizable |
| `Window` | `window/Window.h` | The interface, plus `Create` |
| `GLFWWindow` | `src/window/glfw/GLFWWindow.h` | Desktop implementation, not public |
| `ISurfaceProvider` | `core/platform/Surface.h` | Base of `Window` - `NativeWindow`, `Width`, `Height` |
| `NativeWindowHandle` | `core/platform/Surface.h` | `{backend, display, window}` |

`Window` adds exactly two methods of its own, `PollEvents` and `ShouldClose`; the
other three are inherited from `ISurfaceProvider`. That split is the point of the
design - the renderer takes the base and never sees the event pump.

`NativeWindowHandle::display` is null on Win32 and Cocoa, and is the X11 or
Wayland display elsewhere.

## Usage

```cpp
WindowDesc desc{};
desc.mWidth = 1280;
desc.mHeight = 720;
desc.mTitle = "MitosisEngine";

std::unique_ptr<Window> window = Window::Create(desc);

while (!window->ShouldClose())
{
    window->PollEvents();
    // draw, using window.get() as an ISurfaceProvider
}
```

`App::Initialize` is the real caller; see [app.md](app.md).

## Invariants

- **Only the desktop family has a backend.** On mobile or web nothing defines
  `Window::Create` and the link fails. An unrecognised desktop platform is worse
  handled than that: `macOS` passes the family check but matches neither
  `ENGINE_PLATFORM` branch, so the build stops at configure time with
  `No native window backend for macOS`.
- **Main thread only.** GLFW requires that `glfwInit`, window creation and
  `glfwPollEvents` all happen on the main thread, and `g_windowCount` is a plain
  `int` with no synchronisation. Creating or destroying a window off-thread is a
  data race on top of a GLFW violation.
- **`PollEvents` must be called, and often.** It is what drives the framebuffer
  resize callback and the close flag; skip it and `ShouldClose` never becomes
  true and the size goes stale.
- **The window outlives its renderer.** The renderer builds a `VkSurfaceKHR` from
  the native handle, and destroying the window first leaves that surface pointing
  at a dead window. `App::Shutdown` encodes the order.
- **`Width`/`Height` are framebuffer pixels, not screen coordinates**, and they
  are legitimately `0x0` while minimised. The window deliberately does not filter
  that - the consumer decides to skip frames.
- **`Window` is neither copyable nor movable.** Own it through `unique_ptr`; the
  GLFW backend holds a raw `GLFWwindow*` it destroys, and GLFW holds a user
  pointer back to the instance.
- **Failure aborts rather than returning.** `glfwInit` and `glfwCreateWindow` are
  wrapped in `MTS_CHECK`, which fires in Release too, so `Window::Create` never
  returns null on the desktop backend. This is a departure from the convention
  that init paths return `bool` and let the caller decide - see Open questions.

## Implementation notes

- GLFW is a process-wide singleton, so `g_windowCount` refcounts `glfwInit` and
  `glfwTerminate` against the number of live windows. Nothing creates a second
  window today, but the pattern means the first one to die does not take the
  library down with it.
- `glfwSetWindowUserPointer` is how the static framebuffer callback recovers the
  owning `GLFWWindow`. Any further callback added here follows the same route.
- Size is read once in the constructor and then only ever updated by the
  framebuffer callback, so `Width`/`Height` are a cached value, not a query.
- The error callback is installed before `glfwInit`, so GLFW's own diagnostics go
  through `MTS_LOG_ERROR` rather than being lost.
- `GLFWLinux.cpp` picks between Wayland and X11 at **runtime** with
  `glfwGetPlatform()`, even though the file itself was selected at build time.
  Both native headers are exposed and both code paths are compiled in.
- The X11 window id is an `unsigned long`, not a pointer, so it is cast through
  `uintptr_t` to fit `NativeWindowHandle::window`. Anything reading that field on
  Xlib must cast back the same way.

## Current state

*As of 2026-09-01.* Creation, event pump, close flag, framebuffer size and native
handle work, and that is the whole module. Missing:

- **Input.** No keyboard, mouse or gamepad, and no event type at all - callbacks
  are not surfaced to the caller.
- **Resize and close notification.** The size updates silently; nothing is told.
- Fullscreen, monitor selection, DPI scaling, cursor control, icon, runtime title
  changes, programmatic close.
- macOS: no native translation unit, so a macOS configure fails.
- Linux: sources exist, but there is no Linux preset, so the path is unbuilt.

## Tests

None. `modules/window/` has no `tests/` directory.

Everything here needs a real window server - `glfwInit` fails headless - and what
is left after removing that is a handful of forwarding calls. The module is
covered in practice by the engine failing to start.

## Open questions

- `MTS_CHECK` versus returning `nullptr`. `App::Initialize` already has a
  `if (!mWindow)` branch that the current backend can never take, so the two
  disagree; one of them should change.
- Where input belongs: more virtuals on `Window`, a separate `input` module fed
  by the backend, or an event queue drained by a system.
- Whether multiple windows are a real goal. The refcount supports it; nothing
  else in the engine does.
