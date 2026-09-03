# 0022 - A process-wide component registry behind erased operations

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

Scripting arrives next, and a script names a component with a string. Nothing in
the ECS could answer such a name.

Every access path was a template - `World::Get<T>`, `Has<T>`, `AddComponent<T>`,
`CommandBuffer::Add<T>` - so a binding layer could only reach a component whose
C++ type it already named, which defeats the point. Worse, `TypeIdOf<T>()` is a
function-local static: a component type no C++ code has touched has no `TypeId`
at all, so there was nothing to look up even by hash.

`TypeId` already carried the right key. `seq` is allocation-ordered and differs
between runs, but `hash` is FNV-1a of the trimmed name and is stable, which is
exactly what a name-driven lookup needs.

## Decision

`ComponentRegistry` maps a component name to a `ComponentOps` - a set of
function pointers covering get, has, add, remove, and the two deferred forms -
plus a `FieldDesc` table for named field access.

Each pointer is instantiated for a concrete `T` at registration, so the templates
stay the only code that knows `T`. Every operation takes `const ComponentOps &`
first, which is how one signature serves both a native thunk (which ignores it)
and a script-declared one (which reads the TypeId, size and alignment out of it);
a plain function pointer has nowhere else to carry them.

Four properties the implementation commits to:

- **Process-wide, not per world.** `TypeId::seq` already comes from a global
  counter, and `ComponentBit`, `SparseStorageFor` and `Archetype::FindColumn`
  read it out of a function-local static. A per-world registry would turn each
  of those into a lookup and make that caching wrong, because the tests build
  many `World`s in one process. The ops take a `World &`, so nothing here is
  per-world anyway.
- **Every operation is total.** A dead entity, a missing component or a duplicate
  add is answered, not asserted on. `World::Has` asserts liveness and
  `World::Get` returns null, and that asymmetry is right for engine code, where
  touching a destroyed entity is a bug. A script holding a handle across the
  frame that destroyed it is ordinary, so an assert would turn normal gameplay
  into a debug stop.
- **Registration is explicit, never a static initialiser.** `engine_core` is a
  static library, and a self-registering static object in a translation unit
  nothing references is dropped by the linker - components present in Debug and
  missing in Release. `RegisterCoreComponents()` is called from `App::Initialize`.
- **Hash collisions stop the process in every build.** `TypeIdOf`'s collision
  check is `#ifndef NDEBUG`, which was fine while every name was a C++ identifier.
  Names now come from data, so `ComponentRegistry` re-checks with `MTS_CHECK`.

Field access goes through `FieldDesc`, which carries getter and setter *thunks*
rather than an offset for a C++ component. `Transform` is why: its members are
private so `Version()` cannot fall behind, and `WorldTransform` detects staleness
in O(1) off that version. An offset write to `mPosition` would move the object
and leave `mVersion` untouched, so every world matrix downstream would keep
looking current - no crash, no assert, one wrong frame. A script-declared
component has no invariant to protect and is offset-backed.

## Consequences

- A scripting module can do its whole job without naming a single component type,
  and links only `engine_core`.
- A read-only field is expressible (`mSet == nullptr`) instead of being a
  convention nobody enforces. `WorldTransform::matrix` is the first one.
- Two indirections per script-side component access - a hash lookup for the ops,
  a function pointer call - against a direct member access from C++. Engine
  systems keep the typed API and pay neither.
- **A C++ component must be registered to be reachable by name.** Adding a
  component and forgetting to register it produces no error, only a name a script
  cannot find. `RegisterCoreComponents` is the one list to keep current.
- **Registration order matters between C++ and scripts.** A name is claimed by
  whoever registers it first, and the two paths allocate their `seq` differently.
  Register the native components before loading any script; doing it the other
  way round is refused rather than papered over.
- Not thread safe. Registration is boot-time and script-load-time work.
- The registry is a function-local static that outlives every `World` and is
  never emptied. `TypeId::name` for a script component points into it, so those
  views dangle after exit - which matters to nothing, but is the reason names are
  interned into a `std::deque` rather than a `std::vector`.

## Alternatives considered

- **Offset-based reflection** - one `offsetof` per field and a memcpy. Cheaper and
  wrong for any component with an invariant, silently, which is the worst
  available failure mode. The thunks cost an indirect call on a path that is
  already crossing into a VM.
- **Per-world registry** - better isolation, but it makes every hot-path `seq`
  read a lookup and breaks the static caching those paths depend on.
- **Self-registering statics** (`MTS_REGISTER_COMPONENT` at namespace scope) -
  no list to keep current, but linker-dropped in a static library and it makes
  `seq` assignment depend on static initialisation order across TUs.
- **Put the ops on `World`** - `World` would grow a name-keyed map it has no other
  use for, and every world would pay for a table that is identical in all of them.

## Revisit when

Serialization needs it. A save file wants the same name-to-field machinery, and
the moment it does, `FieldKind` has to grow the types a scene format needs and
the read/write pair becomes a visitor rather than a memcpy of a fixed size.
