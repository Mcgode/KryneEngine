/**
 * @file
 * @author Max Godefroy
 * @date 06/09/2026.
 */

#pragma once

#include "Graphics/Vulkan/VkHeaders.hpp"
#include "KryneEngine/Core/Window/NativeWindowHandle.hpp"

namespace KryneEngine::VkSurfacePlatform
{
    /**
     * @brief Creates a `VkSurfaceKHR` from the platform's native window handle.
     *
     * @details
     * Wraps the per-platform `vkCreate*SurfaceKHR` entry point (`VK_KHR_win32_surface`,
     * `VK_EXT_metal_surface`, `VK_KHR_xlib_surface` / `VK_KHR_wayland_surface`), so that
     * `VkSurface` and the rest of the Vulkan backend never depend on the windowing backend.
     *
     * The matching instance extension must already be enabled (see
     * `VkGraphicsContext::RetrieveRequiredExtensionNames`).
     */
    VkSurfaceKHR Create(VkInstance _instance, const NativeWindowHandle& _nativeWindow);
}
