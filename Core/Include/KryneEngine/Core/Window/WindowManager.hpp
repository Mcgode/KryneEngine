/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#pragma once

#include <EASTL/functional.h>
#include <EASTL/span.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

struct GLFWwindow;
struct GLFWmonitor;

namespace KryneEngine
{
    class InputManager;
    class Window;

    /// @brief A connected display, as reported by the OS. See @ref WindowManager::GetMonitors.
    struct MonitorInfo
    {
        int2 m_position {};        //< Virtual-screen position of the monitor's top-left corner.
        uint2 m_size {};           //< Full resolution, in screen coordinates.
        int2 m_workAreaPosition {};//< Usable-area top-left (excludes menu bars / docks).
        uint2 m_workAreaSize {};
        float m_dpiScale = 1.f;
    };

    /**
     * @brief Per-window OS-event callbacks. At most one set may be registered on a @ref WindowManager
     *        (see @ref WindowManager::SetWindowEventCallbacks). Any member may be left empty.
     *
     * @details Introduced for the Dear ImGui multi-viewport backend; a future input rework (Phase 4)
     * will fold these into the raw event queue.
     */
    struct WindowEventCallbacks
    {
        eastl::function<void(Window* _window, bool _focused)> m_onFocus;
        eastl::function<void(Window* _window, int2 _position)> m_onMove;
        eastl::function<void(Window* _window, uint2 _size)> m_onResize;
        eastl::function<void(Window* _window)> m_onCloseRequest;
        eastl::function<void(Window* _window, bool _entered)> m_onCursorEnter;
        eastl::function<void(Window* _window, float2 _dpiScale)> m_onDpiChange;
        eastl::function<void()> m_onMonitorsChanged;
    };

    /**
     * @brief Owns GLFW's process-global state, the window registry and the per-frame OS message pump.
     *
     * @details
     * Exactly one `WindowManager` may exist at a time (enforced by a ctor assert). It also owns the
     * per-application @ref InputManager and routes every window's raw GLFW callbacks into it. GLFW is
     * confined to `WindowManager.cpp`, so a future SDL3 / native backend is a backend-only change.
     *
     * The `Window*` returned by @ref CreateWindow is the handle used with the rest of this API; it stays
     * valid until the matching @ref DestroyWindow (or the manager's destruction).
     */
    class WindowManager
    {
    public:
        explicit WindowManager(AllocatorInstance _allocator);
        ~WindowManager();

        WindowManager(const WindowManager&) = delete;
        WindowManager& operator=(const WindowManager&) = delete;

        [[nodiscard]] Window* CreateWindow(
            const eastl::string_view& _title,
            const GraphicsCommon::DisplayOptions& _displayOptions,
            bool _initiallyVisible = true);
        void DestroyWindow(Window* _window);

        [[nodiscard]] eastl::span<Window* const> GetWindows() const { return m_windows; }

        /// @brief Pumps the OS message queue once. Call once per application update loop iteration.
        void PollEvents();

        [[nodiscard]] bool ShouldClose(const Window* _window) const;

        /// @brief `true` when every live window has been asked to close (or there are no windows).
        [[nodiscard]] bool AllWindowsClosed() const;

        [[nodiscard]] InputManager& GetInput() const { return *m_inputManager; }

        /**
         * @brief Returns `true` once after the window's framebuffer has been resized, then clears the flag.
         *
         * @details Replaces the old `Window::ShouldResizeSwapChain` / `NotifySwapChainResized` pair.
         * Typical use: `if (wm.ConsumeResizeFlag(w)) gc.ResizeSwapChain(sc, w->GetFramebufferSize());`
         */
        bool ConsumeResizeFlag(Window* _window);

        /// @brief Registers the (single) set of per-window OS-event callbacks. Pass `{}` to clear.
        void SetWindowEventCallbacks(WindowEventCallbacks _callbacks) { m_windowEventCallbacks = eastl::move(_callbacks); }

        [[nodiscard]] eastl::span<const MonitorInfo> GetMonitors() const { return m_monitors; }
        void RefreshMonitors();

    private:
        static WindowManager* s_instance;

        AllocatorInstance m_allocator;
        InputManager* m_inputManager = nullptr;

        eastl::vector<Window*> m_windows;
        eastl::vector<MonitorInfo> m_monitors;

        WindowEventCallbacks m_windowEventCallbacks;

        static void KeyCallback(GLFWwindow* _window, s32 _key, s32 _scancode, s32 _action, s32 _mods);
        static void TextCallback(GLFWwindow* _window, u32 _codepoint);
        static void CursorPosCallback(GLFWwindow* _window, double _posX, double _posY);
        static void MouseButtonCallback(GLFWwindow* _window, s32 _button, s32 _action, s32 _mods);
        static void ScrollCallback(GLFWwindow* _window, double _scrollX, double _scrollY);
        static void CursorEnterCallback(GLFWwindow* _window, s32 _entered);
        static void WindowFocusCallback(GLFWwindow* _window, s32 _focused);
        static void WindowPosCallback(GLFWwindow* _window, s32 _x, s32 _y);
        static void WindowSizeCallback(GLFWwindow* _window, s32 _width, s32 _height);
        static void WindowCloseCallback(GLFWwindow* _window);
        static void ContentScaleCallback(GLFWwindow* _window, float _xScale, float _yScale);
        static void FramebufferSizeCallback(GLFWwindow* _window, s32 _width, s32 _height);
        static void MonitorCallback(GLFWmonitor* _monitor, s32 _event);
    };
} // namespace KryneEngine
