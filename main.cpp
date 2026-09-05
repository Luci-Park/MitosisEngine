#include <app/App.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/log/Log.h>
#include <renderer/Shapes.h>
#include <renderer/components/Camera.h>
#include <renderer/components/MeshRenderer.h>
#include <script/components/ScriptRef.h>

#include <glm/gtc/quaternion.hpp>

namespace
{
    void BuildScene(mts::App &app)
    {
        mts::World &world = app.GetWorld();

        const mts::MeshData cube = mts::MakeCube();
        const mts::MeshHandle cubeMesh = app.Renderer().CreateMesh(cube.vertices, cube.indices);

        const mts::Entity cubeEntity = world.CreateEntity();
        mts::AddTransform(world, cubeEntity, mts::Transform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)});
        world.AddComponent<mts::MeshRenderer>(cubeEntity, mts::MeshRenderer{cubeMesh, glm::vec4(1.0f)});

        app.Scripts().LoadScriptSource("spin", R"lua(
            local Spin = {}
            local angle = 0.0
            local speed = 1.0

            function Spin.OnUpdate(world, entity, dt)
                angle = angle + speed * dt
                local half = angle * 0.5
                world:set(entity, "Transform", "rotation", { w = math.cos(half), x = 0.0, y = math.sin(half), z = 0.0 })
            end

            return Spin
        )lua");

        const int32_t spinInstance = app.Scripts().CreateInstance("spin");
        world.AddComponent<mts::ScriptRef>(cubeEntity, mts::ScriptRef{.instanceRef = spinInstance});

        const mts::MaterialHandle unlitMaterial = app.Renderer().CreateMaterial(mts::MaterialDesc{.shaderName = "unlit"});

        const mts::Entity unlitCube = world.CreateEntity();
        mts::AddTransform(world, unlitCube, mts::Transform{glm::vec3(1.8f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.5f)});
        world.AddComponent<mts::MeshRenderer>(unlitCube, mts::MeshRenderer{cubeMesh, glm::vec4(1.0f), unlitMaterial});

        const mts::Entity camera = world.CreateEntity();
        mts::AddTransform(world, camera, mts::Transform{glm::vec3(0.0f, 0.0f, 5.0f)});
        world.AddComponent<mts::Camera>(camera, mts::Camera{});
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

    // After Initialize so the world exists, before Run because the scheduler
    // refuses new systems once started.
    BuildScene(app);

    app.Run();
    app.Shutdown();

    mts::FlushLog();
    return 0;
}
