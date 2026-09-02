# 0009 - App is a fixed-member composition root

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Something must own the window, renderer, world, scheduler and asset cache and
drive the frame loop. The usual engine answer is a generic subsystem registry,
giving initialisation order and lookup by type for free.

## Decision

`App` holds its subsystems as named members, declared in an order that makes
reverse-order destruction correct, exposed through named accessors. No registry,
no service locator. `Initialize` returns `bool`. Asset loading is lazy and its
failure sticky.

## Consequences

- Order, dependencies and lifetimes are visible in one header - worth more than
  extensibility at this size.
- The compiler checks everything; a missing subsystem is a compile error.
- Adding a subsystem means editing `App`, which forces a look at where it belongs
  in the order.
- Aliasing between members (the cache points into the manifest) is handled by
  declaration order plus a comment.

## Alternatives considered

- **Generic subsystem registry** - initialisation order becomes data and lookups
  become runtime failures, buying nothing at five subsystems.
- **Global singletons** - untestable, implicit lifetimes.

## Revisit when

Something outside the engine needs to inject or replace a subsystem - a headless
renderer for tests, or a second executable with a different window backend.
