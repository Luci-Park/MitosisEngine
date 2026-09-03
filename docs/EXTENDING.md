# Extending the engine

Rules referenced here are in [CONVENTIONS.md](CONVENTIONS.md).

## Add a module

A module is a static library with its own public headers. Add one when a feature
has an API others use and dependencies of its own.

1. **Tasks: Run Task** -> **New Module**, or
   `powershell -NoProfile -File tools/new_module.ps1 -Name input`
2. It creates `modules/<name>/` with `include/`, `src/` and an
   `engine_add_module(<name>)` CMakeLists, registers the subdirectory, and links
   `mts::<name>` into `HelloWorld`.
3. Declare dependencies:

```cmake
target_link_libraries(engine_input PUBLIC mts::core)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(engine_input PRIVATE glfw)
```

4. Add a file before configuring - CMake cannot build a library with no sources.

Check the [module graph](ARCHITECTURE.md#module-graph) still holds.

### By hand

The scripts are PowerShell. Without them:

1. `modules/<name>/include/<name>/` - public headers.
2. `modules/<name>/src/<name>/` - private headers and sources.
3. `modules/<name>/CMakeLists.txt`:

```cmake
engine_add_module(<name>)
target_sources(engine_<name> PRIVATE src/<name>/Thing.cpp)
```

4. In the root `CMakeLists.txt`: `add_subdirectory(modules/<name>)`, and
   `mts::<name>` in the executable's `target_link_libraries`.

Inside its own CMakeLists a module is `engine_<name>`; everywhere else it is the
alias `mts::<name>`.

## Add a file

**Tasks: Run Task** -> **New File**, or
`powershell -NoProfile -File tools/new_file.ps1 -Module core -Path ecs/Archetype`

`class` gives header plus source and lists the `.cpp` in `target_sources`;
`header` gives the header alone. Fill in `@brief`. Header-only is normal here,
especially in `core/ecs` - use `header` rather than an empty `.cpp`.

Nothing is globbed: a `.cpp` that is not listed in its module's
`target_sources(engine_<module> PRIVATE ...)` is never compiled, so its symbols
never reach the link. That call lists what the target compiles - libraries are a
separate matter, through `target_link_libraries`.

A header only gets IntelliSense once some compiled `.cpp` includes it - its own,
a test, or another module's source. `C_Cpp.intelliSenseEngineFallback` is
disabled, so a header no translation unit reaches gets nothing at all. The
header-only files in `core/ecs` work because the tests include them.

## Add a component

```cpp
#include <core/ecs/ComponentAsserts.h>
#include <core/ecs/StorageInfo.h>

namespace mts
{
    struct Position
    {
        float x = 0.0f;
        float y = 0.0f;
    };
    MTS_ASSERT_COMPONENT(Position);
}

// Only for components added and removed often. Outside any namespace.
MTS_COMPONENT_SPARSE(mts::Position);
```

The name must be globally unique across namespaces - `TypeIdOf` hashes the bare
name and a collision asserts in Debug.

## Add a system

```cpp
class MovementSystem final : public ISystem
{
public:
    void OnStart(SystemContext &context) override
    {
        mQuery = &context.world.GetOrCreateQuery<Position, const Velocity>();
    }

    void OnUpdate(SystemContext &context) override
    {
        const float dt = context.dt;
        mQuery->ForEach([&](Entity entity, Position &position, const Velocity &velocity)
        {
            position.x += velocity.x * dt;

            // Structural change is recorded, never applied here.
            if (position.x > 100.0f)
                context.commands.Destroy(entity);
        });
    }

private:
    Query<Position, const Velocity> *mQuery = nullptr;
};
```

Register before `Run`, in the phase that expresses the ordering:

```cpp
app.Systems().Add<MovementSystem>(SystemPhase::Update);
```

- `Add`, `Remove`, `Destroy` go on `context.commands` and land at the next phase
  boundary. Mutating inside `ForEach` invalidates the iteration.
- An entity from `World::CreateEntity` has a handle at once, components only
  after the flush.
- Systems cannot be registered after `Start`.
- Read-only components are `const T` in the query.

Filters go in the same call:

```cpp
context.world.GetOrCreateQuery<Position>(Without<Frozen>{}, With<Active>{});
```

## Add tests

Create `modules/<name>/tests/<Subject>.tests.cpp` and list it:

```cmake
engine_add_module_tests(core
    Paths.tests.cpp
    Archetype.tests.cpp
)
```

If the module has no tests yet, gate the subdirectory in its CMakeLists with
`if(BUILD_TESTING)`. Run
`ctest --test-dir builds/windows-msvc-debug --output-on-failure`.

## Add a shader

Write `.slang` under `assets/shaders/`, then:

```cmake
engine_add_shaders(HelloWorld
    SOURCE assets/shaders/triangle.slang
    ENTRIES vertexMain:vertex fragmentMain:fragment)
```

One `.spv` per entry point, `<source>.<entry>.spv`, copied next to the
executable. Load with `mts::ShaderPath("...")`, which resolves against the
executable, not the working directory. Flags are fixed - SPIR-V 1.5, column-major
matrices, entry-point names preserved, `-g -O0` for RenderDoc.

## Add assets

Drop the file under a cooked source root (`assets/`). Building recooks; a new
*file* needs a reconfigure to join the glob.

```cpp
if (AssetCache *cache = app.Assets())
{
    if (const AssetBlobView *blob = cache->Load(MakeAssetId("assets/foo.txt")))
        Use(blob->content);
}
```

`Assets()` returns `nullptr` when there is no manifest - handle it, do not assert.

A new cook root:

```cmake
engine_cook_assets(MyGame
    SOURCE_ROOTS games/mygame/assets/
    OUT_DIR ${CMAKE_BINARY_DIR}/mygame_cooked)
```

`SOURCE_ROOTS` must be **relative to the repository root** - absolute is rejected,
because the root string is hashed into every asset id.

For a typed asset: add a type tag and content version in `AssetBlob.h`, teach
`tools/AssetCooker/main.cpp` to recognise the extension and emit that tag, and
bump the content version whenever the layout changes - that bump is what forces a
recook instead of a silent mismatch.

## Add a dependency

Add it to `vcpkg.json` (configure re-runs automatically), `find_package(...
CONFIG REQUIRED)` in the module that needs it, link `PRIVATE` unless a type
appears in a public header.

## Add a platform

1. Extend `cmake/EnginePlatform.cmake` if it is not detected.
2. Add a configure and build preset pair.
3. Implement `GLFW<Platform>.cpp` (native handle) and
   `VulkanSurface<Platform>.cpp` (surface), plus the `VK_USE_PLATFORM_*` define.
4. Add the `WindowBackend` enumerator in `core/platform/Surface.h`.
5. Update [SETUP.md](SETUP.md).
