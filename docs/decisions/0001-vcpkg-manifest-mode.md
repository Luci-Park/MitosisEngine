# 0001 - vcpkg in manifest mode, not vendored

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

The engine needs spdlog, GLFW, GLM, Vulkan, volk, VMA and Catch2. Building those
per machine by hand is a day of work each and guarantees version drift.

## Decision

Dependencies live in `vcpkg.json` with a pinned `builtin-baseline`, installed by
vcpkg in manifest mode during configure. vcpkg itself is not vendored: each
developer bootstraps one clone and sets `VCPKG_ROOT`.
`cmake/VcpkgToolchain.cmake` resolves the toolchain before `project()` and fails
with instructions when it cannot. Editing `vcpkg.json` re-runs configure.

## Consequences

- One vcpkg and one binary cache per machine, shared across projects.
- Small repository, no submodule.
- Setup has a manual step; skipping it produces a configure error, so that error
  text is the documentation.
- Everyone resolves against the same baseline SHA.

## Alternatives considered

- **Vendor vcpkg as a submodule** - reproducible without setup, but a full copy
  per project, no shared cache, submodule friction.
- **FetchContent** - builds every dependency from source into every build tree.
- **System packages** - not workable on Windows.
