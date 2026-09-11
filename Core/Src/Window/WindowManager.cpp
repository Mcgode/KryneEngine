/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#include "KryneEngine/Core/Window/WindowManager.hpp"

#include <EASTL/algorithm.h>
#include <GLFW/glfw3.h>

#include "GLFW/Input/KeyInputEvent.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Core/Window/Input/InputManager.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

namespace KryneEngine
{
    namespace
    {
        Window* ResolveWindow(GLFWwindow* _glfwWindow)
        {
            return static_cast<Window*>(glfwGetWindowUserPointer(_glfwWindow));
        }
    }

    WindowManager* WindowManager::s_instance = nullptr;

    WindowManager::WindowManager(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_windows(_allocator)
        , m_monitors(_allocator)
    {
        KE_ZoneScopedFunction("WindowManager::WindowManager");

        KE_ASSERT_FATAL_MSG(s_instance == nullptr, "Only one WindowManager may exist at a time");
        s_instance = this;

        {
            KE_ZoneScoped("GLFW init");
            glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
            glfwInit();
        }

        glfwSetMonitorCallback(MonitorCallback);
        RefreshMonitors();

        m_inputManager = m_allocator.New<InputManager>(m_allocator);
    }

    WindowManager::~WindowManager()
    {
        while (!m_windows.empty())
        {
            DestroyWindow(m_windows.back());
        }

        m_allocator.Delete(m_inputManager);

        glfwSetMonitorCallback(nullptr);
        glfwTerminate();

        KE_ASSERT(s_instance == this);
        s_instance = nullptr;
    }

    Window* WindowManager::SpawnWindow(
        const eastl::string_view& _title,
        const GraphicsCommon::DisplayOptions& _displayOptions,
        const bool _initiallyVisible)
    {
        KE_ZoneScopedFunction("WindowManager::SpawnWindow");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, _displayOptions.m_resizableWindow);
        glfwWindowHint(GLFW_DECORATED, _displayOptions.m_decorated);
        glfwWindowHint(GLFW_VISIBLE, _initiallyVisible);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, _initiallyVisible);

        GLFWwindow* glfwWindow;
        {
            KE_ZoneScoped("GLFW window creation");
            glfwWindow = glfwCreateWindow(
                _displayOptions.m_width,
                _displayOptions.m_height,
                _title.data(),
                nullptr,
                nullptr);
        }
        KE_ASSERT_FATAL_MSG(glfwWindow != nullptr, "glfwCreateWindow failed");

        auto* window = m_allocator.New<Window>(glfwWindow, m_allocator);
        window->m_lastFramebufferSize = window->GetFramebufferSize();

        glfwSetWindowUserPointer(glfwWindow, window);

        glfwSetKeyCallback(glfwWindow, KeyCallback);
        glfwSetCharCallback(glfwWindow, TextCallback);
        glfwSetCursorPosCallback(glfwWindow, CursorPosCallback);
        glfwSetMouseButtonCallback(glfwWindow, MouseButtonCallback);
        glfwSetScrollCallback(glfwWindow, ScrollCallback);
        glfwSetCursorEnterCallback(glfwWindow, CursorEnterCallback);
        glfwSetWindowFocusCallback(glfwWindow, WindowFocusCallback);
        glfwSetWindowPosCallback(glfwWindow, WindowPosCallback);
        glfwSetWindowSizeCallback(glfwWindow, WindowSizeCallback);
        glfwSetWindowCloseCallback(glfwWindow, WindowCloseCallback);
        glfwSetWindowContentScaleCallback(glfwWindow, ContentScaleCallback);
        glfwSetFramebufferSizeCallback(glfwWindow, FramebufferSizeCallback);

