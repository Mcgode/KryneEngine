/**
 * @file
 * @author Max Godefroy
 * @date 06/09/2026.
 */

// The Apple implementation lives in VkSurfacePlatform.mm (needs Objective-C to attach a CAMetalLayer).
#if !defined(__APPLE__)

#include "Graphics/Vulkan/VkSurfacePlatform.hpp"

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"

#if defined(_WIN32)
#   include "KryneEngine/Core/Platform/Windows.h"
#endif
// Linux: <vulkan/vulkan_xlib.h> pulls in <X11/Xlib.h>; <vulkan/vulkan_wayland.h>
// forward-declares the wl_* structs — no extra includes needed here.

namespace KryneEngine::VkSurfacePlatform
{
    VkSurfaceKHR Create(const VkInstance _instance, const NativeWindowHandle& _nativeWindow)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        switch (_nativeWindow.m_kind)
        {
#if defined(_WIN32)
        case NativeWindowHandle::Kind::Win32:
        {
            const VkWin32SurfaceCreateInfoKHR createInfo {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = GetModuleHandle(nullptr),
                .hwnd = static_cast<HWND>(_nativeWindow.m_windowHandle),
            };
            VkAssert(vkCreateWin32SurfaceKHR(_instance, &createInfo, nullptr, &surface));
            break;
        }
#elif defined(__linux__)
        case NativeWindowHandle::Kind::Wayland:
        {
            const VkWaylandSurfaceCreateInfoKHR createInfo {
                .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                .display = static_cast<wl_display*>(_nativeWindow.m_displayHandle),
                .surface = static_cast<wl_surface*>(_nativeWindow.m_windowHandle),
            };
            VkAssert(vkCreateWaylandSurfaceKHR(_instance, &createInfo, nullptr, &surface));
            break;
        }
        case NativeWindowHandle::Kind::Xlib:
        {
            const VkXlibSurfaceCreateInfoKHR createInfo {
                .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                .dpy = static_cast<Display*>(_nativeWindow.m_displayHandle),
                .window = static_cast<::Window>(reinterpret_cast<uintptr_t>(_nativeWindow.m_windowHandle)),
            };
            VkAssert(vkCreateXlibSurfaceKHR(_instance, &createInfo, nullptr, &surface));
            break;
        }
#endif
        default:
            KE_ERROR("Unsupported native window kind %d for Vulkan surface creation",
                     static_cast<int>(_nativeWindow.m_kind));
        }

        return surface;
    }
}

#endif // !defined(__APPLE__)
