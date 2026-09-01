/**
 * @file CommandBuffer.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/CommandBuffer deferred structural changes.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/CommandBuffer.h>
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace
{
    struct CPosition
    {
        float x;
        float y;
    };

    struct CHealth
    {
        int hp;
    };

    // 2-byte alignment: leaves mStorage at an odd size, so the next payload
    // has to be rounded rather than appended where it lands
    struct CSmall
    {
        int16_t v;
    };

    // the case that made AlignUp necessary, and the case that used to trip
    // ComponentColumn's max_align_t assert on MSVC only
    struct alignas(16) CWide
    {
        float m[4];
    };
}

using namespace mts;

TEST_CASE("Add is deferred until Flush", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    world.AddComponent(entity, CPosition{1.0f, 2.0f});

    commands.Add(entity, CHealth{5});

    REQUIRE_FALSE(commands.Empty());
    REQUIRE(world.Get<CHealth>(entity) == nullptr);

    commands.Flush(world);

    REQUIRE(commands.Empty());
    REQUIRE(world.Get<CHealth>(entity) != nullptr);
    REQUIRE(world.Get<CHealth>(entity)->hp == 5);
}

TEST_CASE("Add recorded inside a ForEach applies after the walk", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    std::vector<Entity> entities;
    for (int i = 0; i < 4; ++i)
    {
        const Entity entity = world.CreateEntity();
        world.AddComponent(entity, CPosition{static_cast<float>(i), 0.0f});
        entities.push_back(entity);
    }

    // an immediate AddComponent here would reallocate the CPosition column and
    // dangle the position reference this very callback is holding
    world.ForEach<CPosition>([&](Entity entity, CPosition &position)
                             { commands.Add(entity, CHealth{static_cast<int>(position.x)}); });

    REQUIRE(commands.Size() == 4);
    commands.Flush(world);

    for (int i = 0; i < 4; ++i)
    {
        const CHealth *health = world.Get<CHealth>(entities[static_cast<std::size_t>(i)]);
        REQUIRE(health != nullptr);
        REQUIRE(health->hp == i);
    }
}

TEST_CASE("An entity spawned during iteration is visible after the flush", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    const Entity seed = world.CreateEntity();
    world.AddComponent(seed, CPosition{10.0f, 0.0f});

    Entity spawned = kNullEntity;
    world.ForEach<CPosition>([&](Entity, CPosition &position)
                             {
                                 // CreateEntity stays immediate: the new entity lands in the
                                 // empty archetype, which has no columns to reallocate and
                                 // which no query matches
                                 spawned = world.CreateEntity();
                                 commands.Add(spawned, CPosition{position.x + 1.0f, 0.0f}); });

    REQUIRE_FALSE(spawned.IsNull());
    REQUIRE(world.IsAlive(spawned));          // handle is live immediately
    REQUIRE(world.Get<CPosition>(spawned) == nullptr); // data is not

    commands.Flush(world);

    REQUIRE(world.Get<CPosition>(spawned) != nullptr);
    REQUIRE(world.Get<CPosition>(spawned)->x == 11.0f);

    std::vector<float> seen;
    world.ForEach<CPosition>([&](Entity, CPosition &position) { seen.push_back(position.x); });
    std::sort(seen.begin(), seen.end());
    REQUIRE(seen == std::vector<float>{10.0f, 11.0f});
}

TEST_CASE("Destroy then Add on the same entity is a no-op, not an assert", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    world.AddComponent(entity, CPosition{0.0f, 0.0f});

    commands.Destroy(entity);
    commands.Add(entity, CHealth{9});
    commands.Flush(world);

    REQUIRE_FALSE(world.IsAlive(entity));
    REQUIRE(world.Get<CHealth>(entity) == nullptr);
}

TEST_CASE("Remove is deferred until Flush", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    world.AddComponent(entity, CPosition{0.0f, 0.0f});
    world.AddComponent(entity, CHealth{3});

    commands.Remove<CHealth>(entity);
    REQUIRE(world.Has<CHealth>(entity));

    commands.Flush(world);
    REQUIRE_FALSE(world.Has<CHealth>(entity));

    // removing what is already gone is tolerated, so two systems can both ask
    commands.Remove<CHealth>(entity);
    commands.Flush(world);
    REQUIRE_FALSE(world.Has<CHealth>(entity));
}

TEST_CASE("Adding the same component twice keeps the last value", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    const Entity entity = world.CreateEntity();
    commands.Add(entity, CHealth{1});
    commands.Add(entity, CHealth{7});
    commands.Flush(world);

    REQUIRE(world.Get<CHealth>(entity) != nullptr);
    REQUIRE(world.Get<CHealth>(entity)->hp == 7);
}

TEST_CASE("Interleaved payloads of different alignment survive the flush", "[ecs][commands]")
{
    World world;
    CommandBuffer commands;

    // CSmall first, so the buffer sits at an odd size when CWide is recorded
    std::vector<Entity> entities;
    for (int i = 0; i < 3; ++i)
    {
        const Entity small = world.CreateEntity();
        commands.Add(small, CSmall{static_cast<int16_t>(i)});
        entities.push_back(small);

        const Entity wide = world.CreateEntity();
        commands.Add(wide, CWide{{static_cast<float>(i), 1.0f, 2.0f, 3.0f}});
        entities.push_back(wide);
    }

    commands.Flush(world);

    for (int i = 0; i < 3; ++i)
    {
        const CSmall *small = world.Get<CSmall>(entities[static_cast<std::size_t>(i) * 2]);
        REQUIRE(small != nullptr);
        REQUIRE(small->v == static_cast<int16_t>(i));

        const CWide *wide = world.Get<CWide>(entities[static_cast<std::size_t>(i) * 2 + 1]);
        REQUIRE(wide != nullptr);
        REQUIRE(wide->m[0] == static_cast<float>(i));
        REQUIRE(wide->m[3] == 3.0f);
    }
}
