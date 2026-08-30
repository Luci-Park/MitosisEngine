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

// add to every component type, will check again in storage
// forces pod for memcpy
#define MTS_ASSERT_COMPONENT(T)                                                                       \
    static_assert(std::is_trivially_copyable_v<T>, #T " must be trivially copyable (ECS component)"); \
    static_assert(std::is_standard_layout_v<T>, #T " must be standard layout (ECS component)");       \
    static_assert(std::is_nothrow_move_constructible_v<T>,                                            \
                  #T " must be nothrow move constructible (ECS component)");                          \
    static_assert(std::is_nothrow_move_assignable_v<T>, #T " must be nothrow move assignable (ECS component)")
