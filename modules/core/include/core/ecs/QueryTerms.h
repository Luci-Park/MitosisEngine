/**
 * @file QueryTerms.h
 * @author Sumin Park
 * @brief Query filter terms and the type-erased handle World stores them behind
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include "Entity.h"

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace mts
{
    template <typename... Ts>
    class Query;

    // distingushed by type so can be empty structs

    template <typename... Es>
    struct With
    {
    };

    template <typename... Es>
    struct Without
    {
    };

    template <typename... Es>
    struct Or
    {
    };

    namespace detail
    {
        // const stripping
        template <typename T>
        using Bare = std::remove_const_t<T>;

        // groups the data terms, distinguishes it from filters
        template <typename... Ts>
        struct TypeList
        {
        };

        class IQuery
        {
        public:
            virtual ~IQuery() = default;
        };

        inline uint32_t NextQueryKey()
        {
            static std::atomic<uint32_t> counter{0};
            return counter.fetch_add(1, std::memory_order_relaxed);
        }

        // distinct id per component + filter combination
        template <typename... Key>
        uint32_t QueryKeyOf()
        {
            static const uint32_t id = NextQueryKey();
            return id;
        }
    }
}
