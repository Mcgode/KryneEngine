/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#pragma once

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

#define VK_ENABLE_BETA_EXTENSIONS

// Platform-specific WSI surface extensions. Kept here (rather than as target compile
// definitions) so every Vulkan translation unit sees a consistent set. See
// VkSurfacePlatform for the matching vkCreate*SurfaceKHR calls.
#if defined(_WIN32)
#   define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#   define VK_USE_PLATFORM_METAL_EXT
#elif defined(__linux__)
#   define VK_USE_PLATFORM_XLIB_KHR
#   define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include <vulkan/vulkan.h>