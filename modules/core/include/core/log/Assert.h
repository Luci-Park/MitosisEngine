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
#define MIR_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define MIR_DEBUG_BREAK() __builtin_trap()
#else // undefined user case
#define MIR_DEBUG_BREAK() (::std::abort())
#endif

#define MIR_ASSERT_FAIL(cond, ...)                                          \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            ::mir::detail::LogAssert(#cond, ::std::format("" __VA_ARGS__)); \
            MIR_DEBUG_BREAK();                                              \
        }                                                                   \
    } while (0)

// Assert that works on release
#define MIR_CHECK(cond, ...)                                                \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            ::mir::detail::LogAssert(#cond, ::std::format("" __VA_ARGS__)); \
            ::mir::FlushLog();                                              \
            MIR_DEBUG_BREAK();                                              \
            ::std::abort();                                                 \
        }                                                                   \
    } while (0)

#ifdef NDEBUG
#define MIR_ASSERT(cond, ...) ((void)0)
#define MIR_VARIFY(cond, ...) \
    {                         \
        do                    \
        {                     \
            (void)(cond);     \
        } while (0)           \
    }
#else
#define MIR_ASSERT(cond, ...) MIR_ASSERT_FAIL(cond, __VA_ARGS__)
#define MIR_VARIFY(cond, ...) MIR_ASSERT_FAIL(cond, __VA_ARGS__)
#endif