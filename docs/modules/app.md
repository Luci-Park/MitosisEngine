# app

- **Maintainer:** Sumin Park
- **Depends on:** `mts::core`, `mts::window`, `mts::renderer`, `mts::assets`, all public
- **Public API:** `modules/app/include/app/`
- **Last reviewed:** 2026-09-01

## Purpose

The composition root and the frame loop. `App` is the one place that knows every
other module exists: it constructs the window and renderer in the right order,
owns the ECS world, command buffer and scheduler, lazily opens the asset cache,
and ticks everything until the window closes.

It contains no gameplay and no subsystem of its own. It is also deliberately not
a framework - there is no registry, no service locator and no virtual `Game` to
subclass. `main` constructs an `App`, registers systems on it, and runs it.

## Mental model

Fixed members, three calls.

```
Initialize(desc) -> Run() -> Shutdown()
```

The member list *is* the initialisation order, and reverse-order destruction is
what makes teardown correct, so declaration order in `App.h` is load-bearing
rather than cosmetic.

`Run` is a plain loop with no fixed timestep:

```
Start(scheduler)                 systems cache their queries
while not ShouldClose:
    PollEvents                   close flag and size refresh here
    dt = min(elapsed, mMaxDeltaSeconds)
    Update(scheduler)            PreUpdate, Update, PostUpdate; flush between
    renderer.DrawFrame()         not a system - see below
    ++frame
```

A `SystemContext` is rebuilt every tick rather than stored: it bundles references
to the world and command buffer with the values that change each frame (`dt`,
elapsed, frame index), so keeping one around would only be a stale copy.

`DrawFrame` is called directly instead of from a system in the `Render` phase.
The phase exists in `core`, but a render system would have to reach the renderer,
and `engine_core` must not link `engine_renderer` - so the phase stays empty and
the loop draws. This is the single largest piece of temporary wiring in the
engine.

## Key types

| Type | Header | Role |
|---|---|---|
| `AppDesc` | `app/App.h` | Window size and title, Vulkan app name and validation flag, `dt` clamp |
| `App` | `app/App.h` | Owns the subsystems, runs the loop |

Everything `App` owns is documented in its own module: `World`, `CommandBuffer`
and `SystemScheduler` in [core.md](core.md), `Window` in [window.md](window.md),
`VulkanRenderer` and `AssetCache` in [ARCHITECTURE.md](../ARCHITECTURE.md).

### Ownership and order

| Member | Created | Destroyed | Because |
|---|---|---|---|
| `mWindow` | first | last | the renderer's surface is built from it |
| `mRenderer` | after the window | before it | holds `VkSurfaceKHR` and the device |
| `mWorld`, `mCommands`, `mScheduler` | with `App` | with `App` | no external resources |
| `mAssetManifest` | first `Assets()` | after the cache | owns the parsed manifest |
| `mAssetCache` | with the manifest | before it | holds a raw pointer into it |

The manifest/cache pair is the one real aliasing hazard, and it is handled twice
over: by declaration order, so implicit destruction is already correct, and by
explicit `reset()` calls in `Shutdown` in the same order, because `Initialize`
may follow.

## Usage

`main.cpp`, in full:

```cpp
mts::InitLog();                 // outside App: early failures must still log

mts::App app;

mts::AppDesc desc{};
desc.mTitle = "MitosisEngine - Window Test";

if (!app.Initialize(desc))
{
    mts::FlushLog();
    return -1;
}

app.Run();
app.Shutdown();                 // idempotent; the destructor would do it too
mts::FlushLog();
```

Systems are registered on `app.Systems()` between `Initialize` and `Run`,
components created through `app.GetWorld()`.

## Invariants

- **Register systems before `Run`.** `Run` calls `SystemScheduler::Start`, and
  the scheduler requires registration to be finished by then. A system added
  afterwards never gets `OnStart`.
- **`Initialize` before anything else.** `Run` and `Shutdown` return immediately
  when `mInitialized` is false, silently - a caller that ignores the `bool` gets
  a process that exits successfully having done nothing.
- **`Initialize` cleans up after itself.** A renderer failure resets the window
  before returning `false`, so a failed `Initialize` leaves no half-built state.
- **`Shutdown` is idempotent and the destructor calls it.** Calling it explicitly
  is the documented style; forgetting it is not a leak.
- **Systems stop before subsystems die.** `Shutdown` runs `Stop` first, so
  `OnStop` still sees a live world - but the renderer and window are already on
  their way out, and touching them from `OnStop` is not supported.
- **`Assets()` may return `nullptr`,** and the failure is sticky: no manifest
  means the disk is not re-checked every frame. A caller loses an asset, not the
  process. `Shutdown` clears the flag so a later `Initialize` retries.
- **`dt` is clamped, and `mElapsed` accumulates the clamped value.** After a
  breakpoint or a window drag, `mElapsed` is behind the wall clock on purpose -
  it is simulated time, not a timestamp, and must not be used as one.
- **`App` is neither copyable nor movable,** and hands out references to its own
  members through `SystemContext`.
- **`App` does not own logging.** `InitLog` and `FlushLog` are the caller's, so
  that a construction failure inside `App` is still visible.

## Implementation notes

- The clock is `std::chrono::steady_clock` - monotonic, so a system clock change
  mid-session cannot produce a negative `dt`.
- `MakeContext(0.0f)` is used for both `Start` and `Stop`: neither is a frame,
  and a nonzero `dt` there would be a lie.
- `mFrame` increments after the draw, so the first tick runs as frame 0.
- The `if (!mWindow)` branch after `Window::Create` is currently unreachable -
  the GLFW backend aborts through `MTS_CHECK` instead of returning null (see
  [window.md](window.md)).
- `mDesc` is kept after `Initialize` only for `mMaxDeltaSeconds`; the rest is
  consumed at startup.
- The renderer is a value member, not a `unique_ptr`: `Renderer.h` is still an
  empty placeholder, so there is no interface to hold and `App` names
  `VulkanRenderer` directly.

## Current state

*As of 2026-09-01.* The loop runs, the scheduler ticks, the triangle draws. What
is missing is mostly the connective tissue:

- **Nothing in the world reaches the screen.** The `Render` phase is empty and
  `DrawFrame` ignores the ECS entirely.
- No fixed timestep, so physics-style systems have no stable substep.
- No input, no pause, no frame limiter, no headless mode, no window-event
  handling beyond the renderer re-querying size.
- `Initialize` after `Shutdown` is written to work - the sticky asset flag and
  the explicit resets exist for it - but nothing exercises it.
- One `App` per process is assumed everywhere, though nothing enforces it.

## Tests

None. `modules/app/` has no `tests/` directory.

`App` is composition; its behaviour is a window opening, a device initialising
and a loop running, all of which need a display and a GPU. The parts worth
asserting - phase ordering, command buffer flushing, asset cache lookup - are
tested where they live, in `core` and `assets`. A headless renderer would be the
prerequisite for testing anything here, and that is also the trigger for
revisiting the whole shape of this class.

## Open questions

- How the ECS reaches the renderer without `core` depending on it: a renderer
  handle in `SystemContext`, an extractor owned by `App`, or leaving drawing out
  of the ECS entirely. This decides whether `SystemPhase::Render` survives.
- Whether `App` should own logging after all, given that the only reason it does
  not is the ordering in `main`.
- Whether a fixed-timestep phase belongs in the scheduler or in the loop.
