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
    class InputManager;
}

namespace KryneEngine::Modules::ImGui
{
    /**
     * @brief Bridges the engine @ref InputManager events into Dear ImGui's `ImGuiIO`.
     *
     * @details Focus / DPI / window-lifecycle events are handled by @ref ViewportBackend; this only
     * covers keyboard / text / mouse. Mouse positions are reported in desktop space when
     * `ImGuiConfigFlags_ViewportsEnable` is set (offset by the originating window's position).
     */
    class Input
    {
    public:
        explicit Input(InputManager& _inputManager);

        void Shutdown(InputManager& _inputManager) const;

    private:
        u32 m_keyCallbackId;
        u32 m_textCallbackId;
        u32 m_cursorPosCallbackId;
        u32 m_mouseBtnCallbackId;
        u32 m_scrollEventCallbackId;

        static void ApplyModifiers(KeyInputModifiers _modifiers);

        [[nodiscard]] static ImGuiKey ToImGuiKey(InputKeys _key);
        [[nodiscard]] static ImGuiMouseButton ToImGuiMouseButton(MouseInputButton _mouseButton);
    };
} // namespace KryneEngine
