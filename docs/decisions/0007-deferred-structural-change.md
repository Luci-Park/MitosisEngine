# 0007 - Structural change is deferred through a command buffer

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Adding or removing a component moves an entity between archetypes; destroying one
swaps another row into its place. Either during a query walk invalidates the
walk - the most common way to corrupt an ECS, and it fails intermittently.

## Decision

Systems record structural changes on the `CommandBuffer` in their
`SystemContext`. `Add` copies the value into the buffer's storage; each command
carries a function pointer that applies it. `SystemScheduler` flushes at phase
boundaries, never mid-iteration. Commands apply in order, last writer wins.

`World::CreateEntity` stays immediate - a caller needs the handle to record
components against it. The entity exists at once, in the empty archetype, with no
components until the flush.

## Consequences

- Iteration is safe by construction: never mutate structure inside `ForEach`.
- A spawn is observable at the next phase boundary, not the next statement.
- The buffer copies payloads, so a large component costs a copy per queued add.
- Direct `World::AddComponent` still exists for setup code. Nothing forbids a
  system from calling it; that is convention, with a Debug-only iteration guard
  catching the worst case.

## Alternatives considered

- **Immediate everywhere** - simple until the first crash, which lands far from
  its cause.
- **Defer creation too** - uniform, but a system could not attach components to
  something it just spawned without a placeholder handle scheme.
- **Flush per system** - stronger isolation, but every system sees a different
  world within a phase, making ordering matter more than it should.
