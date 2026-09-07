/**
 * @file ComponentAsserts.h
 * @author Sumin Park
 * @brief Pod check macro
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <type_traits>

namespace mts
{
    // component with no fields
    template <typename T>
    inline constexpr bool kIsTagComponent = std::is_empty_v<T>;
}

// add to every component type, will check again in storage
// forces pod for memcpy
#define MIR_ASSERT_COMPONENT(T)                                                                       \
    static_assert(std::is_trivially_copyable_v<T>, #T " must be trivially copyable (ECS component)"); \
    static_assert(std::is_standard_layout_v<T>, #T " must be standard layout (ECS component)");       \
    static_assert(std::is_nothrow_move_constructible_v<T>,                                            \
                  #T " must be nothrow move constructible (ECS component)");                          \
    static_assert(std::is_nothrow_move_assignable_v<T>, #T " must be nothrow move assignable (ECS component)")
