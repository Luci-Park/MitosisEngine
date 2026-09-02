# 0002 - One static library per module

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

The engine is a learning vehicle as much as a product: each part should be
understandable and replaceable without reading the rest. One monolithic target
makes dependencies invisible and lets any file include any other.

## Decision

Every part is its own static library under `modules/<name>/`, created by
`engine_add_module(<name>)`: target `engine_<name>`, alias `mts::<name>`,
`include/` public. Public headers in `include/<module>/`, everything else in
`src/`. Modules link only through `mts::` aliases. Third-party libraries are
`PRIVATE` unless one of their types appears in a public header.

## Consequences

- Dependencies are declared, visible per module, and checkable.
- A module can be tested against its public headers alone.
- A cycle becomes a link error rather than slow architectural rot.
- More CMake per feature, and longer include paths. The scaffolding absorbs most
  of it.

## Alternatives considered

- **One engine library** - less plumbing, but layering would exist only in
  comments.
- **Shared libraries** - export macros and DLL copying, for hot reload we do not
  use.
- **Header-only** - compile times grow with every consumer, nowhere to hide an
  implementation.
