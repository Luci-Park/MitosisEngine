/**
 * @file ComponentStorage.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/SparseSetStorage.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/SparseSetStorage.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
    struct Position
    {
        float x;
        float y;
    };

    using Storage = mts::SparseSetStorage<Position>;

    mts::Entity MakeEntity(uint32_t index, uint32_t generation = 0)
    {
        return mts::Entity{index, generation};
    }
}

TEST_CASE("SparseSetStorage starts empty", "[ecs][storage]")
{
    Storage storage;
    REQUIRE_FALSE(storage.Has(MakeEntity(0)));
    REQUIRE(storage.Get(MakeEntity(0)) == nullptr);
}

TEST_CASE("SparseSetStorage Add makes the component visible", "[ecs][storage]")
{
    Storage storage;
    const mts::Entity entity = MakeEntity(3);

    storage.Add(entity, Position{1.0f, 2.0f});

    REQUIRE(storage.Has(entity));
    REQUIRE(storage.Get(entity) != nullptr);
    CHECK(storage.Get(entity)->x == 1.0f);
    CHECK(storage.Get(entity)->y == 2.0f);
}

TEST_CASE("SparseSetStorage Remove clears the component", "[ecs][storage]")
{
    Storage storage;
    const mts::Entity entity = MakeEntity(3);

    storage.Add(entity, Position{1.0f, 2.0f});
    storage.Remove(entity);

    REQUIRE_FALSE(storage.Has(entity));
    REQUIRE(storage.Get(entity) == nullptr);
}

TEST_CASE("SparseSetStorage keeps unrelated entities independent", "[ecs][storage]")
{
    Storage storage;
    const mts::Entity a = MakeEntity(1);
    const mts::Entity b = MakeEntity(2);
    const mts::Entity c = MakeEntity(3);

    storage.Add(a, Position{1.0f, 1.0f});
    storage.Add(b, Position{2.0f, 2.0f});
    storage.Add(c, Position{3.0f, 3.0f});

    // remove the middle entry: this is what exercises the swap-remove fixup
    // (b and c both need to still resolve correctly)
    storage.Remove(b);

    REQUIRE(storage.Has(a));
    REQUIRE(storage.Has(c));
    REQUIRE_FALSE(storage.Has(b));

    CHECK(storage.Get(a)->x == 1.0f);
    CHECK(storage.Get(c)->x == 3.0f);
}

TEST_CASE("SparseSetStorage Add after Remove reuses the entity index", "[ecs][storage]")
{
    Storage storage;
    const mts::Entity entity = MakeEntity(5);

    storage.Add(entity, Position{1.0f, 1.0f});
    storage.Remove(entity);
    storage.Add(entity, Position{9.0f, 9.0f});

    REQUIRE(storage.Has(entity));
    CHECK(storage.Get(entity)->x == 9.0f);
}
