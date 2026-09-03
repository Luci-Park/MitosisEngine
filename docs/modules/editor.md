# editor

- **Owns:** the ImGui context and backend lifecycle, and the Slate editor
  shell (dockspace, default panel layout, Debug menu).
- **Depends on:** `renderer` (`VulkanRenderer&` for the Vulkan backend),
  `window` (`Window&` for the GLFW backend and DPI), `core` and
  `editortheme` privately.
- **Depended on by:** `app`, `PUBLIC` - `App.h` embeds an `Editor` by value.

## What it does

`Editor` is the only place in the engine that calls `ImGui::*` or
`ImGui_Impl*`. `App` never does; it drives `Editor` through four calls per
frame and reads one piece of state back. Keeping ImGui behind this boundary
means a build that wants `App`'s window/ECS/render-loop shape without an
editor UI only has to stop constructing an `Editor`, not untangle ImGui calls
out of `App.cpp`.

A typical frame from `App::Run`:

```cpp
mEditor.BeginFrame();                                             // ImGui NewFrame trio

if (windowVisible)
    mEditor.DrawLayout(mDesc.mEnableEditorLayout, mDesc.mShowImGuiDemo);

mRenderer.SetImGuiDrawData(mEditor.EndFrame());                   // ImGui::Render, hand off draw data
mRenderer.SetSceneViewport(mEditor.SceneViewportRect());
```

`DrawLayout(true, ...)` builds the dockspace once (`DockBuilderSplitNode` into
left/right/bottom, `ImGuiDockNodeFlags_PassthruCentralNode` keeping the
center undocked), docks `Hierarchy`/`Inspector`/`Output` into it, draws the
`Debug` menu bar, and - if toggled from that menu - `ImGui::ShowStyleEditor()`.
Passing `false` skips all of that (an ImGui context still runs, e.g. for a
caller's own UI) and resets `SceneViewportRect()` to zero extent.

## Public API

```cpp
bool Initialize(Window &window, VulkanRenderer &renderer);
void Shutdown(VulkanRenderer &renderer);

void BeginFrame();
void DrawLayout(bool enableLayout, bool showDemoWindow);
ImDrawData *EndFrame();

VkRect2D SceneViewportRect() const;
bool IsInitialized() const;
```

`SceneViewportRect()` is read from `ImGui::DockBuilderGetCentralNode` - the
dockspace's undocked center, where the 3D scene shows through -
**never from a named window**. The dockspace docks a window titled
`Hierarchy` into its left split; nothing is docked into the center on
purpose (`PassthruCentralNode`), so the center is where the swapchain shows
through and is therefore the real viewport, even though it has no window and
no name. A window that happens to be named "Scene" would be a decoy, not the
viewport - that mistake shipped once (the scissor rect was sourced from such
a window, and the whole scene silently vanished behind an unrelated sidebar)
and is why this rect is only ever read from the central node.

## Current state

One fixed layout (`Hierarchy` left, `Inspector` right, `Output` bottom,
scene passthru center), built once and left alone - not user-editable beyond
what ImGui's own docking UI allows, and persisted only through ImGui's
`imgui.ini` (path: `<executable dir>/imgui.ini`). Fonts are Inter (body) and
Font Awesome 6 (icons), merged into one atlas, rescaled for the window's
`ContentScale()`. `Editor::~Editor()` is a documented no-op if a caller skips
`Shutdown()` - it leaks the ImGui context rather than reach into a
`VulkanRenderer&` it was never given.

## Known gaps

- `Hierarchy`, `Inspector`, and `Output` are docked but empty - nothing reads
  the `World` to populate them yet.
- No input routing to/from the editor beyond what `ImGui_ImplGlfw` gives for
  free (mouse/keyboard capture when hovering an ImGui window).
- No serialized custom layouts - only whatever `imgui.ini` remembers.
