/**
 * @file Assert.h
 * @author Sumin Park
 * @brief Assertions
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include "Log.h"

#include <cstdlib>
#include <format>

// breakpoint is different per compiler
#if defined(_MSC_VER)
#define MTS_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define MTS_DEBUG_BREAK() __builtin_trap()
#else // undefined user case
#define MTS_DEBUG_BREAK() (::std::abort())
#endif

#define MTS_ASSERT_FAIL(cond, ...)                                          \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            ::mts::detail::LogAssert(#cond, ::std::format("" __VA_ARGS__)); \
            MTS_DEBUG_BREAK();                                              \
        }                                                                   \
    } while (0)

#define MTS_CHECK(cond, ...)                                                \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            ::mts::detail::LogAssert(#cond, ::std::format("" __VA_ARGS__)); \
            ::mts::FlushLog();                                              \
            MTS_DEBUG_BREAK();                                              \
            ::std::abort();                                                 \
        }                                                                   \
    } while (0)

#ifdef NDEBUG
#define MTS_ASSERT(cond, ...) ((void)0)
#define MTS_VARIFY(cond, ...) \
    {                         \
        do                    \
        {                     \
            (void)(cond);     \
        } while (0)           \
    }
#else
#define MTS_ASSERT(cond, ...) MTS_ASSERT_FAIL(cond, __VA_ARGS__)
#define MTS_VARIFY(cond, ...) MTS_ASSERT_FAIL(cond, __VA_ARGS__)
#endif