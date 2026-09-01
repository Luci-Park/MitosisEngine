/**
 * @file System.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/SystemScheduler ordering and lifecycle.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/CommandBuffer.h>
#include <core/ecs/System.h>
#include <core/ecs/SystemScheduler.h>
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace mts;

namespace
{
    struct SPosition
    {
        float x;
        float y;
    };

    struct SSpawned
    {
        int tag;
    };

    struct Recorder
    {
        std::vector<int> started;
        std::vector<int> updated;
        std::vector<int> stopped;
    };

    /// Writes its id into the recorder at every lifecycle point, so ordering is
    /// observable without any world state.
    class TagSystem final : public ISystem
    {
    public:
        TagSystem(Recorder &recorder, int id) : mRecorder(recorder), mId(id) {}

        void OnStart(SystemContext &) override { mRecorder.started.push_back(mId); }
        void OnUpdate(SystemContext &) override { mRecorder.updated.push_back(mId); }
        void OnStop(SystemContext &) override { mRecorder.stopped.push_back(mId); }

    private:
        Recorder &mRecorder;
        int mId;
    };

    /// Spawns one entity per tick through the command buffer.
    class SpawnSystem final : public ISystem
    {
    public:
        void OnUpdate(SystemContext &context) override
        {
            const Entity entity = context.world.CreateEntity();
            context.commands.Add(entity, SSpawned{mNextTag++});
        }

    private:
        int mNextTag = 0;
    };

    /// Counts what is visible to it at the moment it runs.
    class CountSystem final : public ISystem
    {
    public:
        void OnUpdate(SystemContext &context) override
        {
            std::size_t count = 0;
            context.world.ForEach<SSpawned>([&](Entity, SSpawned &) { ++count; });
            seen.push_back(count);
        }

        std::vector<std::size_t> seen;
    };
}

TEST_CASE("Systems run in phase order, then registration order", "[ecs][system]")
{
    World world;
    CommandBuffer commands;
    SystemScheduler scheduler;
    Recorder recorder;

    // registered out of phase order on purpose
    scheduler.Add<TagSystem>(SystemPhase::Update, recorder, 1);
    scheduler.Add<TagSystem>(SystemPhase::PostUpdate, recorder, 4);
    scheduler.Add<TagSystem>(SystemPhase::Update, recorder, 2);
    scheduler.Add<TagSystem>(SystemPhase::PreUpdate, recorder, 3);

    REQUIRE(scheduler.SystemCount() == 4);
    REQUIRE(scheduler.SystemCount(SystemPhase::Update) == 2);
    REQUIRE(scheduler.SystemCount(SystemPhase::Render) == 0);

    SystemContext context{world, commands};
    scheduler.Start(context);
    scheduler.Update(context);

    REQUIRE(recorder.started == std::vector<int>{3, 1, 2, 4});
    REQUIRE(recorder.updated == std::vector<int>{3, 1, 2, 4});
}

TEST_CASE("OnStart runs once and OnStop runs in reverse", "[ecs][system]")
{
    World world;
    CommandBuffer commands;
    SystemScheduler scheduler;
    Recorder recorder;

    scheduler.Add<TagSystem>(SystemPhase::PreUpdate, recorder, 1);
    scheduler.Add<TagSystem>(SystemPhase::Update, recorder, 2);
    scheduler.Add<TagSystem>(SystemPhase::Update, recorder, 3);

    SystemContext context{world, commands};
    REQUIRE_FALSE(scheduler.Started());

    scheduler.Start(context);
    REQUIRE(scheduler.Started());

    scheduler.Update(context);
    scheduler.Update(context);

    REQUIRE(recorder.started == std::vector<int>{1, 2, 3});
    REQUIRE(recorder.updated == std::vector<int>{1, 2, 3, 1, 2, 3});

    scheduler.Stop(context);
    REQUIRE(recorder.stopped == std::vector<int>{3, 2, 1});
    REQUIRE_FALSE(scheduler.Started());

    // Stop is idempotent, so App::Shutdown running twice is harmless
    scheduler.Stop(context);
    REQUIRE(recorder.stopped == std::vector<int>{3, 2, 1});
}

TEST_CASE("Commands recorded in one phase are visible to the next", "[ecs][system]")
{
    World world;
    CommandBuffer commands;
    SystemScheduler scheduler;

    scheduler.Add<SpawnSystem>(SystemPhase::PreUpdate);
    CountSystem &counter = scheduler.Add<CountSystem>(SystemPhase::Update);

    SystemContext context{world, commands};
    scheduler.Start(context);

    scheduler.Update(context);
    scheduler.Update(context);
    scheduler.Update(context);

    // the entity spawned in PreUpdate is already there when Update runs
    REQUIRE(counter.seen == std::vector<std::size_t>{1, 2, 3});
}

TEST_CASE("A spawn is not visible to systems in its own phase", "[ecs][system]")
{
    World world;
    CommandBuffer commands;
    SystemScheduler scheduler;

    scheduler.Add<SpawnSystem>(SystemPhase::Update);
    CountSystem &counter = scheduler.Add<CountSystem>(SystemPhase::Update);

    SystemContext context{world, commands};
    scheduler.Start(context);

    scheduler.Update(context);
    scheduler.Update(context);

    // one phase of latency: the counter always trails the spawner by a frame
    REQUIRE(counter.seen == std::vector<std::size_t>{0, 1});
}

TEST_CASE("The context carries frame timing through to systems", "[ecs][system]")
{
    World world;
    CommandBuffer commands;
    SystemScheduler scheduler;

    class TimeSystem final : public ISystem
    {
    public:
        void OnUpdate(SystemContext &context) override
        {
            deltas.push_back(context.dt);
            frames.push_back(context.frame);
        }

        std::vector<float> deltas;
        std::vector<uint64_t> frames;
    };

    TimeSystem &timing = scheduler.Add<TimeSystem>(SystemPhase::Update);

    SystemContext context{world, commands};
    scheduler.Start(context);

    context.dt = 0.5f;
    context.frame = 7;
    scheduler.Update(context);

    REQUIRE(timing.deltas == std::vector<float>{0.5f});
    REQUIRE(timing.frames == std::vector<uint64_t>{7});
}
