# 0024 - Runtime queries walk archetypes through friendship, not a public accessor

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

`world:each("Transform", "Velocity")` cannot use `Query<Ts...>`: its terms are a
template parameter pack and its column cache is a `std::array` sized at compile
time. A runtime query needs the same match test over a runtime-sized term list.

Whatever walks archetypes needs `World::Archetypes`, which is `protected` with
only `Query` friended, and it needs `BeginQueryIteration`/`EndQueryIteration`,
which are protected for the same reason.

That pairing is not incidental. `Query::ForEach` hands its callback a reference
derived from a cached column pointer and a row index. An add moves the entity to
another table and swap-removes its old row, so an unvisited entity slides into a
row already passed - silently skipped - while the walk runs on past the shortened
table. The iteration depth is what makes `AssertNoStructuralChange` able to see
that coming.

## Decision

Runtime queries get in by friendship, and the friendship is confined to two
`detail` helpers rather than granted to the query classes:

- `detail::ArchetypeMatcher` is the **only** walker of `World::Archetypes`. It
  owns the signature masks, the match test, the Or-clause semantics and the
  generation cache. `Query<Ts...>` and `RuntimeQuery` both drive it and differ
  only in what they cache per match.
- `detail::QueryIterationGuard` is the **only** way to raise the world's
  iteration depth. Both walkers hold one for the duration of a table walk.

`RuntimeQuery` itself is friended by nothing. It reaches archetypes through the
matcher and the guard, and everything else it touches - `Archetype::FindColumn`,
`RowCount`, `EntityAt` - is already public.

`RuntimeQuery` is owned by its caller, unlike `Query`, which `World` caches.

## Consequences

- The match test, the Or semantics and the "rescan only when an archetype
  appeared" rule have one implementation. A fix to any of them lands once.
- A walker cannot forget to raise the depth, because raising it *is* constructing
  the guard, and constructing the guard is the only way to walk.
- `RuntimeQuery::ForEach` hands out `std::span<void *const>` valid for that call
  only, and the terms are `void *` - the caller casts, or reads through
  `FieldDesc`. There is no type checking left at that boundary; the constructor's
  registry check is the last one.
- Every term must be a registered table component, checked in the constructor
  with `MTS_CHECK`. Without it a sparse term would be accepted and then match
  nothing, because a sparse component owns no signature bit.
- No caching for runtime queries. Building one walks the archetype map, so a
  script host that re-runs the same query every frame must hold onto it. That is
  a deliberate hand-off: `World` would need a second map keyed by a hash of the
  term list, and the host knows which queries recur.
- One `std::vector<void *>` allocation per `ForEach` call for the row scratch.
  Per call, not per row, and it keeps the query re-entrant from inside its own
  callback the way `Query` is.

## Alternatives considered

- **Make `Archetypes()` public.** Zero coupling, and it forces
  `BeginQueryIteration`/`EndQueryIteration` public too, since a caller holding
  column pointers must raise the depth. Correctness would then rest on every
  future caller remembering to pair them - the exact silent corruption the guard
  exists to prevent - and it hands out a mutable `Archetype &`, so a caller could
  `AddColumn` on a live table.
- **A second copy of the match logic in `RuntimeQuery`.** What friendship alone
  would have given. Two copies of the Or-clause semantics and the rescan trigger
  is the cost that actually shows up later.
- **Teach `Query<Ts...>` runtime terms.** Its column cache is `std::array<_,
  kTermCount>`; a runtime one needs a `vector`, so it is a separate type either
  way. Sharing the matcher is what that idea was really after.
- **Cache runtime queries in `World`.** A second map keyed by a hash of the sorted
  term list, for a caller that is better placed to key it.

## Revisit when

The scripting layer is re-resolving the same query every frame and it shows in a
profile. The fix is a cache in the script host first; a `World`-side one only if
more than one subsystem wants the same queries.
