# Module docs

One file per module, in depth, written by whoever works on it. `ARCHITECTURE.md`
gives each module a paragraph and says how they connect; these go deeper on one
module at a time.

## Index

| Module | Owns |
|---|---|
| [app](app.md) | The composition root: owns window, renderer, editor, ECS world, runs the loop |
| [editor](editor.md) | ImGui context, backend, and the Slate editor shell |
| [renderer](renderer.md) | The whole Vulkan path: meshes, materials, camera, the scene pass |

Not yet written (see [ARCHITECTURE.md](../ARCHITECTURE.md) for what exists in
the meantime): `core`, `window`, `assets`, `editortheme`.

## Writing one

Copy [TEMPLATE.md](TEMPLATE.md) to `<module>.md` and fill it in. Keep it to
what a consumer of the module - not its author a year later - needs: the
public API's shape and why, not a narrated history of how it got there.
State reasoning inline, in plain English - do not link out to `docs/notes/`
or `docs/decisions/`, both of which are local-only (gitignored) and will be a
dead link for anyone else reading this. Update it in the same change that
changes the module's shape; a module doc that only gets written once and
never touched again is worse than no module doc, because it's wrong and
looks authoritative.
