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

    // rare marker component: opted out of archetypes so it never splits one
    struct Frozen
    {
        int turnsLeft;
    };
}

template <>
struct mts::ComponentStorageInfo<Frozen>
{
    static constexpr mts::StorageKind kValue = mts::StorageKind::SparseSet;
};

TEST_CASE("World starts with only the empty archetype", "[ecs][archetype]")
{
    mts::World world;
    REQUIRE(world.ArchetypeCount() == 1);

    const mts::Entity entity = world.CreateEntity();
    REQUIRE(world.IsAlive(entity));
    REQUIRE(world.ArchetypeCount() == 1);
    REQUIRE_FALSE(world.Has<Position>(entity));
}

TEST_CASE("World AddComponent makes the component readable", "[ecs][archetype]")
{
    mts::World world;
    const mts::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});

    REQUIRE(world.Has<Position>(entity));
    REQUIRE(world.Get<Position>(entity) != nullptr);
    CHECK(world.Get<Position>(entity)->x == 1.0f);
    CHECK(world.Get<Position>(entity)->y == 2.0f);
    CHECK(world.Get<Velocity>(entity) == nullptr);
}

TEST_CASE("World preserves existing component values across an archetype move", "[ecs][archetype]")
{
    mts::World world;
    const mts::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});

    // Position had to be memcpy'd from the {Position} table into {Position,Velocity}
    REQUIRE(world.Has<Position>(entity));
    REQUIRE(world.Has<Velocity>(entity));
    CHECK(world.Get<Position>(entity)->x == 1.0f);
    CHECK(world.Get<Position>(entity)->y == 2.0f);
    CHECK(world.Get<Velocity>(entity)->dx == 3.0f);
    CHECK(world.Get<Velocity>(entity)->dy == 4.0f);
}

TEST_CASE("World reuses one archetype regardless of add order", "[ecs][archetype]")
{
    mts::World world;

    const mts::Entity a = world.CreateEntity();
    world.AddComponent(a, Position{1.0f, 1.0f});
    world.AddComponent(a, Velocity{1.0f, 1.0f});

    const mts::Entity b = world.CreateEntity();
    world.AddComponent(b, Velocity{2.0f, 2.0f});
    world.AddComponent(b, Position{2.0f, 2.0f});

    REQUIRE(world.ArchetypeOf(a) == world.ArchetypeOf(b));
    CHECK(world.Get<Position>(a)->x == 1.0f);
    CHECK(world.Get<Position>(b)->x == 2.0f);
}

TEST_CASE("World RemoveComponent drops only that component", "[ecs][archetype]")
{
    mts::World world;
    const mts::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});
    world.RemoveComponent<Velocity>(entity);

    REQUIRE(world.Has<Position>(entity));
    REQUIRE_FALSE(world.Has<Velocity>(entity));
    CHECK(world.Get<Position>(entity)->x == 1.0f);
    CHECK(world.Get<Velocity>(entity) == nullptr);
}

TEST_CASE("World repairs the entity swapped into a vacated row", "[ecs][archetype]")
{
    mts::World world;

    // three entities in the same archetype, so removing the first forces
    // the third to be swapped down into row 0
    const mts::Entity a = world.CreateEntity();
    const mts::Entity b = world.CreateEntity();
    const mts::Entity c = world.CreateEntity();
    world.AddComponent(a, Health{1});
    world.AddComponent(b, Health{2});
    world.AddComponent(c, Health{3});

    world.DestroyEntity(a);

    REQUIRE_FALSE(world.IsAlive(a));
    REQUIRE(world.IsAlive(b));
    REQUIRE(world.IsAlive(c));
    CHECK(world.Get<Health>(b)->hp == 2);
    CHECK(world.Get<Health>(c)->hp == 3);
}

TEST_CASE("World destroys every component of an entity in one row removal", "[ecs][archetype]")
{
    mts::World world;
    const mts::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    world.AddComponent(entity, Velocity{3.0f, 4.0f});
    world.DestroyEntity(entity);

    REQUIRE_FALSE(world.IsAlive(entity));
    CHECK(world.Get<Position>(entity) == nullptr);
    CHECK(world.Get<Velocity>(entity) == nullptr);
}

TEST_CASE("World recycles entity indices with a bumped generation", "[ecs][archetype]")
{
    mts::World world;

    const mts::Entity first = world.CreateEntity();
    world.AddComponent(first, Health{7});
    world.DestroyEntity(first);

    const mts::Entity second = world.CreateEntity();

    CHECK(second.mIndex == first.mIndex);
    CHECK(second.mGeneration != first.mGeneration);
    REQUIRE(world.IsAlive(second));
    REQUIRE_FALSE(world.IsAlive(first));
    // the recycled handle starts clean, not holding the old entity's Health
    CHECK_FALSE(world.Has<Health>(second));
}

TEST_CASE("World keeps sparse components out of the archetype", "[ecs][archetype][sparse]")
{
    mts::World world;

    const mts::Entity a = world.CreateEntity();
    const mts::Entity b = world.CreateEntity();
    world.AddComponent(a, Position{1.0f, 1.0f});
    world.AddComponent(b, Position{2.0f, 2.0f});

    REQUIRE(world.ArchetypeOf(a) == world.ArchetypeOf(b));
    const std::size_t archetypeCount = world.ArchetypeCount();

    // the whole point of opting out: a rare component must not fragment
    // the archetype its holder happens to sit in
    world.AddComponent(a, Frozen{3});

    CHECK(world.ArchetypeOf(a) == world.ArchetypeOf(b));
    CHECK(world.ArchetypeCount() == archetypeCount);

    REQUIRE(world.Has<Frozen>(a));
    REQUIRE_FALSE(world.Has<Frozen>(b));
    CHECK(world.Get<Frozen>(a)->turnsLeft == 3);
    CHECK(world.Get<Frozen>(b) == nullptr);
}

TEST_CASE("World removes a sparse component without an archetype move", "[ecs][archetype][sparse]")
{
    mts::World world;
    const mts::Entity entity = world.CreateEntity();

    world.AddComponent(entity, Position{1.0f, 2.0f});
    const mts::Archetype *before = world.ArchetypeOf(entity);

    world.AddComponent(entity, Frozen{5});
    world.RemoveComponent<Frozen>(entity);

    CHECK(world.ArchetypeOf(entity) == before);
    REQUIRE_FALSE(world.Has<Frozen>(entity));
    REQUIRE(world.Has<Position>(entity));
    CHECK(world.Get<Position>(entity)->x == 1.0f);
}

TEST_CASE("World clears sparse components on destroy", "[ecs][archetype][sparse]")
{
    mts::World world;

    const mts::Entity first = world.CreateEntity();
    world.AddComponent(first, Frozen{9});
    world.DestroyEntity(first);

    // the recycled index must not inherit the dead entity's sparse component
    const mts::Entity second = world.CreateEntity();
    REQUIRE(second.mIndex == first.mIndex);
    CHECK_FALSE(world.Has<Frozen>(second));
    CHECK(world.Get<Frozen>(second) == nullptr);
}
