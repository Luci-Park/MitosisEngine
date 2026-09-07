# script

- **Maintainer:** Rahul Nair
- **Depends on:** `mir::core` (public), `mir::assets` (private); `sol2`, `luajit` (private, third-party)
- **Public API:** `modules/script/include/script/`
- **Last reviewed:** 2026-09-05

## Purpose

Embeds Lua as the engine's gameplay/mission scripting layer - currently via
sol2 over LuaJIT (see [Implementation notes](#implementation-notes) for why
LuaJIT specifically). A script reads finalized per-frame ECS state and
drives mission triggers, entity behavior and HUD/UI logic; it does not touch
physics, flight dynamics or rendering, and nothing here assumes it ever
will. Exposing a new native C++ component to Lua costs zero new binding
code - this module is the primary consumer of `core`'s
`ComponentRegistry`/`FieldDesc` name-erasure, built for exactly this.

## Mental model

Two lifetimes, per loaded script name, kept deliberately separate:

- **Code** (`ScriptHostImpl::mLoadedScripts`) - one Lua table per loaded
  script *name*, holding only the functions a script defines
  (`OnStart`/`OnUpdate`/`OnStop`/`NewInstanceData`). Shared across every
  entity running that script, and re-resolved fresh from this map on
  *every* callback rather than cached at instance-creation time - the
  mechanism that makes a hot-reload reach an already-running entity on its
  very next tick, not just entities created after the reload.
- **State** (`ScriptHostImpl::mInstances`) - one small, private Lua table
  per `instanceRef`, passed as `self` (the first argument) to every
  callback. Created via an optional script-defined `NewInstanceData()`
  (falls back to an empty table), never by copying a template table: Lua's
  own `{}` literal already gives every instance an independent copy of any
  nested table, which a C++-side shallow copy could not do without
  reimplementing a deep copy.

`ScriptRef` (a POD component: `instanceRef`, `started`, `scriptName`) is the
only thing the ECS itself knows about a script - the Lua-side state lives
entirely in the two maps above, addressed by `instanceRef`. `ScriptSystem`
is the sole driver: one `ISystem` at `SystemPhase::PreUpdate` that walks
`Query<ScriptRef>`, calling `OnStart` once then `OnUpdate` every frame, with
each entity's callback isolated from the others by both a C++
`try`/`catch` and sol2's own `protected_function` - a script error stops
that entity's script, never the frame.

Because `ScriptSystem` always calls into Lua from inside its own
`Query<ScriptRef>::ForEach`, *every* structural change a script makes
(`world:add`/`remove`/`destroy`) is, by construction, always "mid-walk" -
there is no "outside a walk" case for a script to worry about. The bindings
route through `core`'s `DeferredAccess.h` (`AddComponentOrDefer` etc.)
unconditionally rather than trying to detect which case they're in.

## Key types

| Type | Header | Role |
|---|---|---|
| `IScriptHost` | `script/IScriptHost.h` | Abstract seam - the only thing outside `script/src` should depend on |
| `ScriptHost` | `script/ScriptHost.h` | The one Lua-backed implementation; sol2 hidden behind a pimpl |
| `ScriptRef` | `script/components/ScriptRef.h` | POD link component: `instanceRef`, `started`, `scriptName` |
| `ScriptSystem` | `script/ScriptSystem.h` | Drives `OnStart`/`OnUpdate`/`OnStop` per `ScriptRef`, `SystemPhase::PreUpdate` |
| `ScriptReloadWatcher` | `script/ScriptReloadWatcher.h` | Polls a tracked script's cooked file for changes, reloads on change |
| `RegisterScriptComponents` | `script/ComponentRegistration.h` | Registers `ScriptRef` with `ComponentRegistry`, mirrors core/renderer |

The Lua binding surface (`world:has/get/set/spawn/destroy/add/remove/each/declare`,
the `Entity` usertype) lives in `src/script/bindings/` and is not public -
it is reached only by loaded Lua source, never included from C++ outside
this module.

## Usage

The shape `main.cpp` and `ResolveSceneScripts` both use - load, instantiate, attach:

```cpp
mir::AssetCache *cache = app.Assets();
const mir::AssetId id = mir::MakeAssetId("games/HelloWorld/assets/scripts/spin.lua");
const mir::AssetBlobView *blob = cache->Load(id);

app.Scripts().LoadScriptSource("spin", mir::AsStringView(*blob));
app.ScriptReload().Track("spin", id);                     // picks up an edit on the next cook

const int32_t instance = app.Scripts().CreateInstance("spin");
world.AddComponent<mir::ScriptRef>(entity, mir::ScriptRef{.instanceRef = instance});
```

A minimal script:

```lua
local Spin = {}
local speed = 1.0

function Spin.NewInstanceData()
    return { angle = 0.0 }              -- per-instance; never shared
end

function Spin.OnUpdate(self, world, entity, dt)
    self.angle = self.angle + speed * dt
    local half = self.angle * 0.5
    world:set(entity, "Transform", "rotation", { w = math.cos(half), x = 0.0, y = math.sin(half), z = 0.0 })
end

return Spin
```

## Invariants

- **Script state never lives in a component.** Components stay POD
  (`MIR_ASSERT_COMPONENT`); a script's data lives in `ScriptHostImpl`'s two
  maps, addressed by `instanceRef`.
- **Every Lua callback goes through `sol::protected_function`,** never a
  raw call. This is what keeps a script's runtime error from unwinding
  through C++ frames via Lua's own error path instead of surfacing as a
  checked, loggable result - verified specifically under LuaJIT, not just
  assumed to carry over from plain Lua.
- **A system that installs a `World::AddDestroyHook` must remove it in
  `OnStop`.** `ScriptSystem` does. `SystemScheduler::Reset` (or
  `App::Shutdown` followed by another `Initialize`) destroys the system;
  a hook left behind calls into freed memory on the next entity destroyed.
  Covered by `modules/core/tests/System.tests.cpp`, not by a test in this
  module.
- **Register native components, `RegisterScriptComponents` included,
  before loading any script.** `ComponentRegistry` refuses a script that
  tries to claim an already-native name; a script loaded first would
  otherwise win the name instead.
- **`Entity` crosses the Lua boundary as a `sol::usertype<Entity>`
  (userdata), never a packed integer.** A Lua/LuaJIT number is a double,
  exact only to 53 bits; a packed `{generation, index}` handle can silently
  truncate `generation` once a slot has been reused enough times.
- **`FieldKind::Int` accepts any Lua number** (`get_type() ==
  sol::type::number`), not `is<int32_t>()`. That check's strictness is
  Lua-version-coupled (a subtype check on 5.3+, a round-trip check
  otherwise) - neither is what "is this usable as an int" should mean here.
- **`world:each`'s `RuntimeQuery` is cached per distinct term list,** kept
  as a `World` resource so its lifetime matches the `World`'s rather than
  a process-wide cache that could alias a reused heap address across
  `World` instances (e.g. between Catch2 test cases). Rebuilding it every
  call would still be correct, just slower.

## Implementation notes

- **Why LuaJIT:** measured, not assumed. On this build, `fib(32)` recursive
  is 2.7x faster than plain Lua, and a tight `sin`/`cos` loop over 50M
  iterations is 5.0x faster (LuaJIT's trace compiler favors loops over deep
  recursion - the two benchmarks deliberately bracket that). `luajit` has
  no CMake config package, only a `.pc` file, so it's found through
  `find_package(PkgConfig)` + `pkg_check_modules(LuaJIT ... IMPORTED_TARGET
  luajit)` instead of a plain `find_package()`. sol2 auto-detects LuaJIT via
  `LUAJIT_VERSION` in its own headers, so nothing else in this module
  changed for the swap - not the bindings, not `ScriptHost`, nothing.
- **`OnStart` and an entity's *first* `OnUpdate` run in the same
  `PreUpdate` pass,** before that phase's `CommandBuffer` flush - a system
  calls `OnStart` then `OnUpdate` back to back. A deferred `world:add`
  issued in `OnStart` is therefore still pending at the first `OnUpdate`
  too; it only becomes visible starting the *second* `OnUpdate` call.
- **The mtime-poll interval (`ScriptReloadWatcher`, ~1s) is a plain
  accumulator, not a filesystem watch** - nothing else in the engine has
  async/watch infrastructure to build one on top of, and reloading is cheap
  enough (re-run a chunk) that polling costs nothing noticeable.
- Getting the copy-cooked-assets-next-to-the-exe step to actually run on an
  asset-only change (no C++ recompiled) needed a fix in
  `cmake/Assets.cmake`: the old `POST_BUILD` command shared its target's
  own up-to-date check, so ninja skipped the copy whenever nothing needed
  relinking - a script edit would cook fine and then silently never reach
  the running exe's `cooked/` directory. It's now a separate `OUTPUT`-based
  custom command keyed off the cook stamp instead.
  `Shaders.cmake`/`Fonts.cmake` use the same `POST_BUILD` pattern and
  likely have the same latent gap; out of scope here.
- Resolving a script by name after loading a scene (`SceneIO` round-trips
  `ScriptRef.scriptName`, not `instanceRef` - that's this run's live state
  only, deliberately not saved) is glue code that lives in `main.cpp`
  (`ResolveSceneScripts`), not in this module - see Open questions.

## Current state

*As of 2026-09-05.* Embedding, ECS binding
(`has`/`get`/`set`/`spawn`/`destroy`/`add`/`remove`/`each`/`declare`),
per-entity instance dispatch with isolated errors and independent
per-instance state, `CommandBuffer`-deferred structural changes, and
mtime-poll hot-reload all work and have been exercised end to end against
the running app. LuaJIT is the shipped backend; sol2's own
version-detection is the only thing that would need touching to go back to
plain Lua.

`ScriptRef.scriptName` and its `ComponentRegistry` registration exist so
scene files can round-trip which script an entity runs; resolving that name
back into a live `instanceRef` after a scene load is a manual pass
(`main.cpp`'s `ResolveSceneScripts`), not something this module or
`SceneIO` does for the caller.

## Tests

None in `modules/script/` - there is no `tests/` directory yet. Everything
in this module has been verified by hand against the running app instead:
attaching a script to multiple entities and confirming independent state,
editing a script live and confirming an already-running instance picks up
the change on its next tick, and a deliberate script error thrown from
inside a `world:each` callback, confirming it's caught and logged without
crashing or leaking. None of that is captured as an automated regression.

The one piece of this module's behavior that *is* covered by an automated
test lives elsewhere: the `AddDestroyHook`/`RemoveDestroyHook` pairing
`ScriptSystem` relies on is exercised by
`modules/core/tests/System.tests.cpp`, using a generic `ISystem` built for
the test rather than `ScriptSystem` itself - the behavior under test is
`World`'s hook API, not anything script-specific.

## Open questions

- No `modules/script/tests/` exists. `ScriptHost`/`ScriptSystem` need only a
  bare `World` and a `sol::state` to test in isolation - no window, no
  renderer. Binding round-trips, per-instance isolation, a deferred
  mutation issued from inside a Lua callback, and the `Entity`-as-userdata
  precision case are all candidates, and none of them need the running app
  they were actually verified against.
- `ResolveSceneScripts`/`ResolveSceneMeshes` (turning what `SceneIO` can
  round-trip - names - back into live handles after a load) live in
  `main.cpp`, not in `scene` or `script`. Worth a real home once a second
  consumer would otherwise duplicate them.
- `world:each` builds a fresh Lua table per matched entity for each query
  term, every call, even though the underlying `RuntimeQuery` is now
  cached. Whether that allocation is worth avoiding is unmeasured.
- No script-facing way to declare a default value for a *native* (C++)
  component field the way `NewInstanceData()` does for a script's own
  `self` state - `world:add` with no fields table uses whatever
  `ComponentOps` already registered as the type's default, which is a
  C++-side decision a script cannot override per call site.
- LuaJIT vs. plain Lua is a `vcpkg.json` entry plus one `CMakeLists.txt`
  block today; worth confirming that stays true across a future sol2
  upgrade that changes how it detects the Lua flavor underneath.
