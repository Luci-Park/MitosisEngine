# renderer

- **Owns:** the whole Vulkan path - instance through swapchain, meshes,
  materials (pipelines), depth, and the per-frame command recording.
- **Depends on:** `core` (`ISurfaceProvider`, ECS types for `RenderSystem`).
  Never sees `window` directly, only a surface provider - so it can't reach
  in and, say, poll GLFW events itself.
- **Depended on by:** `app`, `editor` (its Vulkan backend calls,
  `InitImGuiVulkanBackend`/`ShutdownImGuiVulkanBackend`).

## What it does

`VulkanRenderer` is one class holding the entire renderer: instance, debug
messenger, surface, device, VMA allocator, swapchain, per-frame-in-flight
command pools/buffers, a depth image per frame, an uploaded-mesh table, a
built-pipeline-per-material table, and timeline-semaphore frame pacing
(`kFramesInFlight = 2`).

`RenderSystem` (in `renderer/RenderSystem.h`) is the only thing that drives
it from the ECS: an `ISystem` in `SystemPhase::Render`, holding a
`VulkanRenderer&`. Each frame it takes the first `Camera` it finds, builds
one view-projection matrix, and builds one `DrawItem` per
`(WorldTransform, MeshRenderer)` pair, then calls `DrawFrame`. It lives in
`SystemPhase::Render` specifically because `TransformPropagateSystem` runs in
`PostUpdate`, one phase earlier - by the time `Render` runs, every
`WorldTransform` it reads is already current for that frame, not the
previous one.

`MeshRenderer`/`Camera` are declared here, in `renderer`, rather than in
`core`: they name renderer-owned resources (a `MeshHandle` the renderer
allocates, validates and eventually destroys), and `core` has no way to
validate or destroy what it can't understand. `engine_core` never depends on
`engine_renderer`, so this stays one-directional.

`main.cpp`/game code never calls Vulkan directly - it calls
`app.Renderer().CreateMesh(...)`/`CreateMaterial(...)` once at scene-build
time, stores the returned handles on `MeshRenderer` components, and the
`RenderSystem` takes it from there every frame after.

## Public API

```cpp
bool Initialize(const RendererDesc &desc);

MeshHandle CreateMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices);
MaterialHandle CreateMaterial(const MaterialDesc &desc);

void DrawFrame(std::span<const DrawItem> items);
void SetClearColor(const glm::vec4 &color);
void SetImGuiDrawData(ImDrawData *drawData);
void SetSceneViewport(VkRect2D rect);   // zero extent = full swapchain
float AspectRatio() const;              // reflects the scene viewport, not the window, once one is set

bool InitImGuiVulkanBackend();          // called by editor, not App or game code
void ShutdownImGuiVulkanBackend();

void Shutdown();
```

`Vertex{pos, color, normal}` and `MeshHandle`/`MaterialHandle` (both
`{index, generation}`, `IsNull()`) are in `Mesh.h`/`Material.h`.
`MaterialDesc{shaderName, cullMode, polygonMode}` names a `.slang` shader
compiled with `vertexMain`/`fragmentMain` entry points; `kNullMaterial` means
"use the renderer's default". One `VkPipeline` is built eagerly per material
at `CreateMaterial` (not lazily on first draw) - the cost is paid once at
load time rather than stalling whatever frame first uses it, matching how
mesh upload already works. `Shapes.h` has free functions (`MakeCube()`
today) building `MeshData{vertices, indices}` with no Vulkan involved -
primitive shapes are code, not cooked assets.

`DrawItem{mesh, model, normalMatrix, tint, material}` is what `RenderSystem`
builds and `DrawFrame` consumes; a game system could also build one by hand
if it needs a draw outside the normal `MeshRenderer` path.

`SetSceneViewport` clips the scene pass to a caller-given `VkRect2D`,
clamped to the swapchain, so an editor's docked panels never get painted
over by geometry underneath them; `SetImGuiDrawData`'s draw data composites
on top afterward, unclipped (ImGui manages its own per-window clip rects).
`editor` is the only caller of `SetSceneViewport` today.

## Current state

No render passes/framebuffers - `vkCmdBeginRendering` (dynamic rendering)
throughout. Requires Vulkan 1.3 with `dynamicRendering`, `synchronization2`,
`timelineSemaphore`, `shaderDrawParameters`; a device missing any of those is
rejected outright. `volk` is compiled from source in this module so the
platform defines are ours; VMA fetches its pointers through it. Swapchain
recreates on resize. RenderDoc object naming is applied under validation.

## Known gaps

- No descriptor sets or uniform buffers - the view-projection matrix is
  premultiplied on the CPU into each `DrawItem`'s push constant, which caps
  what else can travel with a draw call at the 128-byte guaranteed minimum.
- No destroy path for meshes or materials before `Shutdown()` - everything
  uploaded at load time lives until the process ends.
- No draw-item sorting/batching by mesh or material - one bind per item.
- No lighting beyond whatever `unlit`/`triangle` shaders hardcode.
