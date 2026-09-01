
This is a CMake project built with VSCode. Everything is driven through presets and VSCode tasks.

# Requirement
- CMake >= 3.26
- Ninja
- MSVC (Visual Studio 2026 or Build Tools)
- Vulkan SDK >= 1.4.309 (supplies `slangc` for shader builds)
- VSCode extensions: `ms-vscode.cmake-tools`, `ms-vscode.cpptools`

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