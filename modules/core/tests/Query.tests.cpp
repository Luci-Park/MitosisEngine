/**
 * @file Query.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/World component queries.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/World.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace
{
    struct QPosition
    {
        float x;
        float y;
    };

    struct QVelocity
    {
        float dx;
        float dy;
    };

    struct QHealth
    {
        int hp;
    };

    // rare marker: opted out of archetypes, so queries must filter it per entity
    struct QStunned
    {
        int turnsLeft;
    };
}

MTS_COMPONENT_SPARSE(QStunned);

using namespace mts;

namespace
{
    // entities a query visited, order-independent for comparison
    std::vector<uint32_t> SortedIndices(std::vector<Entity> visited)
    {
        std::vector<uint32_t> indices;
        for (Entity e : visited)
            indices.push_back(e.mIndex);
        std::sort(indices.begin(), indices.end());
        return indices;
    }
}

TEST_CASE("ForEach visits only entities holding every component", "[ecs][query]")
{
    World world;

    const Entity both = world.CreateEntity();
    world.AddComponent(both, QPosition{1.0f, 2.0f});
    world.AddComponent(both, QVelocity{10.0f, 20.0f});

    const Entity positionOnly = world.CreateEntity();
    world.AddComponent(positionOnly, QPosition{3.0f, 4.0f});

    const Entity empty = world.CreateEntity();

    SECTION("single component matches every superset archetype")
    {
        std::vector<Entity> visited;
        world.ForEach<QPosition>([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(SortedIndices(visited) == SortedIndices({both, positionOnly}));
    }

    SECTION("two components exclude partial matches")
    {
        std::vector<Entity> visited;
        world.ForEach<QPosition, QVelocity>([&](Entity e, QPosition &, QVelocity &) { visited.push_back(e); });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == both);
    }

    SECTION("no match yields no calls")
    {
        int calls = 0;
        world.ForEach<QHealth>([&](Entity, QHealth &) { ++calls; });

        REQUIRE(calls == 0);
        REQUIRE(!world.Has<QHealth>(empty));
    }
}

TEST_CASE("ForEach yields references that write through to storage", "[ecs][query]")
{
    World world;

    const Entity entity = world.CreateEntity();
    world.AddComponent(entity, QPosition{0.0f, 0.0f});
    world.AddComponent(entity, QVelocity{2.0f, -3.0f});

    world.ForEach<QPosition, QVelocity>([](Entity, QPosition &p, QVelocity &v)
                                        {
                                            p.x += v.dx;
                                            p.y += v.dy;
                                        });

    REQUIRE(world.Get<QPosition>(entity)->x == 2.0f);
    REQUIRE(world.Get<QPosition>(entity)->y == -3.0f);
}

TEST_CASE("ForEach spans multiple archetypes", "[ecs][query]")
{
    World world;

    // same required component, three different archetypes
    const Entity a = world.CreateEntity();
    world.AddComponent(a, QPosition{});

    const Entity b = world.CreateEntity();
    world.AddComponent(b, QPosition{});
    world.AddComponent(b, QVelocity{});

    const Entity c = world.CreateEntity();
    world.AddComponent(c, QPosition{});
    world.AddComponent(c, QHealth{});

    std::vector<Entity> visited;
    world.ForEach<QPosition>([&](Entity e, QPosition &) { visited.push_back(e); });

    REQUIRE(SortedIndices(visited) == SortedIndices({a, b, c}));
}

TEST_CASE("ForEach filters sparse components per entity", "[ecs][query]")
{
    World world;

    const Entity stunned = world.CreateEntity();
    world.AddComponent(stunned, QPosition{1.0f, 1.0f});
    world.AddComponent(stunned, QStunned{3});

    const Entity awake = world.CreateEntity();
    world.AddComponent(awake, QPosition{2.0f, 2.0f});

    SECTION("mixed table and sparse query")
    {
        std::vector<Entity> visited;
        world.ForEach<QPosition, QStunned>([&](Entity e, QPosition &, QStunned &s)
                                           {
                                               visited.push_back(e);
                                               --s.turnsLeft;
                                           });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == stunned);
        REQUIRE(world.Get<QStunned>(stunned)->turnsLeft == 2);
    }

    SECTION("sparse-only query still walks archetypes")
    {
        std::vector<Entity> visited;
        world.ForEach<QStunned>([&](Entity e, QStunned &) { visited.push_back(e); });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == stunned);
    }

    SECTION("sparse component with no storage yet matches nothing")
    {
        World fresh;
        const Entity e = fresh.CreateEntity();
        fresh.AddComponent(e, QPosition{});

        int calls = 0;
        fresh.ForEach<QPosition, QStunned>([&](Entity, QPosition &, QStunned &) { ++calls; });

        REQUIRE(calls == 0);
    }

    SECTION("removing the sparse component drops the entity from the query")
    {
        world.RemoveComponent<QStunned>(stunned);

        int calls = 0;
        world.ForEach<QPosition, QStunned>([&](Entity, QPosition &, QStunned &) { ++calls; });

        REQUIRE(calls == 0);
    }
}

TEST_CASE("ForEach skips destroyed entities", "[ecs][query]")
{
    World world;

    const Entity kept = world.CreateEntity();
    world.AddComponent(kept, QPosition{5.0f, 6.0f});

    const Entity doomed = world.CreateEntity();
    world.AddComponent(doomed, QPosition{7.0f, 8.0f});
    world.DestroyEntity(doomed);

    std::vector<Entity> visited;
    world.ForEach<QPosition>([&](Entity e, QPosition &) { visited.push_back(e); });

    REQUIRE(visited.size() == 1);
    REQUIRE(visited[0] == kept);
    REQUIRE(world.Get<QPosition>(visited[0])->x == 5.0f);
}

namespace
{
    struct QFrozen
    {
        int unused;
    };

    struct QShield
    {
        int amount;
    };
}

TEST_CASE("ForEach accepts const terms", "[ecs][query]")
{
    World world;

    const Entity entity = world.CreateEntity();
    world.AddComponent(entity, QPosition{1.0f, 2.0f});
    world.AddComponent(entity, QVelocity{5.0f, 6.0f});

    // const term binds a const ref; the non-const term still writes through
    world.ForEach<QPosition, const QVelocity>([](Entity, QPosition &p, const QVelocity &v)
                                              {
                                                  static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
                                                  p.x += v.dx;
                                              });

    REQUIRE(world.Get<QPosition>(entity)->x == 6.0f);
}

TEST_CASE("Query filters by With and Without", "[ecs][query]")
{
    World world;

    const Entity plain = world.CreateEntity();
    world.AddComponent(plain, QPosition{});

    const Entity frozen = world.CreateEntity();
    world.AddComponent(frozen, QPosition{});
    world.AddComponent(frozen, QFrozen{});

    const Entity shielded = world.CreateEntity();
    world.AddComponent(shielded, QPosition{});
    world.AddComponent(shielded, QShield{});

    SECTION("Without excludes the archetype carrying the term")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(Without<QFrozen>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(SortedIndices(visited) == SortedIndices({plain, shielded}));
    }

    SECTION("With narrows without adding a callback argument")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(With<QShield>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == shielded);
    }

    SECTION("With and Without combine")
    {
        int calls = 0;
        world.GetOrCreateQuery<QPosition>(With<QShield>{}, Without<QFrozen>{})
            .ForEach([&](Entity, QPosition &) { ++calls; });

        REQUIRE(calls == 1);
    }
}

TEST_CASE("Query Or matches any listed component", "[ecs][query]")
{
    World world;

    const Entity frozen = world.CreateEntity();
    world.AddComponent(frozen, QPosition{});
    world.AddComponent(frozen, QFrozen{});

    const Entity shielded = world.CreateEntity();
    world.AddComponent(shielded, QPosition{});
    world.AddComponent(shielded, QShield{});

    const Entity neither = world.CreateEntity();
    world.AddComponent(neither, QPosition{});

    std::vector<Entity> visited;
    world.GetOrCreateQuery<QPosition>(Or<QFrozen, QShield>{})
        .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

    REQUIRE(SortedIndices(visited) == SortedIndices({frozen, shielded}));
}

TEST_CASE("Query filters on sparse members per row", "[ecs][query]")
{
    World world;

    const Entity stunned = world.CreateEntity();
    world.AddComponent(stunned, QPosition{});
    world.AddComponent(stunned, QStunned{1});

    const Entity awake = world.CreateEntity();
    world.AddComponent(awake, QPosition{});

    SECTION("Without a sparse component")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(Without<QStunned>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == awake);
    }

    SECTION("With a sparse component")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(With<QStunned>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(visited.size() == 1);
        REQUIRE(visited[0] == stunned);
    }
}

TEST_CASE("Query cache survives and refreshes across calls", "[ecs][query]")
{
    World world;

    const Entity a = world.CreateEntity();
    world.AddComponent(a, QPosition{});

    // same shape, same filters -> same cached Query object
    auto &first = world.GetOrCreateQuery<QPosition>();
    auto &second = world.GetOrCreateQuery<QPosition>();
    REQUIRE(&first == &second);

    // differing filters key to a different cache entry
    auto &filtered = world.GetOrCreateQuery<QPosition>(Without<QFrozen>{});
    REQUIRE(static_cast<void *>(&filtered) != static_cast<void *>(&first));

    int calls = 0;
    first.ForEach([&](Entity, QPosition &) { ++calls; });
    REQUIRE(calls == 1);

    SECTION("rows added to an already-matched archetype are picked up")
    {
        const Entity b = world.CreateEntity();
        world.AddComponent(b, QPosition{}); // no new archetype, so no generation bump

        calls = 0;
        first.ForEach([&](Entity, QPosition &) { ++calls; });
        REQUIRE(calls == 2);
    }

    SECTION("a brand new archetype invalidates the cache")
    {
        const std::size_t before = world.Generation();

        const Entity b = world.CreateEntity();
        world.AddComponent(b, QPosition{});
        world.AddComponent(b, QHealth{}); // new signature -> new archetype

        REQUIRE(world.Generation() > before);

        std::vector<Entity> visited;
        first.ForEach([&](Entity e, QPosition &) { visited.push_back(e); });
        REQUIRE(SortedIndices(visited) == SortedIndices({a, b}));
    }
}

TEST_CASE("Query cache keys on filters, not just data terms", "[ecs][query]")
{
    World world;

    const Entity frozenPlayer = world.CreateEntity();
    world.AddComponent(frozenPlayer, QPosition{});
    world.AddComponent(frozenPlayer, QVelocity{});
    world.AddComponent(frozenPlayer, QFrozen{});
    world.AddComponent(frozenPlayer, QShield{});

    const Entity mover = world.CreateEntity();
    world.AddComponent(mover, QPosition{});
    world.AddComponent(mover, QVelocity{});

    // same Ts..., different filter sets -> must be distinct cached objects
    auto &excludeFrozen = world.GetOrCreateQuery<QPosition, QVelocity>(Without<QFrozen>{});
    auto &requireShield = world.GetOrCreateQuery<QPosition, QVelocity>(With<QShield>{});

    REQUIRE(&excludeFrozen != &requireShield);

    std::vector<Entity> withoutFrozen;
    excludeFrozen.ForEach([&](Entity e, QPosition &, QVelocity &) { withoutFrozen.push_back(e); });

    std::vector<Entity> withShield;
    requireShield.ForEach([&](Entity e, QPosition &, QVelocity &) { withShield.push_back(e); });

    REQUIRE(withoutFrozen.size() == 1);
    REQUIRE(withoutFrozen[0] == mover);
    REQUIRE(withShield.size() == 1);
    REQUIRE(withShield[0] == frozenPlayer);

    // and each still resolves back to its own entry on a repeat lookup
    REQUIRE(&world.GetOrCreateQuery<QPosition, QVelocity>(Without<QFrozen>{}) == &excludeFrozen);
    REQUIRE(&world.GetOrCreateQuery<QPosition, QVelocity>(With<QShield>{}) == &requireShield);
}

namespace
{
    struct QMelee
    {
        int reach;
    };

    struct QRanged
    {
        int range;
    };
}

TEST_CASE("Query Or terms are independent clauses", "[ecs][query]")
{
    World world;

    // clause 1: QMelee or QRanged. clause 2: QFrozen or QShield.
    const Entity meleeFrozen = world.CreateEntity();
    world.AddComponent(meleeFrozen, QPosition{});
    world.AddComponent(meleeFrozen, QMelee{});
    world.AddComponent(meleeFrozen, QFrozen{});

    const Entity rangedShield = world.CreateEntity();
    world.AddComponent(rangedShield, QPosition{});
    world.AddComponent(rangedShield, QRanged{});
    world.AddComponent(rangedShield, QShield{});

    // satisfies clause 1 only - a single merged any-of mask would wrongly accept it
    const Entity meleeOnly = world.CreateEntity();
    world.AddComponent(meleeOnly, QPosition{});
    world.AddComponent(meleeOnly, QMelee{});

    // satisfies clause 2 only
    const Entity shieldOnly = world.CreateEntity();
    world.AddComponent(shieldOnly, QPosition{});
    world.AddComponent(shieldOnly, QShield{});

    const Entity neither = world.CreateEntity();
    world.AddComponent(neither, QPosition{});

    SECTION("two Or terms AND together")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(Or<QMelee, QRanged>{}, Or<QFrozen, QShield>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(SortedIndices(visited) == SortedIndices({meleeFrozen, rangedShield}));
    }

    SECTION("one Or term over all four is still plain any-of")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(Or<QMelee, QRanged, QFrozen, QShield>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(SortedIndices(visited) ==
                SortedIndices({meleeFrozen, rangedShield, meleeOnly, shieldOnly}));
    }

    SECTION("Or clauses combine with With and Without")
    {
        std::vector<Entity> visited;
        world.GetOrCreateQuery<QPosition>(Or<QMelee, QRanged>{}, Without<QFrozen>{})
            .ForEach([&](Entity e, QPosition &) { visited.push_back(e); });

        REQUIRE(SortedIndices(visited) == SortedIndices({rangedShield, meleeOnly}));
    }
}

TEST_CASE("Query allows a nested walk when no rebuild is needed", "[ecs][query]")
{
    World world;

    for (int i = 0; i < 3; ++i)
    {
        const Entity e = world.CreateEntity();
        world.AddComponent(e, QPosition{static_cast<float>(i), 0.0f});
    }

    // pairwise scan: the same query re-entered from its own callback. Safe while
    // no archetype is created, and the guard must not reject it.
    auto &query = world.GetOrCreateQuery<QPosition>();

    int pairs = 0;
    query.ForEach([&](Entity outer, QPosition &)
                  {
                      query.ForEach([&](Entity inner, QPosition &)
                                    {
                                        if (!(outer == inner))
                                            ++pairs;
                                    });
                  });

    REQUIRE(pairs == 6); // 3 entities, ordered pairs
}
