# 0006 - ECS components are POD

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Archetype storage moves rows between tables and grows columns by reallocating.
Doing that for arbitrary C++ types means per-type move, copy and destroy function
pointers called per element - or restricting what a component may be.

## Decision

Components must be trivially copyable, standard layout, and nothrow move
constructible and assignable. `MTS_ASSERT_COMPONENT(T)` goes next to the
declaration, storage asserts the same traits again, so a violation is a compile
error at the point of use. Row moves and column growth are `memcpy`.

## Consequences

- Simple, fast storage with no type-erased lifecycle tables.
- Component data is directly serialisable, which is what `TypeId::hash` is for.
- No `std::string`, `std::vector` or anything owning. Such data is referenced by
  handle and owned elsewhere.
- No constructors or destructors run, so defaults must be member initialisers.

## Alternatives considered

- **Type-erased lifecycle hooks per column** - allows any type, at an indirect
  call per element moved and a much larger `ComponentColumn`.
- **Assert only inside storage** - the error surfaces deep in a template
  instantiation rather than next to the offending struct.
