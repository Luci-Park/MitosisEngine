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

    // filter terms: type-only carriers, passed as empty instances so the
    // pack can be deduced from a call argument list
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

        // groups the data terms so a query key can never be mistaken for a filter pack
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

        // distinct integer per Component + Filter<Component> combinations
        template <typename... Key>
        uint32_t QueryKeyOf()
        {
            static const uint32_t id = NextQueryKey();
            return id;
        }

        // a sparse filter member is erased at Query construction (filters are not
        // Query's template parameters), so the Has() call is bound through a thunk
        struct SparseFilterCheck
        {
            bool (*has)(const void *storage, Entity entity);
            const void *storage;
            bool wantPresent; // With -> must be present, Without -> must be absent
        };
    }
}
