/**
 * @file Paths.cpp
 * @author Sumin Park
 * @brief Executable-relative asset paths.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "core/fs/Paths.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace mts
{
    namespace
    {
        std::filesystem::path QueryExecutableDir()
        {
#if defined(_WIN32)
            wchar_t buffer[MAX_PATH]{};
            const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if (length == 0)
                return std::filesystem::current_path();
            return std::filesystem::path(buffer, buffer + length).parent_path();
#elif defined(__linux__)
            char buffer[4096]{};
            const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
            if (length <= 0)
                return std::filesystem::current_path();
            return std::filesystem::path(buffer, buffer + length).parent_path();
#else
            return std::filesystem::current_path();
#endif
        }
    }

    const std::filesystem::path &ExecutableDir()
    {
        static const std::filesystem::path dir = QueryExecutableDir();
        return dir;
    }

    std::filesystem::path ShaderPath(std::string_view name)
    {
        return ExecutableDir() / "shaders" / name;
    }

    std::filesystem::path CookedAssetsDir()
    {
        return ExecutableDir() / "cooked";
    }
}
