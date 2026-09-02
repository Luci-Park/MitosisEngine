# 0008 - System ordering by phase only

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Systems need a defined order. The general solution is a dependency graph over
declared component access, which also buys automatic parallelism - and is a large
machine to build before anything uses it.

## Decision

`SystemPhase` is the only ordering primitive: `PreUpdate`, `Update`,
`PostUpdate`, reserved `Render`. The scheduler runs a phase in registration
order, flushes, then moves on. Single-threaded. Ordering between features is
expressed as a phase, never as registration order across unrelated features.

## Consequences

- The frame is a flat, deterministic list, easy to debug.
- Registration order within a phase is an implementation detail; two systems in
  one phase that need an order should not be in one phase.
- Four phases is coarse. Outgrowing it means more phases or a dependency graph -
  a new decision either way.
- No parallelism, which is why queries already take read-only components as
  `const T`.
- `Render` is unused; `App` drives the renderer directly.

## Alternatives considered

- **Dependency graph over component access** - the eventual answer for
  parallelism, too much machinery now, and hard to validate without real systems.
- **Before/after constraints per system** - more expressive, but turns ordering
  into a distributed puzzle instead of a visible pipeline.
- **Plain registration order** - what phases exist to avoid.
