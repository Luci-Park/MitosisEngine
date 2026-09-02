# Conventions

What review checks against. Reasoning lives in the linked decision records.

## Layout

- One static library per module, `modules/<name>/`, lowercase `[a-z][a-z0-9_]*`.
- `include/<module>/` is the public API; `src/<module>/` is everything else.
- A header's include path mirrors its directory: `core/ecs/World.h`.
- Tests in `modules/<name>/tests/`, named `<Subject>.tests.cpp`.
- Create files and modules with the scaffolding, not by hand
  ([EXTENDING.md](EXTENDING.md)).

## Includes

Groups in this order, blank line between, alphabetical within
(`SortIncludes: Never`, so by hand):

1. Own module, quoted: `#include "TypeId.h"`
2. Other modules, angled: `#include <core/log/Log.h>`
3. Third party
4. Standard library

Headers use `#pragma once`.

## Naming

| Kind | Form | Example |
|---|---|---|
| Namespace | lowercase, all in `mts` | `mts::detail` |
| Type | `PascalCase` | `AssetCache` |
| Interface | `I` prefix | `ISystem` |
| Function, method | `PascalCase` | `CreateEntity` |
| Local, parameter | `camelCase` | `imageIndex` |
| Member | `mPascalCase` | `mSwapchain` |
| Constant, `constexpr` | `k` prefix | `kFramesInFlight` |
| Macro | `MTS_SCREAMING_CASE` | `MTS_ASSERT` |
| CMake function | `engine_snake_case` | `engine_add_module` |
| CMake target | `engine_<module>`, alias `mts::<module>` | `engine_core` |
| File | `PascalCase.h`/`.cpp`, matching its main type | `VulkanRenderer.cpp` |

Members are `mPascalCase` engine-wide
([0017](decisions/0017-member-naming.md)), `*Desc` fields included. The exception
is a plain-data type describing a **layout** - `AssetBlobHeader`,
`AssetManifestEntry`, component fields - which carries no prefix, because the
names are the format.

The rename is applied but not committed; see 0017 for how to split it.


## ECS

- Components are POD. `MTS_ASSERT_COMPONENT(T)` next to the declaration.
- Component names must be globally unique - `TypeIdOf` hashes the bare name.
- `MTS_COMPONENT_SPARSE(T)` for churny components, outside any namespace, same
  header as the component.
- Never add, remove or destroy while iterating. Use the `CommandBuffer`.
- Systems cache queries in `OnStart`.
- Ordering between systems is a phase, never registration order.
- 128 component types max; raising it is a deliberate edit to `Signature.h`.

## Errors

- `MTS_ASSERT`/`MTS_VARIFY` - programmer error, out in Release (`MTS_VARIFY`
  still evaluates its condition). `MTS_CHECK` - must abort even in Release.
- Recoverable failure returns `false`, `nullptr` or `std::optional` and logs once.
  Init paths return `bool`; the caller decides if it is fatal.
- No exceptions in engine code. No `iostream` outside `tools/`.
- Levels: `Trace`/`Debug` per-frame detail, `Info` lifecycle, `Warn` degraded,
  `Error` failed operation, `Critical` unrecoverable. Below `Info` is compiled
  out in Release.

## Ownership

- `unique_ptr` owns, raw pointers observe, references are required non-null.
- Resource-owning types delete copy, and move unless move is genuinely correct.
- A member aliasing another member is declared in destruction order, with a
  comment saying so (see `App::mAssetManifest` / `mAssetCache`).

## Dependencies

Everything through `vcpkg.json`; nothing vendored or fetched at build time. Link
`PRIVATE` unless a type appears in a public header, and say why in a comment. A
dependency in a public header, or affecting every platform, gets a decision
record.

## Tests

Catch2 v3 through `engine_add_module_tests`, one `TEST_CASE` per behaviour,
exercised through the public header. New logic in `core` and `assets` ships with
tests; `renderer` and `window` need a device and a display and are verified by
running. Run `ctest --test-dir builds/windows-msvc-debug --output-on-failure`
before pushing.

