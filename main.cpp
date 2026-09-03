#include <app/App.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/log/Log.h>
#include <renderer/Shapes.h>
#include <renderer/components/MeshRenderer.h>

#include <glm/gtc/quaternion.hpp>

namespace
{
    class SpinSystem final : public mts::ISystem
    {
    public:
        SpinSystem(mts::Entity target, float radiansPerSecond)
            : mTarget(target), mSpeed(radiansPerSecond)
        {
        }

        void OnUpdate(mts::SystemContext &context) override
        {
            mts::Transform *transform = context.world.Get<mts::Transform>(mTarget);
            if (transform == nullptr)
                return;

            transform->Rotate(glm::angleAxis(mSpeed * context.dt, glm::vec3(0.0f, 0.0f, 1.0f)));
        }

    private:
        mts::Entity mTarget;
        float mSpeed;
    };

    void BuildScene(mts::App &app)
    {
        mts::World &world = app.GetWorld();

        // One upload, shared by every entity below: CreateMesh names geometry
        // once and hands back a handle, the same handle any number of
        // MeshRenderers may carry.
        const mts::MeshData quad = mts::MakeQuad();
        const mts::MeshHandle quadMesh = app.Renderer().CreateMesh(quad.vertices, quad.indices);

        const mts::Entity parent = world.CreateEntity();
        mts::AddTransform(world, parent, mts::Transform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.6f)});
        world.AddComponent<mts::MeshRenderer>(parent, mts::MeshRenderer{quadMesh, glm::vec4(1.0f, 0.4f, 0.4f, 1.0f)});

        const mts::Entity child = world.CreateEntity();
        mts::AddTransform(world,
                          child,
                          mts::Transform{glm::vec3(1.2f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.5f)},
                          parent);
        world.AddComponent<mts::MeshRenderer>(child, mts::MeshRenderer{quadMesh, glm::vec4(0.4f, 0.6f, 1.0f, 1.0f)});

        app.Systems().Add<SpinSystem>(mts::SystemPhase::Update, parent, 1.0f);
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
