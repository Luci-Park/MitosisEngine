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
    REQUIRE(std::filesystem::exists(mir::ExecutableDir()));
    REQUIRE(std::filesystem::is_directory(mir::ExecutableDir()));
}

TEST_CASE("ShaderPath appends shaders/<name> under the executable directory", "[paths]")
{
    const auto path = mir::ShaderPath("triangle.spv");
    REQUIRE(path == mir::ExecutableDir() / "shaders" / "triangle.spv");
}

TEST_CASE("FontPath appends fonts/<name> under the executable directory", "[paths]")
{
    const auto path = mir::FontPath("Inter.ttf");
    REQUIRE(path == mir::ExecutableDir() / "fonts" / "Inter.ttf");
}

TEST_CASE("CookedAssetsDir points at cooked/ under the executable directory", "[paths]")
{
    const auto path = mir::CookedAssetsDir();
    REQUIRE(path == mir::ExecutableDir() / "cooked");
}
