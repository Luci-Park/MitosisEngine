# Mitosis Engine

A modular 3D engine: an archetype ECS, a Vulkan 1.3 renderer, and an offline
asset pipeline. This is a CMake project built with VSCode. Everything is driven
through presets and VSCode tasks.

Each game is its own directory under `games/`, defined by configs, assets and Lua
scripts rather than by C++ - one shared runtime loads them, so making a game needs
no rebuild.

# Document Structure

| Document | What it answers |
|---|---|
| [docs/SETUP.md](docs/SETUP.md) | Getting it building on your machine |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | What the engine is today, and how it fits together |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | How to write code that fits in |
| [docs/EXTENDING.md](docs/EXTENDING.md) | Adding a module, component, system, shader or asset |
| [docs/README.md](docs/README.md) | The full documentation index, and where your own docs go |

The rest of this file is the short version.

# Requirement
- CMake >= 3.26
- Ninja
- MSVC (Visual Studio 2026 or Build Tools)
- Vulkan SDK >= 1.4.309 (supplies `slangc` for shader builds)
- VSCode extensions: `ms-vscode.cmake-tools`, `ms-vscode.cpptools`

# File Structure

```
CMakeLists.txt        root build: module list, HelloWorld target
CMakePresets.json     Debug / Release presets, the only supported configure path
vcpkg.json            dependency manifest
main.cpp              HelloWorld entry point

cmake/                build helpers
  EngineModule.cmake    engine_add_module, engine_add_module_tests
  EnginePlatform.cmake  ENGINE_PLATFORM and family flags
  Shaders.cmake         slangc compilation
  Assets.cmake          asset cooking
  VcpkgToolchain.cmake  resolves vcpkg from VCPKG_ROOT

third_party/          vendored (not vcpkg) - see Dependencies below
  imgui/                submodule, docking branch, pinned tag
  imgui_config/         repo-owned imconfig.h override (IMGUI_USER_CONFIG)
  IconFontCppHeaders/   icon codepoint header for Font Awesome

modules/              one static library each, mts::<name>
  core/                 ECS, logging, paths, surface contract
  window/               GLFW window behind an interface
  renderer/             Vulkan 1.3 renderer
  editortheme/          the Slate ImGui theme
  assets/               cooked asset blobs, manifest, cache
  app/                  composition root and main loop
    include/<name>/       public API - the whole surface of the module
    src/<name>/           implementation and private headers
    tests/                Catch2 tests, one CTest case per TEST_CASE

games/                one directory per game - planned, none exist yet
  <name>/
    game.config           boot parameters: title, resolution, entry script
    assets/               its own source assets, cooked separately
    scripts/              Lua: components, systems, scenes, gameplay

tools/                AssetCooker, new_module.ps1, new_file.ps1
templates/            what the scaffolding scripts stamp out
assets/               source assets, cooked into the build tree
fonts/                editor UI fonts, copied to fonts/ next to the exe
docs/                 setup, architecture, conventions
builds/               build trees, gitignored
logs/                 engine.log, gitignored
```

Module details in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

# Getting the Source

```
git clone <repo-url>
```

Dependencies come from vcpkg, which is not vendored here. You install it once
per machine and set `VCPKG_ROOT`. Prerequisites, setup and troubleshooting:
[docs/SETUP.md](docs/SETUP.md). Configure stops with an explanatory error if
`VCPKG_ROOT` is missing.

First configure builds all dependencies and takes a few minutes. Downloads and
prebuilt packages are cached in `%LOCALAPPDATA%\vcpkg` and shared with every other
vcpkg project on the machine, so later configures are fast.

One dependency is vendored instead: Dear ImGui, as a `third_party/imgui` git
submodule, MIT-licensed, `LICENSE.txt` ships inside the submodule. Clone with
`git clone --recurse-submodules`, or after the fact:
`git submodule update --init --recursive`.

The editor UI font, `fonts/Inter.ttf`, is Inter by Rasmus Andersson,
SIL Open Font License, `fonts/Inter-OFL.txt`.

Icon glyphs come from Font Awesome Free, `fonts/fa-solid-900.ttf`
(`fonts/FontAwesome-LICENSE.txt`), addressed through the codepoint macros in
`third_party/IconFontCppHeaders/IconsFontAwesome6.h` (zlib license).

# Adding Modules and Files
This engine is aiming for modularity, therefore each part of the engine is its own static library. To make things easier VSCode tasks have been added.

## Adding Modules
1. `Ctrl + Shift + P` -> `Tasks: Run Task`
2. Select `New Module`
3. Give it a name

## Adding Files
1. `Ctrl + Shift + P` -> `Tasks: Run Task`
2. Select `New File`
3. Type the target module
4. Give it a name
5. Select class(.h + .cpp) or header(.h)

## Building
1. `Ctrl + Shift + P` -> CMake: Select Configure Preset -> `Debug` or `Release`
2. `F7` to build
3. `Ctrl + Shift + P` -> CMake: Set Launch/Debug Target
4. `Ctrl+F5` (run) / `F5` (debug)
-> after setting configure and target once `F7` -> `F5` will be enough

A new module has no sources yet, and CMake cannot build an empty library. Add at
least one file to it before building.

# Assets

Source assets live under `assets/`. Building `HelloWorld` cooks them automatically:
`AssetCooker` builds first, runs over every configured source root, and its output
is copied to `cooked/` next to the executable, alongside `shaders/`. Editing an
asset triggers a recook on the next build; a file whose cooked output is already
newer than the source is skipped.

Any target can cook roots of its own - a game cooks its own `assets/` this way:

```cmake
engine_cook_assets(MyGame
    SOURCE_ROOTS games/mygame/assets/
    OUT_DIR ${CMAKE_BINARY_DIR}/mygame_cooked)
```

To run the cook step by hand:
```
AssetCooker --source <dir> [--source <dir> ...] --out <dir>
```

`SOURCE_ROOTS` and `--source` must both be relative to the repo root, never
absolute: the root string is hashed into every asset id, so an absolute path
bakes one machine's checkout location into ids that have to match everywhere.