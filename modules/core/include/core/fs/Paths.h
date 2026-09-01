/**
 * @file Paths.h
 * @author Sumin Park
 * @brief Executable-relative asset paths.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <filesystem>
#include <string_view>

namespace mts
{
    const std::filesystem::path &ExecutableDir();

    std::filesystem::path ShaderPath(std::string_view name);

    std::filesystem::path CookedAssetsDir();
}
