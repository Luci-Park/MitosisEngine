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
}
