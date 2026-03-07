/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include <KryneEngine/Core/Window/Input/Enums.hpp>
#include <imgui.h>

namespace KryneEngine
{
    class Window;
}

namespace KryneEngine::Modules::ImGui
{
    class Input
    {
    public:
        explicit Input(Window* _window);

        void Shutdown(Window* _window) const;

    private:
        u32 m_keyCallbackId;
        u32 m_textCallbackId;
        u32 m_cursorPosCallbackId;
        u32 m_mouseBtnCallbackId;
        u32 m_scrollEventCallbackId;
        u32 m_windowFocusCallbackId;
        u32 m_dpiChangeCallbackId;

        static void ApplyModifiers(KeyInputModifiers _modifiers);

        [[nodiscard]] static ImGuiKey ToImGuiKey(InputKeys _key);
        [[nodiscard]] static ImGuiMouseButton ToImGuiMouseButton(MouseInputButton _mouseButton);
    };
} // namespace KryneEngine