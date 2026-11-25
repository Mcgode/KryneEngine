/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include <atomic>
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
     *
     * While mouse input is disabled (see #SetMouseEnabled), mouse events skip ImGui entirely and are never consumed,
     * so they reach the lower-priority consumers and the polling state.
     */
    class Input final : public InputConsumer
    {
    public:
        Input();

        void Shutdown();

        bool HandleEvent(const InputEvent& _event) override;

        /**
         * @brief Turns ImGui's mouse input on or off, e.g. off while the application captures the cursor.
         * @details Disabling releases every ImGui mouse button and moves its mouse off-screen, so nothing stays
         * hovered or pressed. Re-enabling waits for the next mouse move to place the mouse again.
         * Call on the thread that runs `ImGui::NewFrame`, with this module's ImGui context current.
         */
        void SetMouseEnabled(bool _enabled);

    private:
        /// Read by #HandleEvent, which runs during `InputManager::Update`, possibly on another thread.
        std::atomic<bool> m_mouseEnabled = true;

        static void ApplyModifiers(KeyInputModifiers _modifiers);

        [[nodiscard]] static ImGuiKey ToImGuiKey(InputKeys _key);
        [[nodiscard]] static ImGuiMouseButton ToImGuiMouseButton(MouseInputButton _mouseButton);
    };
} // namespace KryneEngine
