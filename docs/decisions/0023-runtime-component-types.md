# 0023 - Scripts may declare component types, and a name keeps its bit forever

- **Status:** Accepted
- **Date:** 2026-09-02
- **Deciders:** Sumin Park

## Context

A scripting layer that can only attach components a C++ programmer already
declared is not much of a scripting layer. Letting a script declare its own turns
component types from a closed set the author can count into an open one fed by
data, and three things in the ECS assumed the closed set.

`kMaxComponentTypes` was 128 and `ComponentBit` uses `TypeId::seq` *directly* as
a `std::bitset` index. `World::AddTableComponent` was a template, so there was no
way to add a component with no C++ type. And `seq` came from `NextSeq()`, a
monotonic counter with no reuse.

That last one is the dangerous one. If every script reload allocates fresh
`seq`s, reloading a script five times burns five bits and a session hits the
ceiling. The obvious repair - a free list - is worse: reclaiming a bit needs
every archetype holding it torn down, every sparse storage dropped, and every
cached `Query`'s mask invalidated first. A query that survived with a recycled
bit matches the wrong tables and reports nothing wrong.

## Decision

Scripts may declare component types. `ComponentRegistry::RegisterRuntime` takes a
name and a field list, computes the layout, and binds the erased `World::*Raw`
operations.

- **A name keeps its `seq` for the life of the process.** Registering a name
  already known returns the existing entry rather than allocating a second bit.
  The budget is charged per distinct name ever seen, not per registration, so a
  hot-reload is free and the archetypes already built out of that component keep
  meaning what they meant. There is no free list and no reclamation.
- **`kMaxComponentTypes` is 256.** A static bitset still: `Signature` is a
  by-value key copied on every archetype transition, so heap-allocating it would
  cost far more than the extra 16 bytes. The raise costs two more words hashed
  per archetype map lookup and two more ANDs per match test.
- **The budget is enforced with `MTS_CHECK`, in every build.** The name comes
  from data, so exceeding it must stop a Release build with a message rather than
  corrupt the bitset. `ComponentRegistry` checks at allocation, and
  `ComponentBitOf` checks again on use - not a debug-only backstop, because a C++
  component that is never registered draws its `seq` straight from `TypeIdOf<T>`,
  which has no check at all. Left as an assert, Release would reach
  `std::bitset::set` and throw `std::out_of_range` out of `World::AddComponent`,
  in a codebase that does not use exceptions. It costs one compare against a
  constant.
- **The erased path refuses a sparse component.** `World::AddRaw` cannot recover
  `ComponentStorageInfo<T>` from a TypeId, and would give a sparse component a
  table column that `World::Has<T>`, `Query<T>` and `RemoveComponent<T>` all keep
  missing. `HasRaw` cannot catch it - a sparse component owns no signature bit,
  so it answers false every time - so `ComponentRegistry` publishes the sparse
  seqs into a process-wide mask that the raw API tests instead.
- **Table storage only.** `SparseSetStorage<T>` is a `std::vector<T>` and cannot
  be built without `T`. Sparse is an optimisation for churny components, not
  something scripting needs, so a sparse hint from a script is refused rather
  than half-supported.
- **`World`'s erased path is the implementation, the template is a façade.**
  `AddTableComponentRaw` takes `(TypeId, size, align, const void *)` - exactly
  what `ComponentColumn`'s constructor has always taken - and `AddTableComponent<T>`
  forwards to it. One archetype move, not two that drift.
- **Fields are laid out in declaration order**, each padded up to its own
  alignment, the whole rounded up to the widest. A fieldless tag still occupies
  one byte, because `ComponentColumn::Count` divides by the element size.
- **Re-declaring a name with a different field list is refused.** Live archetypes
  hold rows of the old layout.

## Consequences

- A script can declare, attach, read, write and query its own components, and
  they sit in the same archetype tables as the C++ ones. A `RuntimeQuery` may mix
  the two.
- Hot-reload is cheap and safe *for an unchanged component*. Changing a
  component's fields needs the world restarted - the error message says so.
  Migrating live rows by matching field names is the obvious next step and is not
  implemented.
- Every `FieldKind` is trivially copyable and at most 4-byte aligned, so a
  script-declared component satisfies [0006](0006-pod-components.md) by
  construction and can never be over-aligned for `ComponentColumn`. The price is
  that a script component holds no strings and no handles to anything but
  entities.
- 256 bits is not a lot if scripts declare freely. It is a one-line change in
  `Signature.h`, and the failure is now a clear abort naming the component that
  overflowed rather than a corrupted signature.
- A script component defaults to all zeroes, which is a valid value for every
  `FieldKind` but is not the same thing as a C++ component's `T{}`.

## Alternatives considered

- **A `seq` free list with reclamation** - keeps the budget genuinely bounded, at
  the cost of invalidating every archetype, storage and cached query holding the
  bit. A missed invalidation is a query silently matching the wrong tables, and
  reload was the only case that needed it - which the permanent-name rule solves
  outright.
- **A dynamic `Signature` (`std::vector<uint64_t>`)** - removes the ceiling and
  puts an allocation on the by-value key of the archetype map, on every
  transition, plus a hand-written hash.
- **No script-declared components** - scripts attach only C++ components. Simpler
  and it is the version this decision replaces before it shipped; it makes every
  new gameplay concept a C++ edit and a rebuild, which is the thing scripting is
  for.
- **A generic `ScriptData` blob component** keyed by script - one bit for
  everything, but it is invisible to queries, so no system could ever select on
  it and the archetype machinery would go unused by exactly the code that needs
  it most.

## Revisit when

A script component's field list has to change without restarting the world -
which is the moment a designer edits a script while the game runs. That needs row
migration: for every archetype holding the type, build a replacement column and
copy field-by-field by name.
