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

#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Core/Window/Input/InputManager.hpp"

namespace KryneEngine
{
    Window::Window(
        const eastl::string_view& _title,
        const GraphicsCommon::DisplayOptions& _displayOptions,
        const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_windowFocusEventListeners(_allocator)
        , m_dpiChangeEventListeners(_allocator)
    {
        KE_ZoneScopedFunction("Window init");

        {
            KE_ZoneScoped("GLFW init");
            glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
            glfwInit();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        const auto& displayInfo = _displayOptions;

        glfwWindowHint(GLFW_RESIZABLE, displayInfo.m_resizableWindow);

        {
            KE_ZoneScoped("GLFW window creation");

            m_glfwWindow = glfwCreateWindow(displayInfo.m_width,
                                            displayInfo.m_height,
                                            _title.data(),
                                            nullptr,
                                            nullptr);
        }
        glfwSetWindowUserPointer(m_glfwWindow, this);

        {
            KE_ZoneScoped("Input management init");

            m_inputManager = m_allocator.New<InputManager>(this, _allocator);

            glfwSetWindowFocusCallback(m_glfwWindow, WindowFocusCallback);
            glfwSetWindowContentScaleCallback(m_glfwWindow, DpiChangeCallback);
            glfwSetFramebufferSizeCallback(m_glfwWindow, ResizeCallback);
        }

        m_previousFramebufferSize = GetFramebufferSize();
    }

    Window::~Window()
    {
        m_allocator.Delete(m_inputManager);

        glfwDestroyWindow(m_glfwWindow);
        glfwTerminate();
    }

    bool Window::WaitForEvents()
    {
        KE_ZoneScopedFunction("Window::WaitForEvents");

        m_resizedThisFrame = false;
        glfwPollEvents();

        return !glfwWindowShouldClose(m_glfwWindow);
    }

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

    u32 Window::RegisterWindowFocusEventCallback(eastl::function<void(bool)>&& _callback)
    {
        const auto lock = m_callbackMutex.AutoLock();

        const u32 id = m_windowFocusEventCounter++;
        m_windowFocusEventListeners.emplace(id, _callback);
        return id;
    }

    void Window::UnregisterWindowFocusEventCallback(u32 _id)
    {
        const auto lock = m_callbackMutex.AutoLock();
        m_windowFocusEventListeners.erase(_id);
    }

    u32 Window::RegisterDpiChangeEventCallback(eastl::function<void(const float2&)>&& _callback)
    {
        const auto lock = m_callbackMutex.AutoLock();
        const u32 id = m_dpiChangeEventCounter++;
        m_dpiChangeEventListeners.emplace(id, _callback);
        return id;
    }

    void Window::UnregisterDpiChangeEventCallback(u32 _id)
    {
        const auto lock = m_callbackMutex.AutoLock();
        m_dpiChangeEventListeners.erase(_id);
    }

    void Window::WindowFocusCallback(GLFWwindow* _window, s32 _focused)
    {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(_window));

        const auto lock = window->m_callbackMutex.AutoLock();

        for (const auto& pair : window->m_windowFocusEventListeners)
        {
            pair.second(_focused);
        }
    }

    void Window::DpiChangeCallback(GLFWwindow* _window, float _xScale, float _yScale)
    {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(_window));

        const auto lock = window->m_callbackMutex.AutoLock();
        for (const auto& pair : window->m_dpiChangeEventListeners)
        {
            pair.second({ _xScale, _yScale });
        }
    }

    void Window::ResizeCallback(GLFWwindow* _window, int _width, int _height)
    {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(_window));
        const uint2 currentFramebufferSize = { _width, _height };
        if (window->m_previousFramebufferSize != currentFramebufferSize)
        {
            window->m_resizedSwapChain = false;
            window->m_resizedThisFrame = true;
        }
        window->m_previousFramebufferSize = currentFramebufferSize;
    }
}

