#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace mts
{
    std::optional<std::vector<std::byte>> ReadFileBytes(const std::filesystem::path &path);

    bool WriteFileBytes(const std::filesystem::path &path, std::span<const std::byte> bytes);

    /// First @p count bytes of @p path, or nullopt if it cannot be opened or is
    /// shorter than that. Lets a caller inspect a blob header without paying to
    /// read the whole asset. Silent on failure: probing a missing file is normal.
    std::optional<std::vector<std::byte>> ReadFilePrefix(const std::filesystem::path &path, std::size_t count);
}