        m_windows.push_back(window);
        return window;
    }

    void WindowManager::DestroyWindow(Window* _window)
    {
        KE_ZoneScopedFunction("WindowManager::DestroyWindow");

        m_inputManager->NotifyWindowClosed(_window);

        const auto it = eastl::find(m_windows.begin(), m_windows.end(), _window);
        if (it == m_windows.end())
            return;
        m_windows.erase(it);

        glfwDestroyWindow(_window->m_glfwWindow);
        m_allocator.Delete(_window);
    }

    void WindowManager::PollEvents()
    {
        KE_ZoneScopedFunction("WindowManager::PollEvents");

        for (Window* window : m_windows)
            window->m_resizePending = false;

        glfwPollEvents();
    }

    bool WindowManager::ShouldClose(const Window* _window) const
    {
        return _window == nullptr || glfwWindowShouldClose(_window->m_glfwWindow);
    }

    bool WindowManager::AllWindowsClosed() const
    {
        for (const Window* window : m_windows)
        {
            if (!ShouldClose(window))
                return false;
        }
        return true;
    }

    bool WindowManager::ConsumeResizeFlag(Window* _window)
    {
        if (_window == nullptr || !_window->m_resizePending)
            return false;
        _window->m_resizePending = false;
        return true;
    }

    const char* WindowManager::GetLabel(const InputKeys _key)
    {
        const s32 glfwKey = GLFW::FromInputPhysicalKeys(_key);
        if (glfwKey == GLFW_KEY_UNKNOWN)
            return nullptr;

        const s32 scancode = glfwGetKeyScancode(glfwKey);
        return glfwGetKeyName(glfwKey, scancode);
    }

    void WindowManager::RefreshMonitors()
    {
        KE_ZoneScopedFunction("WindowManager::RefreshMonitors");

        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);

        m_monitors.clear();
        m_monitors.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; i++)
        {
            GLFWmonitor* monitor = monitors[i];

            MonitorInfo info {};
            glfwGetMonitorPos(monitor, &info.m_position.x, &info.m_position.y);

            if (const GLFWvidmode* mode = glfwGetVideoMode(monitor))
                info.m_size = { static_cast<u32>(mode->width), static_cast<u32>(mode->height) };

            int wx, wy, ww, wh;
            glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
            info.m_workAreaPosition = { wx, wy };
            info.m_workAreaSize = { static_cast<u32>(ww), static_cast<u32>(wh) };

            float sx, sy;
            glfwGetMonitorContentScale(monitor, &sx, &sy);
            info.m_dpiScale = sx;

            m_monitors.push_back(info);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // GLFW callbacks
    // ---------------------------------------------------------------------------------------------

    void WindowManager::KeyCallback(GLFWwindow* _window, s32 _key, s32 _scancode, s32 _action, s32 _mods)
    {
        KE_ZoneScopedFunction("WindowManager::KeyCallback");

        s_instance->m_inputManager->OnKeyEvent(ResolveWindow(_window), KeyInputEvent {
            .m_physicalKey = GLFW::ToInputPhysicalKeys(_key),
            .m_customCode = _scancode,
            .m_action = GLFW::ToInputEventAction(_action),
            .m_modifiers = GLFW::ToInputEventModifiers(_mods),
        });
    }

    void WindowManager::TextCallback(GLFWwindow* _window, u32 _codepoint)
    {
        KE_ZoneScopedFunction("WindowManager::TextCallback");

        s_instance->m_inputManager->OnTextEvent(ResolveWindow(_window), _codepoint);
    }

    void WindowManager::CursorPosCallback(GLFWwindow* _window, double _posX, double _posY)
    {
        KE_ZoneScopedFunction("WindowManager::CursorPosCallback");

        s_instance->m_inputManager->OnCursorPosEvent(
            ResolveWindow(_window), static_cast<float>(_posX), static_cast<float>(_posY));
    }

    void WindowManager::MouseButtonCallback(GLFWwindow* _window, s32 _button, s32 _action, s32 _mods)
    {
        KE_ZoneScopedFunction("WindowManager::MouseButtonCallback");

        s_instance->m_inputManager->OnMouseButtonEvent(ResolveWindow(_window), MouseInputEvent {
            .m_mouseButton = GLFW::ToMouseInputButton(_button),
            .m_action = GLFW::ToInputEventAction(_action),
            .m_modifiers = GLFW::ToInputEventModifiers(_mods),
        });
    }

    void WindowManager::ScrollCallback(GLFWwindow* _window, double _scrollX, double _scrollY)
    {
        KE_ZoneScopedFunction("WindowManager::ScrollCallback");

        s_instance->m_inputManager->OnScrollEvent(
            ResolveWindow(_window), static_cast<float>(_scrollX), static_cast<float>(_scrollY));
    }

    void WindowManager::CursorEnterCallback(GLFWwindow* _window, s32 _entered)
    {
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onCursorEnter)
            fn(ResolveWindow(_window), _entered != 0);
    }

    void WindowManager::WindowFocusCallback(GLFWwindow* _window, s32 _focused)
    {
        Window* window = ResolveWindow(_window);
        s_instance->m_inputManager->OnWindowFocusEvent(window, _focused != 0);
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onFocus)
            fn(window, _focused != 0);
    }

    void WindowManager::WindowPosCallback(GLFWwindow* _window, s32 _x, s32 _y)
    {
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onMove)
            fn(ResolveWindow(_window), int2 { _x, _y });
    }

    void WindowManager::WindowSizeCallback(GLFWwindow* _window, s32 _width, s32 _height)
    {
        Window* window = ResolveWindow(_window);
        const uint2 size { static_cast<u32>(_width), static_cast<u32>(_height) };
        s_instance->m_inputManager->OnWindowResizeEvent(window, size);
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onResize)
            fn(window, size);
    }

    void WindowManager::WindowCloseCallback(GLFWwindow* _window)
    {
        Window* window = ResolveWindow(_window);
        s_instance->m_inputManager->OnWindowCloseRequestEvent(window);
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onCloseRequest)
            fn(window);
    }

    void WindowManager::ContentScaleCallback(GLFWwindow* _window, float _xScale, float _yScale)
    {
        Window* window = ResolveWindow(_window);
        const float2 dpiScale { _xScale, _yScale };
        s_instance->m_inputManager->OnWindowDpiChangeEvent(window, dpiScale);
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onDpiChange)
            fn(window, dpiScale);
    }

    void WindowManager::FramebufferSizeCallback(GLFWwindow* _window, s32 _width, s32 _height)
    {
        Window* window = ResolveWindow(_window);
        const uint2 size { static_cast<u32>(_width), static_cast<u32>(_height) };
        if (window->m_lastFramebufferSize != size)
        {
            window->m_resizePending = true;
            window->m_lastFramebufferSize = size;
        }
    }

    void WindowManager::MonitorCallback(GLFWmonitor*, s32)
    {
        s_instance->RefreshMonitors();
        if (const auto& fn = s_instance->m_windowEventCallbacks.m_onMonitorsChanged)
            fn();
    }
} // namespace KryneEngine
