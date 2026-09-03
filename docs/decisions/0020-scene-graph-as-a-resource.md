# 0020 - Scene structure lives in a resource, and destruction cascades

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

Parenting needs both directions. Resolving a world transform walks *up*, so a
child needs its parent. Gameplay queries, subtree destruction and ordered
traversal walk *down*, so a parent needs its children. Two directions means two
encodings of the same edge, and two encodings can disagree.

The first attempt put both in a component. [0006](0006-pod-components.md) forbids
`std::vector` in a component, so the child list became an intrusive chain:
`Hierarchy { parent, firstChild, nextSibling, prevSibling }`, all POD. It was
correct in isolation and failed repeatedly in place:

- `RemoveComponent<Hierarchy>` on a mid-chain node truncated the parent's child
  walk, silently dropping every later sibling. `World::RemoveComponent` is a
  template that deletes invariants it has never heard of, and no hook can
  intercept it.
- A `World::DestroyEntity` that bypassed the helper left a dead handle mid-chain,
  with the same silent truncation.
- `AddTransform` attaching without detaching first put an entity in two parents'
  chains at once.

Every one was a wrong answer rather than a crash. The lesson was not that the
redundancy was wrong - it is what makes both walks cheap - but that **storing it
where the generic ECS can independently mutate it** was. [0019](0019-world-resources.md)
created somewhere else to put it.

## Decision

Scene structure lives in `HierarchyIndex`, a `World` resource. **No component
holds an edge.**

- A node is `{owner, parent, std::vector<Entity> children}`, in a dense array
  keyed by `Entity::mIndex`. The slot stores the whole owner handle, so a
  recycled index self-invalidates on a generation compare rather than inheriting
  the previous occupant's edges.
- Mutators are private, reached only through `detail::HierarchyMutator` and so
  only through `mts::SetParent`, which pairs every structural change with the
  `WorldTransform` invalidation that a reparent needs.
- `SetParent` **refuses and returns false** - it does not assert - on
  self-parenting, a cycle, or a chain that would pass `kMaxHierarchyDepth`
  counting the moved subtree's own height. Asserts compile out under `NDEBUG`,
  and one cycle makes every upward walk in the engine non-terminating, so the
  graph is bounded and acyclic in *every* build.
- **Destruction cascades.** `InstallHierarchy` registers a `World` destroy hook,
  so `DestroyEntity` destroys the whole subtree - and so does
  `CommandBuffer::Destroy`, which flushes through the same call. There is no
  separate `DestroyHierarchy`.
- Children are in insertion order, and that order is stable.

`World` gained a generic destroy-hook list to make this possible. It still knows
nothing about hierarchy.

## Consequences

- The desync class is gone structurally, not patched. There is no component for
  `RemoveComponent` to reach.
- Children can be a `std::vector`, which is only possible because a resource is
  not a component.
- Reparenting touches no component, so it never moves an entity between
  archetypes.
- Stable child order is a guarantee serialisation and scripting may rely on. It
  costs the option of swap-and-pop erase, so `SetParent` is O(children).
- Cascade destroy is O(total nodes) only because `TakeChildren` detaches a whole
  child list in one pass. Removing them one at a time makes each `Unlink` rescan
  and shift the parent's vector - measured at 279 ms to destroy 40,000 children,
  against 3.2 ms after.
- **You cannot query "entities with a parent" by signature.** Structure has no
  signature bit any more.
- `ForEachChild` and `ForEachDescendant` snapshot before visiting, so a callback
  may destroy or reparent freely. That is one allocation per call.
- `kMaxHierarchyDepth` (64) sizes a stack array in `ResolveWorld`. The bound
  checks the new parent's depth plus the moved subtree's height, so a legal graph
  can never make that walk truncate.
- **Cascade is the only policy.** Detaching a subtree instead of destroying it -
  dropping a weapon when its holder dies - is not expressible without adding one.
- A destroy hook now runs on every `DestroyEntity`, in every world, including
  those with no hierarchy.

## Alternatives considered

- **Intrusive links in POD components** - what was tried. O(1) splice and no
  allocation, but `RemoveComponent` or a raw destroy corrupts it silently, and
  nothing in the ECS can prevent either.
- **Parent-only component, child index rebuilt each frame** - one source of
  truth and zero desync, but the index is stale the moment anything reparents
  mid-frame, which is exactly when gameplay asks. There was also nowhere to put
  it before 0019.
- **A `std::vector<Entity>` children component** - the obvious shape, forbidden
  by 0006, which is what forced the intrusive chain in the first place.
- **Teaching `World::DestroyEntity` to cascade directly** - puts a scene concept
  inside the container, and every world without a hierarchy pays for it. The
  hook keeps `World` generic.
- **Fixed-capacity child array in a component** - POD-legal, but picks a fan-out
  limit that some rig will exceed.

## Revisit when

- Reparenting appears in a profile. `SetParent` is O(children); tombstones plus a
  stored index would make it O(1) without giving up contiguous iteration.
- Something needs detach-on-destroy. That means a cleanup policy per relation
  rather than one hardcoded behaviour.
- A second relation appears - `AttachedTo`, `OwnedBy`, `DockedTo`. Generalise the
  index into typed relations instead of copying it.
