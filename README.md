
This is a CMake project built with VSCode. Everything is driven through presets and VSCode tasks.

# Requirement
- CMake >= 3.25
- Ninja
- MSVC (Visual Studio 2026 or Build Tools)
- VSCode extensions: `ms-vscode.cmake-tools`, `ms-vscode.cpptools`

# Getting the Source
vcpkg is vendored as a submodule and pinned to the `builtin-baseline` in `vcpkg.json`,
so there is nothing to install and no `VCPKG_ROOT` to set.

```
git clone --recurse-submodules <repo-url>
```

Already cloned without it:

```
git submodule update --init --recursive
```

The first configure bootstraps vcpkg and builds the dependencies; expect a few minutes.
Later configures are cached.

Downloads and prebuilt packages are kept in `%LOCALAPPDATA%\vcpkg` and shared with
every other vcpkg on the machine, so the submodule stays small. Its `buildtrees/`
directory is compile scratch and can be deleted at any time.

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