/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#pragma once

#include <EASTL/functional.h>
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

struct GLFWwindow;

namespace KryneEngine
{
    class InputManager;
    class Window;

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

        [[nodiscard]] u32 RegisterWindowFocusEventCallback(eastl::function<void(bool)>&& _callback);
        void UnregisterWindowFocusEventCallback(u32 _id);

        [[nodiscard]] u32 RegisterDpiChangeEventCallback(eastl::function<void(const float2&)>&& _callback);
        void UnregisterDpiChangeEventCallback(u32 _id);

    private:
        static WindowManager* s_instance;

        AllocatorInstance m_allocator;
        InputManager* m_inputManager = nullptr;

        eastl::vector<Window*> m_windows;

        LightweightMutex m_callbackMutex;

        eastl::vector_map<u32, eastl::function<void(bool)>> m_windowFocusEventListeners;
        u32 m_windowFocusEventCounter = 0;

        eastl::vector_map<u32, eastl::function<void(const float2&)>> m_dpiChangeEventListeners;
        u32 m_dpiChangeEventCounter = 0;

        static void KeyCallback(GLFWwindow* _window, s32 _key, s32 _scancode, s32 _action, s32 _mods);
        static void TextCallback(GLFWwindow* _window, u32 _codepoint);
        static void CursorPosCallback(GLFWwindow* _window, double _posX, double _posY);
        static void MouseButtonCallback(GLFWwindow* _window, s32 _button, s32 _action, s32 _mods);
        static void ScrollCallback(GLFWwindow* _window, double _scrollX, double _scrollY);
        static void WindowFocusCallback(GLFWwindow* _window, s32 _focused);
        static void ContentScaleCallback(GLFWwindow* _window, float _xScale, float _yScale);
        static void FramebufferSizeCallback(GLFWwindow* _window, s32 _width, s32 _height);
    };
} // namespace KryneEngine
