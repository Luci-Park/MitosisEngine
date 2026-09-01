/**
 * @file StorageInfo.h
 * @author Sumin Park
 * @brief Can set per-component storage policy trait
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once

namespace mts
{
    enum class StorageKind
    {
        Table,
        SparseSet,
    };

    // Table is default, opt in for SparseSet
    template <typename T>
    struct ComponentStorageInfo
    {
        static constexpr StorageKind kValue = StorageKind::Table;
    };
}

// Set T's storage to sparse
// must always be in the in the same header as the component outside of a namespace
#define MTS_COMPONENT_SPARSE(T)                    \
    template <>                                    \
    struct mts::ComponentStorageInfo<T>            \
    {                                              \
        static constexpr mts::StorageKind kValue = \
            mts::StorageKind::SparseSet;           \
    }
