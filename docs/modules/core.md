# core

- **Maintainer:** Sumin Park
- **Depends on:** nothing in the engine; `spdlog` privately
- **Public API:** `modules/core/include/core/`
- **Last reviewed:** 2026-09-01

Also the worked example for [the template](TEMPLATE.md) - aim for this depth.

## Purpose

Everything the rest of the engine may depend on: the ECS, logging and assertions,
executable-relative paths, and the surface contract between a window and a
renderer. It knows nothing about windows, graphics APIs or assets, and must stay
that way.

## Mental model

Four independent pieces sharing one library because they share one property - no
engine dependencies.

- `ecs/` - world, entities, components, systems. By far the largest part.
- `log/` - logging and asserts without exposing a logging library.
- `fs/` - paths resolved against the executable, not the working directory.
- `platform/` - `ISurfaceProvider`, so a renderer can take a drawing surface from
  something without knowing it is a window.

## Key types

| Type | Header | Role |
|---|---|---|
| `Entity` | `ecs/Entity.h` | `{index, generation}` handle, 8 bytes |
| `EntityPool` | `ecs/EntityPool.h` | Allocates and recycles slots, bumps generations |
| `TypeId` | `ecs/TypeId.h` | Per-type `{seq, hash, name}` |
| `Signature` | `ecs/Signature.h` | 128-bit component set, the archetype key |
| `ComponentColumn` | `ecs/ComponentColumn.h` | One type's data in one archetype |
| `Archetype` | `ecs/Archetype.h` | Rows of entities sharing a signature |
| `SparseSetStorage<T>` | `ecs/SparseSetStorage.h` | Storage for churny components |
| `World` | `ecs/World.h` | Owns entities, archetypes, sparse storages, resources |
| `HierarchyIndex` | `ecs/HierarchyIndex.h` | The scene graph, held as a resource |
| `Transform` | `ecs/components/Transform.h` | Authored local TRS, versioned on write |
| `WorldTransform` | `ecs/components/WorldTransform.h` | Derived world matrix plus staleness stamps |
| `Query<Ts...>` | `ecs/Query.h` | Cached view over matching archetypes |
| `CommandBuffer` | `ecs/CommandBuffer.h` | Deferred structural changes |
| `ISystem`, `SystemContext` | `ecs/System.h` | Per-frame work and its inputs |
| `SystemScheduler` | `ecs/SystemScheduler.h` | Owns systems, runs them by phase |
| `ISurfaceProvider` | `platform/Surface.h` | Native handle plus dimensions |

### Type identity

`TypeIdOf<T>()` gives `seq` (process-local, used for bit positions and indices),
`hash` (FNV-1a of the name, stable across runs, for serialisation) and `name`
(logs, future editor).

The name is extracted from `__FUNCSIG__` or `__PRETTY_FUNCTION__` at compile time
and normalised - keywords stripped, spaces collapsed, namespaces removed - so
every compiler produces the same string. Because namespaces are stripped,
**component names must be globally unique**; a Debug registry asserts on a
collision and names both offenders.

### Storage

**Table (default).** Entities with the same component set share an `Archetype`
holding one `ComponentColumn` per type. Add or remove computes a new signature,
finds or creates that archetype, and moves the row - a `memcpy` per column, which
is why components are POD. Removing swaps the last row into the hole and fixes up
the moved entity's record.

**Sparse set (opt in).** `MTS_COMPONENT_SPARSE(T)`. `World` keeps one type-erased
storage per such type, outside the archetype system, cleared on destroy.

#### Resources

State that belongs to a world but not to an entity - the scene graph, a camera,
an asset cache, a script VM - lives in `World` keyed by type:

```cpp
world.EmplaceResource<HierarchyIndex>();
HierarchyIndex *index = world.TryResource<HierarchyIndex>();
```

Resources are not components and carry none of the POD constraints: nothing
relocates one, so they may own heap memory and have destructors. They are
invisible to queries and have no per-entity form. See
[0019](../decisions/0019-world-resources.md).

## Queries

`GetOrCreateQuery<Ts...>(filters...)` returns a cached `Query&` that resolves
matching archetypes and re-resolves when `World::Generation()` changes. Filters
are `With`, `Without`, `Or`; each `Or` is its own clause, ORed within. Sparse
components in the pack are fetched per entity rather than per column. `ForEach`
passes `(Entity, Ts&...)`, with `const T` arriving as `const T&`.

