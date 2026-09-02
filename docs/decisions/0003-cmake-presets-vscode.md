# 0003 - CMake presets and VS Code as the supported workflow

- **Status:** Accepted
- **Date:** 2026-09-01 (backfilled)
- **Deciders:** Sumin Park

## Context

Build configuration is where a team quietly diverges - different generators,
flags and output directories, then a bug that reproduces on one machine only.

## Decision

`CMakePresets.json` is the only supported way to configure:
`windows-msvc-debug` and `windows-msvc-release`, both Ninja, both building into
`builds/<presetName>/`, both exporting `compile_commands.json` and setting the
vcpkg cache locations. VS Code with `cmake-tools` and `cpptools` is the supported
editor, with `tools/` surfaced as tasks. The command line uses the same presets.

## Consequences

- Everyone builds identically, and future CI uses the same preset.
- One build tree per configuration; do not create a second by hand.
- Anything not expressible as a preset is unsupported - a new platform means a
  new preset pair.
- Command-line builds need the MSVC environment loaded; VS Code handles it
  through `cmake.useVsDeveloperEnvironment`.

CMake itself was chosen over the alternatives for reach: project settings are
easy to change in one place, it has far more support, documentation and
third-party integration than the alternatives, and it is the build system worth
knowing.

## Alternatives considered

- **premake** - pleasanter to write, but a much smaller ecosystem, and vcpkg,
  presets and the VS Code tooling all assume CMake.
- **Visual Studio solutions** - generated files that drift, and a second place to
  declare sources.
- **Wrapper scripts** - reimplements what presets standardise.
- **Multi-config generators** - workable, but single-config keeps a configuration
  component out of the output paths, which the shader and asset copy steps rely
  on.
