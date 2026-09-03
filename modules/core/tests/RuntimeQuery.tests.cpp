/**
 * @file RuntimeQuery.tests.cpp
 * @author Sumin Park
 * @brief Tests for queries whose terms are chosen at runtime.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/RuntimeQuery.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace
{
    using mts::ComponentOps;
    using mts::ComponentRegistry;
    using mts::Entity;
    using mts::FieldKind;
    using mts::RuntimeFieldDecl;
    using mts::RuntimeQuery;
    using mts::TypeId;
    using mts::World;

    struct RqPosition
    {
        float x = 0.0f;
    };

    struct RqVelocity
    {
        float x = 0.0f;
    };

    struct RqFrozen
    {
    };

    const ComponentOps &Position() { return ComponentRegistry::Instance().Register<RqPosition>(); }
    const ComponentOps &Velocity() { return ComponentRegistry::Instance().Register<RqVelocity>(); }
    const ComponentOps &Frozen() { return ComponentRegistry::Instance().Register<RqFrozen>(); }

    Entity Spawn(World &world, float position, float velocity)
    {
        const Entity entity = world.CreateEntity();
        world.AddComponent<RqPosition>(entity, RqPosition{position});
        world.AddComponent<RqVelocity>(entity, RqVelocity{velocity});
        return entity;
    }
}

TEST_CASE("A runtime query walks the same rows a templated one would")
{
    const std::array terms{Position().mType, Velocity().mType};

    World world;
    Spawn(world, 1.0f, 10.0f);
    Spawn(world, 2.0f, 20.0f);

    // an entity with only one of the two terms must not match
    const Entity partial = world.CreateEntity();
    world.AddComponent<RqPosition>(partial, RqPosition{99.0f});

    RuntimeQuery query(world, terms);

    float positions = 0.0f;
    float velocities = 0.0f;
    std::size_t visited = 0;

    query.ForEach(
        [&](Entity, std::span<void *const> row)
        {
            ++visited;
            positions += static_cast<RqPosition *>(row[0])->x;
            velocities += static_cast<RqVelocity *>(row[1])->x;
        });

    CHECK(visited == 2);
    CHECK(positions == 3.0f);
    CHECK(velocities == 30.0f);
}

TEST_CASE("A runtime query writes through to the column")
{
    const std::array terms{Position().mType};

    World world;
    const Entity entity = Spawn(world, 1.0f, 0.0f);

    RuntimeQuery query(world, terms);
    query.ForEach([](Entity, std::span<void *const> row) { static_cast<RqPosition *>(row[0])->x = 5.0f; });

    CHECK(world.Get<RqPosition>(entity)->x == 5.0f);
}

TEST_CASE("A runtime query narrows by With and Without")
{
    const std::array terms{Position().mType};

    World world;
    const Entity moving = Spawn(world, 1.0f, 1.0f);
    const Entity frozen = Spawn(world, 2.0f, 2.0f);
    world.AddComponent<RqFrozen>(frozen, RqFrozen{});

    RuntimeQuery query(world, terms);
    query.With(Velocity().mType).Without(Frozen().mType);

    std::vector<Entity> seen;
    query.ForEach([&](Entity entity, std::span<void *const>) { seen.push_back(entity); });

    REQUIRE(seen.size() == 1);
    CHECK(seen.front() == moving);
    CHECK(seen.front() != frozen);
}

TEST_CASE("A runtime query picks up an archetype created after its first walk")
{
    const std::array terms{Position().mType};

    World world;
    Spawn(world, 1.0f, 1.0f);

    RuntimeQuery query(world, terms);

    // Relative, not absolute: adding components one at a time leaves the
    // intermediate archetypes behind - {Position} still exists, empty, after
    // the entity moved on to {Position, Velocity} - and a one-term query
    // matches those too. What is being tested is that the cache notices a new
    // one, not how many an add happens to leave.
    const std::size_t before = query.MatchedArchetypeCount();

    // a new component combination is a new archetype, and the generation stamp
    // is what makes the cached match list notice
    const Entity tagged = world.CreateEntity();
    world.AddComponent<RqPosition>(tagged, RqPosition{3.0f});
    world.AddComponent<RqFrozen>(tagged, RqFrozen{});

    CHECK(query.MatchedArchetypeCount() == before + 1);

    // the empty intermediates contribute no rows
    std::size_t visited = 0;
    query.ForEach([&](Entity, std::span<void *const>) { ++visited; });
    CHECK(visited == 2);
}

TEST_CASE("A runtime query matches script-declared components")
{
    constexpr RuntimeFieldDecl fields[] = {{"hp", FieldKind::Int}};
    const ComponentOps &script = ComponentRegistry::Instance().RegisterRuntime("RqScriptHealth", fields);

    World world;
    for (int32_t hp : {3, 4})
    {
        const Entity entity = world.CreateEntity();
        script.AddDefault(world, entity);
        script.FindField("hp")->Write(script.Get(world, entity), &hp);
    }

    // one entity that has only the C++ component, to prove the term narrows
    world.AddComponent<RqPosition>(world.CreateEntity(), RqPosition{});

    const std::array terms{script.mType};
    RuntimeQuery query(world, terms);

    int32_t total = 0;
    query.ForEach(
        [&](Entity, std::span<void *const> row)
        {
            int32_t hp = 0;
            script.FindField("hp")->Read(row[0], &hp);
            total += hp;
        });

    CHECK(total == 7);
}

TEST_CASE("A runtime query mixes a script component with a C++ one")
{
    constexpr RuntimeFieldDecl fields[] = {{"amount", FieldKind::Float}};
    const ComponentOps &script = ComponentRegistry::Instance().RegisterRuntime("RqScriptBoost", fields);

    World world;
    const Entity entity = Spawn(world, 1.0f, 2.0f);
    script.AddDefault(world, entity);

    const float amount = 0.5f;
    script.FindField("amount")->Write(script.Get(world, entity), &amount);

    const std::array terms{Position().mType, script.mType};
    RuntimeQuery query(world, terms);

    std::size_t visited = 0;
    query.ForEach(
        [&](Entity, std::span<void *const> row)
        {
            ++visited;

            float boost = 0.0f;
            script.FindField("amount")->Read(row[1], &boost);
            static_cast<RqPosition *>(row[0])->x += boost;
        });

    CHECK(visited == 1);
    CHECK(world.Get<RqPosition>(entity)->x == 1.5f);
}

TEST_CASE("A runtime query reports the world as iterating")
{
    // The guard is shared with Query, so a binding reached from inside a
    // runtime walk defers its structural changes for the same reason.
    const std::array terms{Position().mType};

    World world;
    Spawn(world, 1.0f, 1.0f);

    RuntimeQuery query(world, terms);
    CHECK_FALSE(world.IsIterating());

    bool checked = false;
    query.ForEach(
        [&](Entity, std::span<void *const>)
        {
            checked = true;
            CHECK(world.IsIterating());
        });

    REQUIRE(checked);
    CHECK_FALSE(world.IsIterating());
}
