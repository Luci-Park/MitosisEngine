# editortheme

The Slate colour palette and spacing, plus DPI rescaling for fonts and icons. It
draws nothing and knows no engine type.

`mir::editortheme` · depends nothing engine-side; `imgui` (private) · API `modules/editortheme/include/editortheme/` · maintainer Sumin Park · reviewed 2026-09-06

## Model

One header, one source file, one call. `editor` applies the theme once at
startup, and nothing else calls in.

It is a separate module rather than a file inside `editor` because it depends on
nothing engine-side. It can be read, edited or swapped without touching anything
that owns state, and a second theme is a second file here rather than a branch in
`editor`.

## API

| Type | Header | Role |
|---|---|---|
| `EditorTheme` | `editortheme/EditorTheme.h` | Palette and spacing, applied to the live `ImGuiStyle` |

## Rules

**No engine dependency.** The moment this needs `mir::core`, it has stopped being
theme data and belongs in `editor`.

**Applied once, at startup, by `editor`.** There is no re-application path, so a
value changed after `Initialize` has no effect until the next run.

## State

*As of 2026-09-06.* One theme. Colours, spacing and DPI-aware font and icon
sizing all work.

No tests. It is a table of constants written into `ImGuiStyle`.

## Backlog

1. Runtime theme switching, which needs a re-apply entry point that `editor`
   calls.
2. A second theme, which would show whether the palette is genuinely
   parameterised or hardcoded in one place.