## Usage

```cpp
class MovementSystem final : public ISystem
{
public:
    void OnStart(SystemContext &context) override
    {
        mQuery = &context.world.GetOrCreateQuery<Position, const Velocity>();
    }

    void OnUpdate(SystemContext &context) override
    {
        mQuery->ForEach([&](Entity entity, Position &position, const Velocity &velocity)
        {
            position.x += velocity.x * context.dt;
            if (position.x > 100.0f)
                context.commands.Destroy(entity);   // never immediate
        });
    }

private:
    Query<Position, const Velocity> *mQuery = nullptr;
};
```

## Invariants

- **Components are POD.** Enforced twice. Anything owning memory belongs
  elsewhere, referenced by handle.
- **Component names are globally unique.** A collision asserts in Debug and would
  silently alias in Release.
- **No structural change during iteration.** `AddComponent`, `RemoveComponent`
  and `DestroyEntity` assert if a `Query` is walking; use the `CommandBuffer`.
  `World::IsIterating()` reports it, so a binding layer can route rather than
  guess. `CreateEntity` is deliberately exempt - the new entity lands in the
  empty archetype, so no matched table moves.
- **A resource destructor must not call back into `World`.** `~World` is already
  destroying the resource map. Anything holding entity handles needs an explicit
  shutdown call.
- **Scene structure is not in components.** Parent and child edges live in the
  `HierarchyIndex` resource, so `RemoveComponent` cannot tear them apart.
  `SetParent` refuses cycles and anything past `kMaxHierarchyDepth`, returning
  `false` rather than asserting. Destroying an entity destroys its subtree. See
  [0020](../decisions/0020-scene-graph-as-a-resource.md).
- **`Transform` is written through its setters.** Each bumps a version that
  `WorldTransform` compares against; a raw write leaves descendants stale
  permanently. `ResolveWorld` is correct at any point in the frame, so no reader
  needs to reason about phase order. See
  [0021](../decisions/0021-world-transforms-resolve-on-read.md).
- **At most 128 component types.** `ComponentBit` asserts past the limit.
- **A stale `Entity` is detected, not honoured.** `Get` returns `nullptr` for a
  dead handle; `AddComponent` and friends assert.
- **A signature excludes sparse components** - it is not the full component set.
- **Systems register before `Start`.**
- **`World` is neither copyable nor movable** - archetypes hold pointers back into
  it.

## Implementation notes

- `TypeIdOf<T>` builds its name in a `constexpr` buffer and returns a view into
  static storage, so a `TypeId` is cheap to copy.
- `Start` flushes at every phase boundary too, so an entity spawned in `OnStart`
  is visible to the next phase's `OnStart`. `OnStop` runs in reverse order.
- Logging is initialised in `main`, so construction failures are still visible.
- Trace and Debug compile out in Release and are free to leave in hot paths.

## Current state

*As of 2026-09-02.* Enough to build systems on: entities, both storages, filtered
queries, deferred structural change and phase scheduling all work and are tested.
Since 2026-09-01: typed resources on `World`, a transform hierarchy with lazy
world-matrix resolution, and cascading destruction through a `World` destroy
hook.

Missing: parallelism, dependency ordering, change detection, component lifecycle
hooks, serialisation (the stable hash exists, nothing uses it), a runtime
component registry, and debugging tools. `SystemPhase::Render` is unused - the
ECS-to-renderer bridge is a pair of calls in `App::Run`.

## Tests

`modules/core/tests/`:

| File | Covers |
|---|---|
| `ComponentStorage.tests.cpp` | Columns and sparse set storage |
| `Archetype.tests.cpp` | Row add/remove, signature transitions |
| `Query.tests.cpp` | Matching, filters, cache invalidation |
| `CommandBuffer.tests.cpp` | Recording and flush semantics |
| `Resources.tests.cpp` | Type keying, pointer stability, destruction |
| `Transform.tests.cpp` | TRS composition and the memcpy traits |
| `TransformHierarchy.tests.cpp` | Linking, walking, cascade destroy, staleness |
| `System.tests.cpp` | Phase ordering, lifecycle, spawn visibility |
| `Paths.tests.cpp` | Executable-relative path resolution |

Logging and asserts are untested - every other test exercises them, and their
failure mode is loud.

## Open questions

- 128 component types is arbitrary. Nothing is near it.
- Whether systems should be forbidden from calling `World::AddComponent`
  directly, rather than it being convention.
