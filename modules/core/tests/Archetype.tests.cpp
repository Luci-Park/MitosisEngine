/**
 * @file Archetype.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/World archetype storage.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace
{
    struct Position
    {
        float x;
        float y;
    };

    struct Velocity
    {
        float dx;
        float dy;
    };

    struct Health
    {
        int hp;
    };

    // marker component: no fields, so it carries a signature bit and no column
    struct Frozen
    {
    };
}

TEST_CASE("World starts with only the empty archetype", "[ecs][archetype]")
{
    mir::World world;
    REQUIRE(world.ArchetypeCount() == 1);

    const mir::Entity entity = world.CreateEntity();
    REQUIRE(world.IsAlive(entity));
    REQUIRE(world.ArchetypeCount() == 1);
    REQUIRE_FALSE(world.Has<Position>(entity));
}

TEST_CASE("World AddComponent makes the component readable", "[ecs][archetype]")
{
    mir::World world;
    const mir::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});

    REQUIRE(world.Has<Position>(entity));
    REQUIRE(world.GetComponent<Position>(entity) != nullptr);
    CHECK(world.GetComponent<Position>(entity)->x == 1.0f);
    CHECK(world.GetComponent<Position>(entity)->y == 2.0f);
    CHECK(world.GetComponent<Velocity>(entity) == nullptr);
}

TEST_CASE("World preserves existing component values across an archetype move", "[ecs][archetype]")
{
    mir::World world;
    const mir::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});

    // Position had to be memcpy'd from the {Position} table into {Position,Velocity}
    REQUIRE(world.Has<Position>(entity));
    REQUIRE(world.Has<Velocity>(entity));
    CHECK(world.GetComponent<Position>(entity)->x == 1.0f);
    CHECK(world.GetComponent<Position>(entity)->y == 2.0f);
    CHECK(world.GetComponent<Velocity>(entity)->dx == 3.0f);
    CHECK(world.GetComponent<Velocity>(entity)->dy == 4.0f);
}

TEST_CASE("World reuses one archetype regardless of add order", "[ecs][archetype]")
{
    mir::World world;

    const mir::Entity a = world.CreateEntity();
    world.AddComponent(a, Position{1.0f, 1.0f});
    world.AddComponent(a, Velocity{1.0f, 1.0f});

    const mir::Entity b = world.CreateEntity();
    world.AddComponent(b, Velocity{2.0f, 2.0f});
    world.AddComponent(b, Position{2.0f, 2.0f});

    REQUIRE(world.ArchetypeOf(a) == world.ArchetypeOf(b));
    CHECK(world.GetComponent<Position>(a)->x == 1.0f);
    CHECK(world.GetComponent<Position>(b)->x == 2.0f);
}

TEST_CASE("World RemoveComponent drops only that component", "[ecs][archetype]")
{
    mir::World world;
    const mir::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});
    world.RemoveComponent<Velocity>(entity);

    REQUIRE(world.Has<Position>(entity));
    REQUIRE_FALSE(world.Has<Velocity>(entity));
    CHECK(world.GetComponent<Position>(entity)->x == 1.0f);
    CHECK(world.GetComponent<Velocity>(entity) == nullptr);
}

TEST_CASE("World repairs the entity swapped into a vacated row", "[ecs][archetype]")
{
    mir::World world;

    // three entities in the same archetype, so removing the first forces
    // the third to be swapped down into row 0
    const mir::Entity a = world.CreateEntity();
    const mir::Entity b = world.CreateEntity();
    const mir::Entity c = world.CreateEntity();
    world.AddComponent(a, Health{1});
    world.AddComponent(b, Health{2});
    world.AddComponent(c, Health{3});

    world.DestroyEntity(a);

    REQUIRE_FALSE(world.IsAlive(a));
    REQUIRE(world.IsAlive(b));
    REQUIRE(world.IsAlive(c));
    CHECK(world.GetComponent<Health>(b)->hp == 2);
    CHECK(world.GetComponent<Health>(c)->hp == 3);
}

TEST_CASE("World destroys every component of an entity in one row removal", "[ecs][archetype]")
{
    mir::World world;
    const mir::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});
    world.DestroyEntity(entity);

    REQUIRE_FALSE(world.IsAlive(entity));
    CHECK(world.GetComponent<Position>(entity) == nullptr);
    CHECK(world.GetComponent<Velocity>(entity) == nullptr);
}

TEST_CASE("World recycles entity indices with a bumped generation", "[ecs][archetype]")
{
    mir::World world;

    const mir::Entity first = world.CreateEntity();
    world.AddComponent(first, Health{7});
    world.DestroyEntity(first);

    const mir::Entity second = world.CreateEntity();

    CHECK(second.mIndex == first.mIndex);
    CHECK(second.mGeneration != first.mGeneration);
    REQUIRE(world.IsAlive(second));
    REQUIRE_FALSE(world.IsAlive(first));
    // the recycled handle starts clean, not holding the old entity's Health
    CHECK_FALSE(world.Has<Health>(second));
}

TEST_CASE("World tags an entity without giving the archetype a column", "[ecs][archetype][tag]")
{
    mir::World world;

    const mir::Entity a = world.CreateEntity();
    const mir::Entity b = world.CreateEntity();
    world.AddComponent(a, Position{1.0f, 1.0f});
    world.AddComponent(b, Position{2.0f, 2.0f});

    REQUIRE(world.ArchetypeOf(a) == world.ArchetypeOf(b));

    world.AddTag<Frozen>(a);

    // a tag is a real signature bit, so it does split the archetype - what it
    // does not do is add a column, which is where the per-row cost would be
    CHECK(world.ArchetypeOf(a) != world.ArchetypeOf(b));
    CHECK(world.ArchetypeOf(a)->Columns().size() == 1);
    CHECK(world.ArchetypeOf(a)->FindColumn(mts::TypeIdOf<Frozen>()) == nullptr);

    REQUIRE(world.Has<Frozen>(a));
    REQUIRE_FALSE(world.Has<Frozen>(b));
}

TEST_CASE("World keeps other components across a tag add and remove", "[ecs][archetype][tag]")
{
    mir::World world;
    const mir::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    const mir::Archetype *before = world.ArchetypeOf(entity);

    world.AddTag<Frozen>(entity);
    REQUIRE(world.ArchetypeOf(entity) != before);
    CHECK(world.GetComponent<Position>(entity)->x == 1.0f);

    world.RemoveComponent<Frozen>(entity);

    // back to the table it started in, with its column value intact
    CHECK(world.ArchetypeOf(entity) == before);
    REQUIRE_FALSE(world.Has<Frozen>(entity));
    REQUIRE(world.Has<Position>(entity));
    CHECK(world.GetComponent<Position>(entity)->x == 1.0f);
}

TEST_CASE("World clears tags on destroy", "[ecs][archetype][tag]")
{
    mir::World world;

    const mir::Entity first = world.CreateEntity();
    world.AddTag<Frozen>(first);
    world.DestroyEntity(first);

    // the recycled index must not inherit the dead entity's tag
    const mir::Entity second = world.CreateEntity();
    REQUIRE(second.mIndex == first.mIndex);
    CHECK_FALSE(world.Has<Frozen>(second));
}

TEST_CASE("A tagged row survives a swap-remove", "[ecs][archetype][tag]")
{
    mts::World world;

    // three entities in one tagged table, so removing the first swaps the
    // last into its row - the path that walks every column of a table that
    // has one fewer column than it has signature bits
    const mts::Entity a = world.CreateEntity();
    const mts::Entity b = world.CreateEntity();
    const mts::Entity c = world.CreateEntity();

    for (const auto &[entity, x] : {std::pair{a, 1.0f}, std::pair{b, 2.0f}, std::pair{c, 3.0f}})
    {
        world.AddComponent(entity, Position{x, x});
        world.AddTag<Frozen>(entity);
    }

    world.DestroyEntity(a);

    REQUIRE(world.Has<Frozen>(c));
    CHECK(world.GetComponent<Position>(c)->x == 3.0f);
    CHECK(world.GetComponent<Position>(b)->x == 2.0f);
}
