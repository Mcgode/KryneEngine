/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include <KryneEngine/Core/Window/Input/Enums.hpp>
#include <KryneEngine/Core/Window/Input/InputConsumer.hpp>
#include <imgui.h>

namespace KryneEngine
{
    class InputManager;
}

namespace KryneEngine::Modules::ImGui
{
    /**
     * @brief Bridges the engine @ref InputManager event queue into Dear ImGui's `ImGuiIO`.
     *
     * @details A high-priority @ref InputConsumer: every relevant event is unconditionally forwarded
     * to ImGui (it needs the raw stream to compute hover/capture state for the next frame), and
     * #HandleEvent reports the event as consumed whenever `io.WantCaptureMouse` / `WantCaptureKeyboard`
     * is set, so lower-priority consumers (gameplay input) don't also see it. Focus / DPI /
     * window-lifecycle events are handled by @ref ViewportBackend; this only covers keyboard / text /
     * mouse. Mouse positions are reported in desktop space when `ImGuiConfigFlags_ViewportsEnable` is set
     * (offset by the originating window's position).
     */
    class Input final : public InputConsumer
    {
    public:
        Input();

        void Shutdown();

        bool HandleEvent(const InputEvent& _event) override;

    private:
        static void ApplyModifiers(KeyInputModifiers _modifiers);

        [[nodiscard]] static ImGuiKey ToImGuiKey(InputKeys _key);
        [[nodiscard]] static ImGuiMouseButton ToImGuiMouseButton(MouseInputButton _mouseButton);
    };
} // namespace KryneEngine
