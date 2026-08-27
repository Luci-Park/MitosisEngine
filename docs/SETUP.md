# Setup

vcpkg is not vendored in this repo. Install it once per machine, set
`VCPKG_ROOT`, and every project shares the same download and binary cache.

## 1. Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Visual Studio 2022/2026 | any | "Desktop development with C++" workload |
| CMake | >= 3.25 | ships with that workload |
| Ninja | any | ships with that workload |
| Vulkan SDK | 1.4.x | https://vulkan.lunarg.com, default options |
| VS Code | any | `ms-vscode.cmake-tools`, `ms-vscode.cpptools` |

## 2. Install vcpkg

```
git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
```

Do not use `--depth 1`. `vcpkg.json` pins an exact vcpkg commit
(`builtin-baseline`) so everyone gets identical dependency versions, and a
shallow clone will not contain it.

## 3. Set VCPKG_ROOT

```
setx VCPKG_ROOT C:\dev\vcpkg
```

`setx` only affects new processes. Close all terminals and quit VS Code fully;
"Reload Window" is not enough. Confirm with `echo %VCPKG_ROOT%` in a new
terminal.

## 4. Build

```
git clone <repo-url>
cd MitosisEngine
cmake --preset windows-msvc-debug
```

Or in VS Code: `Ctrl+Shift+P` -> CMake: Select Configure Preset -> `Debug`, then
`F7`.

First configure builds all dependencies and takes a few minutes. Later ones hit
the cache in `%LOCALAPPDATA%\vcpkg`.

## Adding a dependency

1. Add the port to `dependencies` in `vcpkg.json`.
2. Re-configure. vcpkg only runs at configure time, so nothing installs and
   IntelliSense keeps stale include paths until it does.
3. `find_package` / `target_link_libraries` in the owning module.

## When the baseline is bumped

Your vcpkg clone needs the new commit. No checkout or re-bootstrap required.

```
git -C %VCPKG_ROOT% fetch origin master
```

## Troubleshooting

**`VCPKG_ROOT is not set.`**
Terminal or VS Code was not restarted after `setx`. Check `echo %VCPKG_ROOT%` in
a new terminal.

**`no vcpkg toolchain was found there`**
Wrong path, or `bootstrap-vcpkg.bat` was never run.

**Unknown baseline commit**
`git -C %VCPKG_ROOT% fetch origin master`. If cloned shallow, use
`fetch --unshallow origin master`.

**New header red-squiggles after a successful build**
Re-configure, then `Ctrl+Shift+P` -> C/C++: Reset IntelliSense Database.

**Everything is slow**
`builds/vcpkg_installed/` holds a copy of every installed header. Keep the
exclusions in `.vscode/settings.json`.

**Need a different vcpkg for this project only**
Not supported by design. Override explicitly if you must:

```
cmake --preset windows-msvc-debug -DCMAKE_TOOLCHAIN_FILE=<path>/scripts/buildsystems/vcpkg.cmake
```
