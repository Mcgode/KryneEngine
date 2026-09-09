/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#include "KryneEngine/Core/Window/Window.hpp"

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#   define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#   define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#   define GLFW_EXPOSE_NATIVE_X11
#   define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

namespace KryneEngine
{
    Window::Window(GLFWwindow* _glfwWindow, const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_glfwWindow(_glfwWindow)
    {}

    Window::~Window() = default;

    NativeWindowHandle Window::GetNativeHandle() const
    {
        using Kind = NativeWindowHandle::Kind;
#if defined(_WIN32)
        return { Kind::Win32, static_cast<void*>(glfwGetWin32Window(m_glfwWindow)), nullptr };
#elif defined(__APPLE__)
        return { Kind::Cocoa, static_cast<void*>(glfwGetCocoaWindow(m_glfwWindow)), nullptr };
#elif defined(__linux__)
        if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        {
            return {
                Kind::Wayland,
                static_cast<void*>(glfwGetWaylandWindow(m_glfwWindow)),
                static_cast<void*>(glfwGetWaylandDisplay()),
            };
        }
        return {
            Kind::Xlib,
            reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(m_glfwWindow))),
            static_cast<void*>(glfwGetX11Display()),
        };
#else
        return {};
#endif
    }

    uint2 Window::GetFramebufferSize() const
    {
        int width, height;
        glfwGetFramebufferSize(m_glfwWindow, &width, &height);
        return { width, height };
    }

    float2 Window::GetDpiScale() const
    {
        float2 result;
        glfwGetWindowContentScale(m_glfwWindow, &result.x, &result.y);
        return result;
    }
}
