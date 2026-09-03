# Decision records

One file per choice that constrains how others write code, with the reasoning
that produced it and the alternatives turned down.

0001-0016 were backfilled on 2026-09-01 from reasoning already in the code; their
dates are when they were written, not when the choice was made. 0017 was decided
that day.

## Index

| # | Decision |
|---|---|
| [0001](0001-vcpkg-manifest-mode.md) | vcpkg in manifest mode, not vendored |
| [0002](0002-module-per-library.md) | One static library per module |
| [0003](0003-cmake-presets-vscode.md) | CMake presets and VS Code as the supported workflow |
| [0004](0004-scaffolding-scripts.md) | Scaffolding scripts instead of hand-written boilerplate |
| [0005](0005-archetype-plus-sparse.md) | Archetype tables with opt-in sparse sets |
| [0006](0006-pod-components.md) | ECS components are POD |
| [0007](0007-deferred-structural-change.md) | Structural change deferred through a command buffer |
| [0008](0008-phase-ordering.md) | System ordering by phase only |
| [0009](0009-app-composition-root.md) | App is a fixed-member composition root |
| [0010](0010-cooked-assets.md) | Assets cooked offline, addressed by path hash |
| [0011](0011-slang-shaders.md) | Slang shaders, no fallback compiler |
| [0012](0012-vulkan-1-3-baseline.md) | Vulkan 1.3 core feature baseline |
| [0013](0013-surface-provider.md) | The renderer sees a surface provider, never a window |
| [0014](0014-logging-and-asserts.md) | Logging and asserts are macros over a private spdlog |
| [0015](0015-catch2-per-module-tests.md) | Catch2 and CTest, per module |
| [0016](0016-manual-formatting.md) | Formatting is manual |
| [0017](0017-member-naming.md) | Members are named `mPascalCase` |
| [0018](0018-game-definition.md) | A game is data: config, assets and Lua scripts |
| [0019](0019-world-resources.md) | Per-world state lives in typed resources |
| [0020](0020-scene-graph-as-a-resource.md) | Scene structure lives in a resource, and destruction cascades |
| [0021](0021-world-transforms-resolve-on-read.md) | World transforms resolve on read |

All Accepted.

## Writing one

Copy [TEMPLATE.md](TEMPLATE.md) to `NNNN-short-kebab-title.md` with the next free
number, open it as `Proposed`, and add it to the index. It becomes `Accepted`
when the change merges.

- One decision per record.
- Immutable once accepted. To change course, write a new record and mark the old
  one `Superseded by NNNN`.
- Always fill in the alternatives - that section is what stops the argument
  recurring.
- Write one when a choice is hard to reverse or hard to guess: a dependency in a
  public header, a module boundary, a data format, an ordering guarantee, a rule
  everyone follows. Not for local implementation choices.
- An unresolved record says `Open` in its status and the index.

Statuses: `Proposed`, `Accepted`, `Open`, `Superseded by NNNN`, `Deprecated`.
