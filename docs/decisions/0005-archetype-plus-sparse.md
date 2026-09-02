# 0005 - Archetype tables with opt-in sparse sets

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Archetype storage groups entities by their exact component set: dense iteration,
but a row move on every add or remove. Sparse sets make mutation cheap and pay in
indirection while iterating. Neither suits every component - a transform is
stable and iterated constantly, a one-frame tag is added and removed all the time.

## Decision

Both, chosen per component type at compile time. Table storage is the default;
`MTS_COMPONENT_SPARSE(T)` opts into a sparse set. `World` dispatches on
`ComponentStorageInfo<T>::kValue` with `if constexpr`, so the choice costs
nothing at runtime. Archetypes are keyed by a `Signature` bitset over
`kMaxComponentTypes` (128) sequence-numbered types.

## Consequences

- The common case - many entities, same components - iterates densely.
- Churny components avoid archetype fragmentation without a redesign.
- Two storage paths to maintain, and queries must merge both. That is where the
  complexity in `Query.h` comes from.
- Sparse components are absent from a `Signature`, so an archetype does not
  identify an entity's full component set. Code assuming it does is wrong.
- 128 types is a hard ceiling; raising it grows every signature.

## Alternatives considered

- **Pure archetype** - simpler and faster to iterate, pathological for toggled
  components.
- **Pure sparse set** - cheap to mutate, but loses the contiguity that is the
  reason to write an ECS here.
- **Runtime-selectable storage** - same power, but every access becomes a virtual
  call.
