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

// X11/Xlib.h (pulled in transitively via VK_USE_PLATFORM_XLIB_KHR) defines a bunch of
// very common identifiers as macros (None, Bool, True, False, Status, Success, ...).
// These clash with enum values and identifiers used throughout the engine, so undefine
// them here right after Xlib.h has had a chance to declare its own API.
#if defined(VK_USE_PLATFORM_XLIB_KHR)
#   undef None
#   undef Bool
#   undef True
#   undef False
#   undef Status
#   undef Success
#   undef Always
#   undef Button1
#   undef Button2
#   undef Button3
#   undef Button4
#   undef Button5
#   undef Button6
#endif