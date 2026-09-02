/**
 * @file Transform.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/ecs/components/Transform.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/ecs/components/Transform.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    using Catch::Approx;

    void RequireNear(const glm::vec3 &actual, const glm::vec3 &expected)
    {
        CHECK(actual.x == Approx(expected.x).margin(1e-5));
        CHECK(actual.y == Approx(expected.y).margin(1e-5));
        CHECK(actual.z == Approx(expected.z).margin(1e-5));
    }

    glm::vec3 Apply(const glm::mat4 &m, const glm::vec3 &p)
    {
        return glm::vec3(m * glm::vec4(p, 1.0f));
    }
}

TEST_CASE("Transform stays memcpy-safe for the ECS", "[ecs][transform]")
{
    // The column relocates rows with memcpy, so the traits are load-bearing,
    // not decoration. Restated here so a bad glm config fails a test rather
    // than only failing whichever translation unit happens to include it.
    STATIC_REQUIRE(std::is_trivially_copyable_v<mts::Transform>);
    STATIC_REQUIRE(std::is_standard_layout_v<mts::Transform>);
    STATIC_REQUIRE(alignof(mts::Transform) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

TEST_CASE("Default Transform is the identity", "[ecs][transform]")
{
    const mts::Transform t;
    RequireNear(Apply(t.Matrix(), glm::vec3(1.0f, 2.0f, 3.0f)), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_CASE("Transform translates", "[ecs][transform]")
{
    mts::Transform t;
    t.SetPosition(glm::vec3(5.0f, -1.0f, 2.0f));

    RequireNear(Apply(t.Matrix(), glm::vec3(0.0f)), glm::vec3(5.0f, -1.0f, 2.0f));
}

TEST_CASE("Transform scales", "[ecs][transform]")
{
    mts::Transform t;
    t.SetScale(glm::vec3(2.0f, 3.0f, 4.0f));

    RequireNear(Apply(t.Matrix(), glm::vec3(1.0f)), glm::vec3(2.0f, 3.0f, 4.0f));
}

TEST_CASE("Transform rotates", "[ecs][transform]")
{
    mts::Transform t;
    // +90 degrees about Z takes +X to +Y
    t.SetRotation(glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f)));

    RequireNear(Apply(t.Matrix(), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST_CASE("Transform applies scale, then rotation, then translation", "[ecs][transform]")
{
    // Order is the whole contract: scaling after rotating would shear, and
    // rotating the translation would move the pivot off the origin.
    mts::Transform t;
    t.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    t.SetRotation(glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f)));
    t.SetScale(glm::vec3(2.0f));

    // (1,0,0) -> scale -> (2,0,0) -> rotate -> (0,2,0) -> translate -> (10,2,0)
    RequireNear(Apply(t.Matrix(), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(10.0f, 2.0f, 0.0f));

    // and it agrees with the naive three-matrix product it replaces
    const glm::mat4 expected = glm::translate(glm::mat4(1.0f), t.Position()) *
                               glm::mat4_cast(t.Rotation()) *
                               glm::scale(glm::mat4(1.0f), t.Scale());
    for (int c = 0; c < 4; ++c)
        RequireNear(glm::vec3(t.Matrix()[c]), glm::vec3(expected[c]));
}
