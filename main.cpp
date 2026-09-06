#include <app/App.h>
#include <assets/AssetBlob.h>
#include <assets/AssetCache.h>
#include <assets/AssetId.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/log/Log.h>
#include <renderer/Shapes.h>
#include <renderer/components/Camera.h>
#include <renderer/components/MeshRenderer.h>
#include <scene/SceneAsset.h>
#include <script/components/ScriptRef.h>

#include <glm/gtc/quaternion.hpp>

#include <string>
#include <string_view>

namespace
{
    /// Loads a `.lua` asset by its cooked path and registers it with the
    /// script host under `scriptName`, then hands it to the reload watcher
    /// so an edit followed by a rebuild is picked up without restarting.
    /// False when assets aren't available or the file failed to load or
    /// parse - logged by AssetCache/ScriptHost already, so the caller just
    /// decides whether to attach the script.
    bool LoadScriptAsset(mts::App &app, std::string_view assetPath, std::string_view scriptName)
    {
        mts::AssetCache *cache = app.Assets();
        if (cache == nullptr)
            return false;

        const mts::AssetId id = mts::MakeAssetId(assetPath);
        const mts::AssetBlobView *blob = cache->Load(id);
        if (blob == nullptr)
            return false;

        if (!app.Scripts().LoadScriptSource(scriptName, mts::AsStringView(*blob)))
            return false;

        app.ScriptReload().Track(std::string(scriptName), id);
        return true;
    }

    void BuildScene(mts::App &app)
    {
        mts::World &world = app.GetWorld();
        mts::LoadedScene &scene = app.Scene();

        const mts::MeshData cube = mts::MakeCube();
        const mts::MeshHandle cubeMesh = app.Renderer().CreateMesh(cube.vertices, cube.indices);

        const mts::Entity cubeEntity = mts::CreateSceneEntity(world, scene);
        mts::AddTransform(world, cubeEntity, mts::Transform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)});
        world.AddComponent<mts::MeshRenderer>(cubeEntity, mts::MeshRenderer{cubeMesh, glm::vec4(1.0f)});

        if (LoadScriptAsset(app, "assets/scripts/spin.lua", "spin"))
        {
            const int32_t spinInstance = app.Scripts().CreateInstance("spin");
            world.AddComponent<mts::ScriptRef>(cubeEntity, mts::ScriptRef{.instanceRef = spinInstance});
        }
        else
        {
            MTS_LOG_ERROR("BuildScene: could not load assets/scripts/spin.lua - cube will not spin");
        }

        const mts::MaterialHandle unlitMaterial = app.Renderer().CreateMaterial(mts::MaterialDesc{.shaderName = "unlit"});

        const mts::Entity unlitCube = mts::CreateSceneEntity(world, scene);
        mts::AddTransform(world, unlitCube, mts::Transform{glm::vec3(1.8f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.5f)});
        world.AddComponent<mts::MeshRenderer>(unlitCube, mts::MeshRenderer{cubeMesh, glm::vec4(1.0f), unlitMaterial});

        const mts::Entity camera = mts::CreateSceneEntity(world, scene);
        mts::AddTransform(world, camera, mts::Transform{glm::vec3(0.0f, 0.0f, 5.0f)});
        world.AddComponent<mts::Camera>(camera, mts::Camera{});

        app.Scripts().LoadScriptSource("stage4_demo", R"lua(
            local Stage4Demo = {}

            function Stage4Demo.OnStart(self, world, entity)
                local a = world:spawn()
                self.entityA = a
                print("has(a, Transform) before add (expect false):", world:has(a, "Transform"))
                world:add(a, "Transform", { position = { x = 1, y = 0, z = 0 } })
                print("has(a, Transform) right after add, same call (expect false - deferred):", world:has(a, "Transform"))

                local b = world:spawn()
                self.entityB = b
                world:add(b, "Transform", { position = { x = 2, y = 0, z = 0 } })
                world:declare("MissionFlag", { { name = "triggered", kind = "bool" } })
                world:add(b, "MissionFlag", { triggered = true })
                print("get(b, MissionFlag) right after add, same call (expect nil):", world:get(b, "MissionFlag"))
            end

            function Stage4Demo.OnUpdate(self, world, entity, dt)
                self.stage = (self.stage or 0) + 1
                local a = self.entityA
                local b = self.entityB

                -- stage 1 is the *same* PreUpdate pass as OnStart (a system
                -- calls OnStart then OnUpdate back to back, before its own
                -- phase has finished, let alone flushed) - so OnStart's adds
                -- are still pending here too. stage 2 is the next frame's
                -- OnUpdate, the first call after that PreUpdate boundary
                -- actually flushed.
                if self.stage == 2 then
                    print("has(a, Transform) after OnStart's flush (expect true):", world:has(a, "Transform"))
                    print("MissionFlag.triggered on b (expect true):", world:get(b, "MissionFlag").triggered)

                    local sawA = false
                    world:each("Transform", function(e)
                        if e == a then sawA = true end
                    end)
                    print("world:each saw a (expect true):", sawA)

                    world:remove(b, "Transform")
                    print("has(b, Transform) right after remove, same call (expect true - deferred):", world:has(b, "Transform"))

                    world:destroy(a)
                    print("has(a, Transform) right after destroy, same call (expect true - deferred):", world:has(a, "Transform"))
                elseif self.stage == 3 then
                    print("has(b, Transform) after remove's flush (expect false):", world:has(b, "Transform"))
                    print("has(a, Transform) after destroy's flush (expect false):", world:has(a, "Transform"))
                end
            end

            return Stage4Demo
        )lua");

        const mts::Entity demoEntity = world.CreateEntity();
        const int32_t demoInstance = app.Scripts().CreateInstance("stage4_demo");
        world.AddComponent<mts::ScriptRef>(demoEntity, mts::ScriptRef{.instanceRef = demoInstance});
    }
}

int main()
{
    // Logging lives outside App so early construction failures are still visible.
    mts::InitLog();

    mts::App app;

    mts::AppDesc desc{};
    desc.mTitle = "MitosisEngine - Transform Hierarchy";

    if (!app.Initialize(desc))
    {
        mts::FlushLog();
        return -1;
    }

    BuildScene(app);

    app.Run();
    app.Shutdown();

    mts::FlushLog();
    return 0;
}
