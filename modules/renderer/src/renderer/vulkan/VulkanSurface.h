/**
 * @file VulkanSurface.h
 * @author Sumin Park
 * @brief Surface interface for vulkan
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <volk.h>
#include <window/Window.h>

namespace mts::vk
{
    // extensions per backend
    const char *PlatformSurfaceExtension(WindowBackend backend);

    // surface per backend
    VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle &handle);
}