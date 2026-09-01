/**
 * @file Paths.tests.cpp
 * @author Sumin Park
 * @brief Tests for core/fs/Paths.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <core/fs/Paths.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ExecutableDir points at a real directory", "[paths]")
{
    REQUIRE(std::filesystem::exists(mts::ExecutableDir()));
    REQUIRE(std::filesystem::is_directory(mts::ExecutableDir()));
}

TEST_CASE("ShaderPath appends shaders/<name> under the executable directory", "[paths]")
{
    const auto path = mts::ShaderPath("triangle.spv");
    REQUIRE(path == mts::ExecutableDir() / "shaders" / "triangle.spv");
}

TEST_CASE("CookedAssetsDir points at cooked/ under the executable directory", "[paths]")
{
    const auto path = mts::CookedAssetsDir();
    REQUIRE(path == mts::ExecutableDir() / "cooked");
}
