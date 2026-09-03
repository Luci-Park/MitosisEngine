# 0019 - Per-world state lives in typed resources

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

[0006](0006-pod-components.md) requires components to be trivially copyable and
says owning data is "referenced by handle and owned elsewhere". There was no
elsewhere.

State that belongs to a world but not to any entity had nowhere to go: a scene
graph index, a camera, an input snapshot, the asset cache, a script VM. Each
option in the tree was worse than the last. `AssetCache` sat as a private `App`
member, so no system could reach an asset at all. `SystemContext` was the other
route, and it grows a field per subsystem while handing every system a reference
to all of them.

The immediate forcing case was the scene graph. Parent and child links had to be
kept in step, and while they were POD fields on a component the generic ECS could
pull them apart - `World::RemoveComponent` is a template that deletes invariants
it has never heard of. Sealing them inside one owner needed somewhere for that
owner to live.

## Decision

`World` holds at most one instance of any type, keyed by type.

```cpp
T &EmplaceResource<T>(Args&&...);   // constructs in place, replaces any previous
T *TryResource<T>();                // nullptr when absent  (+ const overload)
T &Resource<T>();                   // must exist           (+ const overload)
bool HasResource<T>();
bool RemoveResource<T>();
```

Resources are not components and carry none of 0006's constraints: nothing ever
relocates one, so they may own heap memory and have destructors.

Four properties the implementation commits to:

- **Their own type counter.** `detail::ResourceIdOf` does not use `TypeIdOf`,
  because `ComponentBit` uses `TypeId::seq` *directly* as a signature bit index
  and asserts it stays under `kMaxComponentTypes`. Drawing resource ids from
  that counter would push real components toward the ceiling, failing later, in
  unrelated code, in an order-dependent way.
- **Stable addresses.** The value lives in a heap-allocated holder, so a `T*`
  survives any number of later emplacements of *other* resources. Systems cache
  them in `OnStart`.
- **Const-normalised keys.** `TryResource<const T>` names the same resource as
  `TryResource<T>`. Without that it is a separate instantiation with its own id,
  which compiles and then always reports the resource as absent.
- **Destroyed first.** `mResources` is declared last, so reverse declaration
  order tears resources down while entity storage is still alive.

## Consequences

- 0006's "owned elsewhere" is now a real place. `std::vector`, `std::string` and
  destructors are all fine in a resource.
- Systems reach engine state through the `World` they already have, instead of
  `SystemContext` growing a field per subsystem.
- Re-emplacing **replaces and destroys**, which invalidates any cached pointer to
  *that* resource. Emplace during setup, not mid-frame.
- `Resource<T>()` uses `MTS_CHECK`, not `MTS_ASSERT`: it returns a reference, so
  a missing resource under `NDEBUG` would be a null dereference rather than a
  diagnosable stop. It aborts. Anything reachable from user input - a script
  naming a resource that does not exist - must use `TryResource` instead.
- **A resource destructor must not call back into `World`.** `~World` is already
  destroying the map, so `DestroyEntity` - whose hooks look resources up again -
  would search a container whose elements are being destroyed. A resource holding
  entity handles needs an explicit shutdown call before the world goes down, not
  destructor cleanup.
- Resources are invisible to queries, have no change detection, and no
  per-entity form. Anything needing those is a component.
- One hash lookup per access. Hot paths hoist the pointer once rather than
  looking it up per iteration.

## Alternatives considered

- **Singleton components**, as Bevy and Flecs do - reuses the query machinery and
  gets change detection for free, but inherits 0006's POD constraint. That
  constraint is the exact thing that forced this decision, so it solves nothing
  for a scene graph that wants a `std::vector`.
- **Keep them as `App` members** - what the asset cache already did. Systems
  cannot reach them, and `App` becomes the god object
  [0009](0009-app-composition-root.md) exists to prevent.
- **More `SystemContext` fields** - a field per subsystem, every system handed a
  reference to all of them, and `core` forced to know every module's types.
- **Globals or statics** - no per-world isolation, and tests construct many
  `World`s in one process.

## Revisit when

Parallel system scheduling arrives. A scheduler that runs systems concurrently
needs each system's read and write set, and resources are invisible to the query
machinery that would otherwise report it - so either resources gain declared
access, or systems touching them stay serial.
