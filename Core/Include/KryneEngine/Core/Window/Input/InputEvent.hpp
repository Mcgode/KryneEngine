/**
 * @file
 * @author Max Godefroy
 * @date 11/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Window/Input/KeyInputEvent.hpp"
#include "KryneEngine/Core/Window/Input/MouseInputEvent.hpp"

namespace KryneEngine
{
    class Window;

    /// @brief Discriminates the payload carried by an @ref InputEvent.
    enum class InputEventType : u8
    {
        Key,
        Text,
        MouseButton,
        MouseMove,
        Scroll,
        WindowFocus,
        WindowResize,
        WindowDpiChange,
        WindowCloseRequest,
    };

    /**
     * @brief A single raw event, as pushed into the @ref InputManager's per-frame queue by the
     * @ref WindowManager.
     *
     * @details Window-lifecycle events (`WindowFocus`/`WindowResize`/`WindowDpiChange`/
     * `WindowCloseRequest`) always update @ref InputManager polling state even when a consumer
     * captures them; the others (raw input) only do so when left unconsumed.
     */
    struct InputEvent
    {
        InputEventType m_type;
        Window* m_window = nullptr;
        union
        {
            KeyInputEvent m_key {};
            u32 m_codepoint;
            MouseInputEvent m_mouseButton;
            float2 m_mousePos;
            float2 m_scroll;
            bool m_focused;
            uint2 m_newSize;
            float2 m_dpiScale;
        };
    };
} // namespace KryneEngine
