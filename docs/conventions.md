# Conventions

Rules. How to apply them to a specific task is in [extending.md](extending.md).

## Layout

One static library per module, in `modules/<name>/`, named lowercase
`[a-z][a-z0-9_]*`.

`include/<module>/` is the public API and `src/<module>/` is everything else. A
header's include path mirrors its directory, so `core/ecs/World.h` sits at
`modules/core/include/core/ecs/World.h`.

Tests go in `modules/<name>/tests/`, named `<Subject>.tests.cpp`.

Create files and modules with the scaffolding rather than by hand. The two
mirrored trees and the `target_sources` edit are the parts that get forgotten,
and the symptom arrives later as a link error.

Every `.cpp` that needs to be compiled must be listed in `target_sources`.
Nothing is globbed, so a file left off that list is never compiled and its
symbols never reach the link.

## Naming

| Kind | Form | Example |
|---|---|---|
| Namespace | lowercase, all in `mir` | `mir::detail` |
| Type | `PascalCase` | `AssetCache` |
| Interface | `I` prefix | `ISystem` |
| Function, method | `PascalCase` | `CreateEntity` |
| Local, parameter | `camelCase` | `imageIndex` |
| Member | `mPascalCase` | `mSwapchain` |
| Constant, `constexpr` | `k` prefix | `kFramesInFlight` |
| Macro | `MIR_SCREAMING_CASE` | `MIR_ASSERT` |
| CMake function | `engine_snake_case` | `engine_add_module` |
| CMake target | `engine_<module>`, alias `mir::<module>` | `engine_core` |
| File | `PascalCase.h`/`.cpp`, matching its main type | `VulkanRenderer.cpp` |

Members take `mPascalCase` engine-wide, including the fields of a `*Desc`
struct. The prefix separates a member from a parameter at a glance, which the
Vulkan code leans on heavily.

Plain-data types that describe a **layout** are the exception and take no
prefix — `AssetBlobHeader`, `AssetManifestEntry`, component fields. Those names
are the format itself, so renaming one changes the file it describes.

## Includes and formatting

Include groups go in this order, with a blank line between each and alphabetical
order within:

1. Own module, quoted: `#include "TypeId.h"`
2. Other modules, angled: `#include <core/log/Log.h>`
3. Third party
4. Standard library

Headers use `#pragma once`.

Don't forget header formats for each file. Check [build.md](build.md#3-setting-name-in-headers-optional)
```
/**
 * @file App.h
 * @author Sumin Park
 * @brief Owns the engine subsystems and drives the main loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
```

`templates/.clang-format` sets `DisableFormat: true` and `SortIncludes: Never`,
so an editor with format-on-save does nothing. The Vulkan struct initialisers
and the log macro tables are laid out by hand to be read as tables, and a
formatter rewrites them. Sorting includes alphabetically across the whole file
also destroys the group order above, which is what makes a file's dependencies
visible.

## Comments

Plain `//` for everything, whether it documents a declaration or notes something
inline.

## ECS

Components are POD. Put `MIR_ASSERT_COMPONENT(T)` next to the declaration, so a
violation is a compile error at the offending struct rather than deep inside a
template instantiation.

Component names must be globally unique. `TypeIdOf` strips namespaces and hashes
the bare name, so two `Position` structs in different namespaces collide. A
collision asserts in Debug.

A component with no fields is a tag. It gets one signature bit and no column.
Add it with `AddTag<T>`, and filter on it with `With`, `Without` or `Or`. A tag
has nothing to read, so naming it as a query data term is a compile error.

Never add, remove or destroy a component while iterating. An add moves the entity
to another table and swap-removes its old row, so an entity you have not visited
slides into a row you already passed and is skipped silently. Record the change
on the `CommandBuffer` instead.

Systems cache their queries in `OnStart`.

Ordering between systems is a phase, never registration order. Two systems in the
same phase that need a specific order should not be in the same phase.

Read-only components are `const T` in the query. Queries then carry the access
information a parallel scheduler will need.

There are at most 256 component types, counting C++ and script-declared ones
together. Raising that is a deliberate edit to `Signature.h`.

Register a new C++ component in its module's `ComponentRegistration.cpp`. Give it
a field table if scripts, the inspector or serialization should reach its values.
Forgetting to register is not an error — it produces a name that nothing can find
— and a component with no field table serializes as present but empty.

Register the C++ components before loading any script. A name belongs to whoever
claims it first.

## Errors

`MIR_ASSERT` is for programmer error and compiles out in Release. Its condition
is not evaluated there at all, so nothing inside it may have a side effect the
program depends on.

`MIR_CHECK` stays in Release. Use it whenever the bad input can come from data
rather than from C++

Recoverable failure returns `false`, `nullptr` or `std::optional`, and logs once.
Initialization returns `bool` and leaves the caller to decide whether that is
fatal.

Engine code uses no exceptions, and nothing outside `tools/` uses `iostream`.

Log levels: `Trace` and `Debug` for per-frame detail, `Info` for lifecycle,
`Warn` for degraded, `Error` for a failed operation, `Critical` for
unrecoverable. Anything below `Info` compiles out in Release, so `Trace` and
`Debug` are free to leave in hot paths.

## Ownership

`unique_ptr` owns, a raw pointer observes, and a reference is required to be
non-null.

A type that owns a resource deletes copy. It also deletes move, unless move is
genuinely correct for it.

A member that aliases another member is declared in destruction order, with a
comment saying so. `App::mAssetManifest` and `mAssetCache` are the example.

## Dependencies

Everything comes through `vcpkg.json`. Nothing is vendored or fetched at build
time.

Link `PRIVATE` unless one of the library's types appears in your public header,
and say why in a comment. A `PUBLIC` link puts that library's include path on
every consumer of your module, transitively.

`third_party/imgui` is the one exception. It is a git submodule.

## Tests

Catch2 v3 through `engine_add_module_tests`, one `TEST_CASE` per behaviour,
exercised through the module's public header.

A module's test executable links only that module. A test that needs a second
module is a signal that the boundary is in the wrong place.

New logic in `core`, `assets`, `scene` and `script` ships with tests. `renderer`,
`window` and `editor` need a device and a display, and are verified by running.

Run `ctest --test-dir builds/windows-msvc-debug --output-on-failure` before
pushing.

## Changed

*2026-09-01* — **Members are `mPascalCase`.** Was: three conventions, roughly by
the age of the code. `core/ecs` and `assets` used `mPascalCase`, `renderer` used
`m_PascalCase`, and `app`, `window` and the `*Desc` structs used `m_camelCase`.
Now: one form everywhere. `mPascalCase` won because it was already the largest
and newest body of code, so the least code had to move. The rename landed as
isolated mechanical commits listed in `.git-blame-ignore-revs`; enable it per
clone with `git config blame.ignoreRevsFile .git-blame-ignore-revs`.
