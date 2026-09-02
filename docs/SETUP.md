# Setup

Windows only. Linux sources exist for window and surface backends, but there is
no Linux preset.

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| CMake | >= 3.26 | |
| Ninja | recent | Ships with the VS "C++ CMake tools" component |
| MSVC | VS 2026 or Build Tools | C++20 |
| Vulkan SDK | >= 1.4.309 | Also supplies `slangc`, required for shaders |
| vcpkg | any bootstrapped clone | Not vendored |
| VS Code | - | `ms-vscode.cmake-tools`, `ms-vscode.cpptools` |

## 1. vcpkg, once per machine

vcpkg is shared across projects, not vendored
([0001](decisions/0001-vcpkg-manifest-mode.md)).

```
git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat
setx VCPKG_ROOT C:/dev/vcpkg
```

Restart the terminal, and VS Code **entirely**, so the variable is visible.
Configure fails with instructions if `VCPKG_ROOT` is missing or unbootstrapped.

Dependencies come from `vcpkg.json` and install on configure. The first one takes
minutes; the cache in `%LOCALAPPDATA%\vcpkg` is shared with every vcpkg project,
so later ones are fast.

## 2. Vulkan SDK

Its installer sets `VULKAN_SDK`. The build finds `slangc` under
`$VULKAN_SDK/Bin`, else `PATH`, else fails - no glslc fallback
([0011](decisions/0011-slang-shaders.md)).

## 3. Optional: your name in generated files

Author resolution: `-Author`, then `MITOSIS_AUTHOR`, then `git config user.name`,
then the OS user.

```
setx MITOSIS_AUTHOR "Your Name"
setx MITOSIS_ORG "DigiPen (USA) Corporation"
```

## 4. Build and run

VS Code:

1. `Ctrl+Shift+P` -> **CMake: Select Configure Preset** -> `Debug` / `Release`
2. `F7` to build
3. `Ctrl+Shift+P` -> **CMake: Set Launch/Debug Target** -> `HelloWorld`
4. `Ctrl+F5` to run, `F5` to debug

After the first time, `F7` then `F5`.

Terminal, from a Developer PowerShell (or after `vcvars64.bat`):

```
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --test-dir builds/windows-msvc-debug --output-on-failure
```

Output goes to `builds/<presetName>/`, gitignored. Build into that tree, not a
second one.

Any change to a `CMakeLists.txt` or to `vcpkg.json` needs a reconfigure. Ninja
does it for you at build time, and cmake-tools can do it on save - see
`cmake.configureOnEdit`. To force one: `Ctrl+Shift+P` -> **CMake: Configure**.

One module's tests, without building everything:

```
cmake --build builds/windows-msvc-debug --target engine_core_tests
ctest --test-dir builds/windows-msvc-debug -R "archetype" --output-on-failure
```

`-R` matches Catch2 test case names, since each one is its own CTest case.

## 5. Verify

`HelloWorld` opens a window with a triangle; `ctest` is all green;
`logs/engine.log` has trace output.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `VCPKG_ROOT is not set` | `setx` only affects new processes. Restart the terminal and all of VS Code. |
| `slangc not found` | No Vulkan SDK, or `VULKAN_SDK` unset. |
| Configure fails right after **New Module** | A module with no sources is an empty library. Add a file. |
| IntelliSense misses a new dependency | Run **CMake: Delete Cache and Reconfigure**. |
| `cl.exe` not found in a plain terminal | MSVC environment not loaded. Use a Developer PowerShell or prefix with `vcvars64.bat`. |
