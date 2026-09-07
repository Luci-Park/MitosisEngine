/**
 * @file InputSnapshot.tests.cpp
 * @author Rahul Nair
 * @brief Tests for window/InputSnapshot - specifically the Key::Unknown
 *        bounds-checking regression (a default/hand-edited-JSON InputBinding
 *        carries Key::Unknown == -1, which used to wrap to SIZE_MAX).
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include <window/InputSnapshot.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("IsDown(Key::Unknown) is false, not an out-of-bounds read", "[window][input]")
{
    mir::RawInputSnapshot snapshot;
    REQUIRE_NOTHROW(snapshot.IsDown(mir::Key::Unknown));
    REQUIRE_FALSE(snapshot.IsDown(mir::Key::Unknown));
}

TEST_CASE("IsDown(Key) reflects the keys array for an in-range key", "[window][input]")
{
    mir::RawInputSnapshot snapshot;
    snapshot.keys[static_cast<size_t>(mir::Key::Space)] = true;

    REQUIRE(snapshot.IsDown(mir::Key::Space));
    REQUIRE_FALSE(snapshot.IsDown(mir::Key::A));
}

TEST_CASE("IsDown(MouseButton) is bounds-checked too", "[window][input]")
{
    mir::RawInputSnapshot snapshot;
    snapshot.mouseButtons[static_cast<size_t>(mir::MouseButton::Left)] = true;

    REQUIRE(snapshot.IsDown(mir::MouseButton::Left));
    REQUIRE_FALSE(snapshot.IsDown(mir::MouseButton::Right));
}
