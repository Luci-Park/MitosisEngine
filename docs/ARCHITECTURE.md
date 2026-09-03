# Architecture

State as of 2026-09-03. Update the "Current state" lines when you change them.

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
     +---------+-----+------+------+
     |         |            |      |
  assets    window      renderer editor
     |         |            |      |
     +---------+------------+------+
               |                   |
              core            editortheme
```

- `core` depends on nothing in the engine; everything may depend on it.
- No module reaches into another's `src/`. `include/<module>/` is the API.
- `app` is the only module that knows all the others.
- `renderer` never sees `window` - it takes an `ISurfaceProvider`.
- `editor` owns ImGui end to end (context, backend, dockspace layout); `app`
  drives it through four calls (`BeginFrame`/`DrawLayout`/`EndFrame`/
  `SceneViewportRect`) and never touches ImGui itself.
- `editortheme` depends on nothing engine-side, only `imgui` - it is pure
  theme data (colors, spacing), applied once by `editor` at startup.
- Third-party libraries link `PRIVATE` unless one of their types is in a public
  header (`spdlog` is private to `core`; `volk` is public in `renderer` and,
  transitively through `VulkanRenderer&`, in `editor`).

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
device, VMA allocator, swapchain, per-frame pools and buffers, meshes, materials
(pipelines), depth buffer, and frame pacing on a timeline semaphore with
`kFramesInFlight = 2`.

Requires Vulkan 1.3 with `dynamicRendering`, `synchronization2`,
`timelineSemaphore`, `shaderDrawParameters`; devices missing any are rejected.
No render passes or framebuffers. `volk` is compiled from source here so the
platform defines are ours; VMA fetches its pointers through it.

- `Mesh.h` / `CreateMesh` - a `Vertex{pos, color, normal}` buffer pair per
  mesh, referenced by a generation-checked `MeshHandle`. `Shapes.h` builds
  primitives (currently a cube) as plain vectors, no Vulkan involved.
- `Material.h` / `CreateMaterial` - names a `.slang` shader plus cull/fill
  state, compiles one `VkPipeline` eagerly (at creation, not on first draw),
  returns a `MaterialHandle`.
- `components/Camera.h`, `CameraMath.h` - `fovY`/`near`/`far`; the
  view-projection is computed once per frame and premultiplied on the CPU
  into each draw item's push constant (no uniform buffer yet).
- `RenderSystem.h` - an `ISystem` in `SystemPhase::Render`, the only thing
  connecting the ECS to `VulkanRenderer`. Runs after `PostUpdate` (where
  `TransformPropagateSystem` lives) so every `WorldTransform` it reads is
  already current for the frame.
- `SetSceneViewport`/`SetImGuiDrawData` - the scene pass clips to a caller-given
  `VkRect2D` (falling back to the full swapchain when unset) so the editor's
  docked panels never get painted over; ImGui's draw data composites on top,
  unclipped. `editor` is the only caller.

*Current state:* swapchain recreation on resize, RenderDoc object naming
under validation. No descriptor sets or render-graph layer; materials/meshes
have no destroy path before `Shutdown()`. `Renderer.h` is an empty
placeholder and `App` uses `VulkanRenderer` directly.

## editortheme

Pure theme data - the Slate color palette and spacing, plus DPI rescaling for
fonts/icons. No engine dependency, only `imgui`. Applied once by `editor` at
startup; nothing else calls into it.

*Current state:* one theme, no runtime switching.

## editor

Owns the ImGui context end to end: creation, font/icon loading (Inter +
Font Awesome 6, merged into one atlas), theme application, the GLFW+Vulkan
backend, and the Slate editor shell - a dockspace with `Hierarchy`/
`Inspector`/`Output` panels docked around a passthru center, a `Debug` menu,
and a Style Editor toggle. `App` drives it through `BeginFrame` ->
`DrawLayout(enableLayout, showDemo)` -> `EndFrame() -> ImDrawData*`, and reads
`SceneViewportRect()` for `VulkanRenderer::SetSceneViewport`.

*Current state:* one fixed layout, not user-editable or serialized beyond
ImGui's own `imgui.ini`. No inspector content, hierarchy content, or output
log wired up yet - the panels are empty.

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

`App` owns the window, renderer, editor, `World`, `CommandBuffer`,
`SystemScheduler`, and lazily the manifest and cache. `Initialize`/`Run`/
`Shutdown`, with `dt` clamped by `AppDesc::mMaxDeltaSeconds`. Fixed members, no
subsystem registry. `Assets()` returns `nullptr` when there is no manifest, so
a missing one costs a caller an asset, not the process.

`Run`'s per-frame shape: poll input, `Editor::BeginFrame`/`DrawLayout`/
`EndFrame`, hand the resulting draw data and scene viewport to the renderer,
then `SystemScheduler::Update` (which reaches `RenderSystem` in the `Render`
phase). `App.cpp` has no ImGui calls of its own - all of that lives in
`editor`.

## Build system

- `engine_add_module(<name>)` - target `engine_<name>`, alias `mts::<name>`,
  `include/` public. `engine_add_module_tests(<name> <sources>)` - Catch2 binary,
  one CTest case per `TEST_CASE`.
- `EnginePlatform.cmake` - `ENGINE_PLATFORM` plus family flags, ordered so the
  overlapping cases (Emscripten sets `UNIX`, iOS sets `APPLE`) resolve correctly.
- `Shaders.cmake` / `Assets.cmake` - compile and cook into the build tree, then
  copy next to the executable so a moved build folder still runs.

## Known gaps

1. No input.
2. No renderer abstraction (`Renderer.h` is an empty placeholder).
3. No typed assets.
4. No scene or serialization layer, though `TypeId::hash` exists for it.
5. No scripting. Games are meant to be Lua and data; that needs a `script`
   module, a name to `TypeId` component registry, query access from Lua, config
   and script asset types, and a runtime executable - none of which exist.
6. Linux sources, but no Linux preset.
7. No CI.
8. `editor`'s panels (`Hierarchy`/`Inspector`/`Output`) are laid out but empty
   - nothing populates them from the `World` yet.
9. No descriptor sets/uniform buffers - the camera's view-projection travels
   as a premultiplied push constant, which caps what else can ride along with
   a draw call.
