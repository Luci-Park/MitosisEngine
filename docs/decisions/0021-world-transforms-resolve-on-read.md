# 0021 - World transforms resolve on read

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

Once transforms are parented ([0020](0020-scene-graph-as-a-resource.md)), a
child's world matrix depends on its parent's, and something has to decide when
that dependency is discharged.

The usual answer is a cached world matrix with a dirty flag, refreshed by a pass
in a known phase. It works, and it makes freshness a property of the **schedule**:
anything reading between a write and that pass sees a stale matrix. Correctness
then depends on every system, script and tool knowing where it sits in
[0008](0008-phase-ordering.md)'s phase order - a rule that is remembered rather
than enforced, and one that a gameplay query, a raycast or an editor action will
eventually break.

Dirty flags have a second cost. A moved parent invalidates its whole subtree, so
either the write walks every descendant, or every descendant polls.

## Decision

A world transform is **derived on read**. `ResolveWorld(world, entity)` refreshes
that entity and every stale ancestor before returning, so a read cannot observe a
stale value - at any point in the frame, in any phase, with no ordering
discipline required from the caller.

- `Transform` is the authored local TRS. Its members are private and every
  mutator bumps `mVersion`. `World::Get` hands out a raw `T*`, so there is no
  ECS-side hook to stamp a write; the component is the last place the invariant
  can be enforced rather than remembered.
- `WorldTransform` caches the matrix plus three stamps and a dirty flag. It is
  clean only if it is not dirty, its `localVersion` still matches
  `Transform::Version()`, and its `parentVersion` still matches the parent's
  `WorldTransform::Version()`.
- **A write is O(1).** A moved parent bumps only its own version and touches no
  descendant. A child notices on its next read that the stamp no longer matches
  and rebuilds then, so staleness flows downward lazily, along the chains
  something actually asks about.
- Dirtiness is an explicit flag, not a reserved version value. A pivot node that
  has a `WorldTransform` and no `Transform` legitimately stamps `localVersion`
  0, so 0 cannot also mean "invalid".
- Reparenting invalidates the child's cache explicitly. Versions are per-entity
  counters, so two freshly built parents are both at 1 and the stamps alone
  cannot see a reparent.
- `TransformPropagateSystem` resolves everything once per frame in `PostUpdate`.
  That is a **batching optimisation, not the correctness mechanism**: afterwards
  every `WorldTransform` is current, so downstream read-only systems may read
  `Matrix()` directly. Visit order inside the pass is irrelevant, because
  `ResolveWorld` refreshes a node's ancestors before the node.

## Consequences

- The property this exists for: a mid-frame read is always right. Nothing has to
  reason about phases to be correct, only to be fast.
- Writes do not scale with subtree size, and N writes to one entity in a frame
  coalesce into one rebuild at the next read.
- A clean read costs a flag and two integer compares per level, and no matrix
  maths.
- **`ResolveWorld` mutates on read**, so it is neither `const` nor thread-safe.
  The bulk pass is what lets later parallel systems read the cache instead.
- **`Transform` must be written through its setters.** A raw write leaves every
  descendant stale permanently. This is why the members are private, and why
  reflection over raw field offsets is not usable on this type.
- `WorldTransform` is derived. Exposing a setter would break the contract, so its
  stamps are private behind a friend.
- An entity with a `Transform` and no `WorldTransform` still resolves correctly,
  just uncached. An uncached ancestor marks the rest of the chain non-cacheable,
  so no descendant trusts a stamp that cannot move.
- Versions are `uint32_t` and wrap, skipping 0 so "root" stays distinguishable.
  Four billion writes to one entity is not a reachable state.
- `WorldTransform` is ~80 bytes, against 44 for `Transform`.
- There is no change detection for consumers. Nothing tells a renderer which
  transforms moved this frame, though `WorldTransform::Version` is the obvious
  hook if that is wanted.

## Alternatives considered

- **Eager write-through** - recompute the subtree on every write. Also always
  correct, but O(subtree) per write, needs a downward child list at write time,
  and coalesces nothing: a physics step writing 10,000 transforms triggers 10,000
  subtree walks.
- **Dirty flag plus a scheduled pass only** - the standard design, and the one
  this rejects. It makes a correct read a scheduling property, which is exactly
  the discipline that was not wanted.
- **No cache at all - always recompute from TRS** - no staleness of any kind and
  no stamps to maintain, but every query pays O(depth) matrix multiplies with no
  reuse between the many readers of the same entity in a frame.
- **Author the world matrix directly** - removes derivation entirely, but then
  parenting cannot work, and a hand-edited matrix can drift into a sheared or
  non-affine state that TRS cannot represent.

## Revisit when

- Transform work is parallelised. `ResolveWorld` mutating on read is the blocker:
  the bulk pass would have to become the only resolver, with every downstream
  read `const`.
- Something needs "what moved this frame". That is change detection, and
  `WorldTransform::Version` already carries the information.
- The full sweep shows up in a profile. Depth-sorted iteration and dirty-subtree
  skipping are both available and both deliberately skipped for now.
