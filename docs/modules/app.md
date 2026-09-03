# app

- **Owns:** the composition root - window, renderer, editor, ECS world,
  scheduler, and the main loop.
- **Depends on:** `assets`, `core`, `window`, `renderer`, `editor` - all
  `PUBLIC`. `App` is the only module that knows all the others; nothing else
  in the engine is allowed to reach across this many module boundaries.
- **Depended on by:** `HelloWorld` (`main.cpp`); any future game executable.

## What it does

`App` is a fixed-member composition root, not a subsystem registry - adding a
new always-on subsystem (input, audio) means adding a member to `App` by
hand, on purpose, so every subsystem `App` owns is visible in one place
rather than scattered across a runtime-registered list. A caller does:

```cpp
mts::App app;
app.Initialize(mts::AppDesc{ .mTitle = "My Game" });

// scene setup: app.GetWorld(), app.Renderer().CreateMesh/CreateMaterial,
// app.Systems().Add<MySystem>(SystemPhase::Update), all before Run()

app.Run();
app.Shutdown();
```

`Run`'s per-iteration shape: poll window events, `Editor::BeginFrame` ->
`DrawLayout` -> `EndFrame`, hand the resulting `ImDrawData*` and scene
viewport rect to the renderer, then `SystemScheduler::Update` (which reaches
`RenderSystem` in the `Render` phase). `App.cpp` has no ImGui calls of its
own; all editor UI lives in `editor` - `App` only calls its four lifecycle
methods.

## Public API

```cpp
bool Initialize(const AppDesc &desc);
void Run();
void Shutdown();

World &GetWorld();
SystemScheduler &Systems();
VulkanRenderer &Renderer();     // temporary seam - see the comment on it in App.h
AssetCache *Assets();           // nullptr if no manifest; sticky failure, not retried every call
```

`AppDesc`: `mWidth`/`mHeight`/`mTitle`/`mAppName`, `mEnableValidation`,
`mShowImGuiDemo` (defaults true in Debug, false in Release),
`mEnableEditorLayout` (gates `Editor::DrawLayout`'s dockspace - an ImGui
context still runs either way), `mMaxDeltaSeconds` (clamps `dt`).

## Current state

Initialize order: `Window::Create` -> `VulkanRenderer::Initialize` ->
`Editor::Initialize` -> install the scene-graph destroy hook -> register core
and renderer components -> publish `FrameCommands` as a world resource ->
register `TransformPropagateSystem` (`PostUpdate`, so it runs before
`Render` sees this frame's transforms) and `RenderSystem` (`Render`). Any
failure before the last successful step rolls back what it already created
and returns `false` - `Initialize` either fully succeeds or leaves nothing
behind. `Shutdown` is safe to call from the destructor or manually, is
idempotent (`mInitialized` gate), and its ordering is the mirror image of
`Initialize` for exactly the pieces that need it: `Editor::Shutdown` before
`VulkanRenderer::Shutdown` (the editor's Vulkan backend needs the device
still alive), and the renderer's shutdown before the window is destroyed
(the renderer holds a surface built from it). `mEditor` is declared right
after `mRenderer` in `App`'s member list, before `mWorld`/`mCommands`, so
this same ordering holds even if `Shutdown()` were somehow skipped and
`App`'s own destructor had to unwind it.

## Known gaps

- `Renderer()` is called out in its own doc comment as a temporary seam -
  scene code reaches the renderer through `App` because there's no
  asset-facing mesh/material service yet.
