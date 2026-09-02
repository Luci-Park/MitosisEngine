# 0011 - Shaders are Slang, compiled by slangc, with no fallback

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Vulkan consumes SPIR-V and several languages compile to it. Slang is an HLSL
superset with modules, generics and multiple entry points per file, and it ships
in the Vulkan SDK, so it needs no extra install.

## Decision

Shaders are Slang, compiled by `slangc`, declared with `engine_add_shaders`. One
`.spv` per entry point, copied next to the executable. `cmake/Shaders.cmake`
looks in `$VULKAN_SDK/Bin` first so the compiler matches the validation layers,
then `PATH`, then fails configure. No glslc fallback: a silent fallback compiles
a different language than the source is written in.

Fixed flags: `-target spirv -profile spirv_1_5`; `-fvk-use-entrypoint-name`,
without which Slang renames every entry point to `main`;
`-matrix-layout-column-major` to match GLM; `-g -O0` so RenderDoc shows Slang
source. The debug flags are unconditional because the output path carries no
configuration, so a per-config flag would leave one configuration reading the
other's `.spv`.

## Consequences

- Vertex and fragment stages share one file with real entry point names.
- No toolchain beyond the Vulkan SDK, which is why SETUP has an SDK version floor.
- Release ships unoptimised shaders; fixing that needs a per-config output path
  first.
- A machine without the SDK cannot configure - intentionally loud.

## Alternatives considered

- **GLSL with glslc** - the default everywhere, but no modules or generics, one
  file per stage.
- **HLSL with DXC** - a second toolchain to install and version.
- **Precompiled SPIR-V in the repository** - shaders stop being reviewable source.
