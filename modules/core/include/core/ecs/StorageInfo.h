/**
 * @file StorageInfo.h
 * @author Sumin Park
 * @brief Can set per-component storage policy trait
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once

namespace engine
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
