# Architecture

State as of 2026-09-01. Update the "Current state" lines when you change them.

An archetype ECS, a Vulkan 1.3 renderer, an offline asset pipeline, and a thin
app shell. Every part is its own static library.

## Layout

```
CMakeLists.txt      root build: options, module list, HelloWorld
CMakePresets.json   the only supported way to configure
vcpkg.json          dependency manifest
cmake/              helpers: modules, platform, shaders, assets, vcpkg
modules/<name>/     include/<name>/ (public API), src/<name>/, tests/
games/<name>/       planned: config, assets/ and Lua scripts/, no C++
tools/              AssetCooker, scaffolding scripts
templates/          what the scaffolding stamps out
assets/             source assets, cooked into the build tree
builds/             build trees, gitignored
```

## Module graph

```
        HelloWorld (main.cpp)
               |
              app
     +---------+-----+------+
     |         |            |
  assets    window      renderer
     |         |            |
     +---------+------------+
               |
              core
```

- `core` depends on nothing in the engine; everything may depend on it.
- No module reaches into another's `src/`. `include/<module>/` is the API.
- `app` is the only module that knows all the others.
- `renderer` never sees `window` - it takes an `ISurfaceProvider`.
- Third-party libraries link `PRIVATE` unless one of their types is in a public
  header (`spdlog` is private to `core`; `volk` is public in `renderer`).

## core

No engine dependencies. Links `spdlog` privately.

- `ecs/` - see below, and [modules/core.md](modules/core.md) for detail.
- `log/` - `MTS_LOG_*` over spdlog with a compile-time floor (Trace in Debug,
  Info in Release); `MTS_ASSERT`/`MTS_VARIFY` (Debug) and `MTS_CHECK` (always).
- `fs/Paths.h` - executable-relative paths, so a copied build tree runs.
- `platform/Surface.h` - `NativeWindowHandle`, `ISurfaceProvider`.

Entities are `{index, generation}` handles, the generation guarding against ABA.
Components are POD, in one of two storages: archetype tables by default, sparse
sets for churny components via `MTS_COMPONENT_SPARSE(T)`.
`Signature` is a 256-bit bitset over sequence-numbered types and keys the
archetype map. `Query<Ts...>` caches matching archetypes, re-resolving when the
world's archetype generation changes; filters are `With`, `Without`, `Or`.

Structural changes go through `CommandBuffer`; `CreateEntity` stays immediate.
`SystemScheduler` runs systems by phase - `PreUpdate`, `Update`, `PostUpdate`,
reserved `Render` - flushing at each boundary.

*Current state:* single-threaded. No dependency graph, change detection or
component lifecycle hooks. `Render` phase unused.

## window

`Window` is an interface (`PollEvents`, `ShouldClose`, `Create`) that also
implements `ISurfaceProvider`. GLFW backs it on desktop, split into a
cross-platform part and per-platform native handle extraction. GLFW is private.

*Current state:* creation, event pump, close flag, size, native handle. No input.

## renderer

`VulkanRenderer` holds the whole Vulkan path: instance, debug messenger, surface,
device, VMA allocator, swapchain, per-frame pools and buffers, pipeline, vertex
buffer, and frame pacing on a timeline semaphore with `kFramesInFlight = 2`.

Requires Vulkan 1.3 with `dynamicRendering`, `synchronization2`,
`timelineSemaphore`, `shaderDrawParameters`; devices missing any are rejected.
No render passes or framebuffers. `volk` is compiled from source here so the
platform defines are ours; VMA fetches its pointers through it.

*Current state:* one hard-coded triangle, swapchain recreation on resize,
RenderDoc object naming under validation. No material, mesh, camera, descriptor
or render-graph layer. `Renderer.h` is an empty placeholder and `App` uses
`VulkanRenderer` directly. Nothing connects the ECS to the renderer.

## assets

Offline pipeline: assets are cooked at build time, never at runtime.

- `AssetId` - FNV-1a 64 of the repository-relative source path, so ids match on
  every machine. Cook roots must never be absolute.
- `AssetBlob` - 32-byte header (magic, format version, type tag, content version,
  size, content hash) plus payload.
- `AssetManifest` - id to type tag, content version and path, with a shared path
  table.
- `AssetCache` - loads by id on demand, remembers failures. Move-only: the parsed
  view aliases the entry's own buffer.

`tools/AssetCooker` writes one blob per file named by its id, plus the manifest.
It recooks unless the output is both newer than the source and written by the
current blob format.

*Current state:* everything cooks as a raw passthrough blob. No typed importers,
and nothing reads cooked assets beyond the cache.

## app

`App` owns the window, renderer, `World`, `CommandBuffer`, `SystemScheduler`, and
lazily the manifest and cache. `Initialize`/`Run`/`Shutdown`, with `dt` clamped by
`AppDesc::mMaxDeltaSeconds`. Fixed members, no subsystem registry. `Assets()`
returns `nullptr` when there is no manifest, so a missing one costs a caller an
asset, not the process.

## Build system

- `engine_add_module(<name>)` - target `engine_<name>`, alias `mts::<name>`,
  `include/` public. `engine_add_module_tests(<name> <sources>)` - Catch2 binary,
  one CTest case per `TEST_CASE`.
- `EnginePlatform.cmake` - `ENGINE_PLATFORM` plus family flags, ordered so the
  overlapping cases (Emscripten sets `UNIX`, iOS sets `APPLE`) resolve correctly.
- `Shaders.cmake` / `Assets.cmake` - compile and cook into the build tree, then
  copy next to the executable so a moved build folder still runs.

## Known gaps

1. No ECS to renderer path - nothing in the world is drawn.
2. No input.
3. No renderer abstraction.
4. No typed assets.
5. No scene or serialization layer, though `TypeId::hash` exists for it.
6. No scripting. Games are meant to be Lua and data; that needs a `script`
   module, a name to `TypeId` component registry, query access from Lua, config
   and script asset types, and a runtime executable - none of which exist.
7. Linux sources, but no Linux preset.
8. No CI.
