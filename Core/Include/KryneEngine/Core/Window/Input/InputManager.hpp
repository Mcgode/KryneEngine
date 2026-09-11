/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Memory/Containers/FlatHashMap.hpp"
#include "KryneEngine/Core/Window/Input/InputAction.hpp"
#include "KryneEngine/Core/Window/Input/InputConsumer.hpp"
#include "KryneEngine/Core/Window/Input/InputEvent.hpp"

namespace KryneEngine
{
    class Window;
    class WindowManager;

    /**
     * @brief Per-application input hub: raw event queue -> per-frame polling state -> action map,
     *        with a priority-ordered @ref InputConsumer stack in between.
     *
     * @details
     * Owned and fed by the @ref WindowManager, which translates the raw GLFW callbacks into engine
     * events and forwards them through the `On*Event` entry points into #m_frameEvents. Call #Update
     * once per frame, right after `WindowManager::PollEvents()`, to drain that queue: each event is
     * offered to the consumer stack (highest priority first; the first to return `true` stops
     * propagation), then folded into the polling state and the action map unless a consumer captured
     * it (window-lifecycle events always fold in regardless).
     *
     * Exactly one instance may exist at a time (enforced by a ctor assert, matching @ref WindowManager).
     * #Get() is backed by that explicitly-owned instance rather than a lazy singleton: it exists because
     * polling gameplay code (`InputManager::Get().IsActionPressed(...)`) is real friction to thread a
     * reference through, not because of any global state GLFW forces here.
     */
    class InputManager
    {
    public:
        explicit InputManager(AllocatorInstance _allocator);
        ~InputManager();

        InputManager(const InputManager&) = delete;
        InputManager& operator=(const InputManager&) = delete;

        [[nodiscard]] static InputManager& Get();

        // --- Fed by the WindowManager's GLFW callbacks (already translated to engine events) ---
        void OnKeyEvent(Window* _window, const KeyInputEvent& _event);
        void OnTextEvent(Window* _window, u32 _codepoint);
        void OnCursorPosEvent(Window* _window, float _posX, float _posY);
        void OnMouseButtonEvent(Window* _window, const MouseInputEvent& _event);
        void OnScrollEvent(Window* _window, float _scrollX, float _scrollY);
        void OnWindowFocusEvent(Window* _window, bool _focused);
        void OnWindowResizeEvent(Window* _window, uint2 _size);
        void OnWindowDpiChangeEvent(Window* _window, float2 _dpiScale);
        void OnWindowCloseRequestEvent(Window* _window);

        /// @brief Drains the frame's raw event queue into per-frame polling state, the consumer stack
        /// and the action map. Call once per frame, right after the window manager's OS pump.
        void Update();

        // --- Polling API (state as of the last #Update) ---
        [[nodiscard]] bool IsKeyPressed(InputKeys _key) const;
        [[nodiscard]] bool WasKeyJustPressed(InputKeys _key) const;
        [[nodiscard]] bool WasKeyJustReleased(InputKeys _key) const;
        [[nodiscard]] KeyInputModifiers GetModifiers() const { return m_modifiers; }

        /// @brief The window that last received a `WindowFocus(true)` event, or `nullptr`.
        [[nodiscard]] Window* GetFocusedWindow() const { return m_focusedWindow; }

        /// @brief `_window` defaults to the focused window.
        [[nodiscard]] float2 GetCursorPosition(Window* _window = nullptr) const;
        [[nodiscard]] float2 GetCursorDelta(Window* _window = nullptr) const;
        [[nodiscard]] float2 GetScrollDelta(Window* _window = nullptr) const;
        [[nodiscard]] bool IsMouseButtonPressed(MouseInputButton _button, Window* _window = nullptr) const;
        [[nodiscard]] bool WasMouseButtonJustPressed(MouseInputButton _button, Window* _window = nullptr) const;
        [[nodiscard]] bool WasMouseButtonJustReleased(MouseInputButton _button, Window* _window = nullptr) const;

        // --- Consumer stack ---
        /// @brief Registers an @ref InputConsumer. Higher `_priority` consumers see events first.
        void PushConsumer(InputConsumer* _consumer, s32 _priority = 0);
        void RemoveConsumer(InputConsumer* _consumer);

        // --- Action map (data-shaped; registration is hardcoded for now) ---
        ActionId RegisterAction(const eastl::string_view& _name, const InputAction& _action);
        [[nodiscard]] bool IsActionPressed(ActionId _action) const;
        [[nodiscard]] bool WasActionJustPressed(ActionId _action) const;
        [[nodiscard]] bool WasActionJustReleased(ActionId _action) const;
        [[nodiscard]] float GetActionValue(ActionId _action) const;

        // --- Keymap cache (physical key <-> current-layout label, e.g. for a rebinding UI) ---
        /// @brief Rebuilds the physical-key <-> label cache from the current OS keyboard layout via
        /// `WindowManager::GetLabel`. GLFW has no layout-change notification, so call this on demand
        /// (e.g. when a settings/rebinding screen opens) rather than every frame.
        void RefreshKeymap();

        /// @brief Current-layout label for a physical key, from the last #RefreshKeymap. Empty if that
        /// key has no printable label, or the keymap has never been refreshed.
        [[nodiscard]] const eastl::string& GetKeyLabel(InputKeys _key) const;

        /// @brief The physical key whose current-layout label matches `_label`, or `InputKeys::Unknown`
        /// if none does. From the last #RefreshKeymap.
        [[nodiscard]] InputKeys GetKeyFromLabel(const StringViewHash& _label) const;

    private:
        struct KeyState
        {
            bool m_pressed: 1 = false;
            bool m_justPressed: 1 = false;
            bool m_justReleased: 1 = false;
        };

        struct WindowInputState
        {
            float2 m_cursorPos {};
            float2 m_cursorDelta {};
            float2 m_scrollDelta {};
            eastl::array<KeyState, static_cast<size_t>(MouseInputButton::Count)> m_mouseButtons {};
        };

        struct ConsumerEntry
        {
            InputConsumer* m_consumer;
            s32 m_priority;
        };

        struct RegisteredAction
        {
            eastl::string m_name;
            InputAction m_action;
            bool m_pressed = false;
            bool m_justPressed = false;
            bool m_justReleased = false;
            float m_value = 0.f;
        };

        static InputManager* s_instance;

        AllocatorInstance m_allocator;

        eastl::vector<InputEvent> m_frameEvents;
        eastl::vector<ConsumerEntry> m_consumers;
        eastl::vector<RegisteredAction> m_actions;

        eastl::array<KeyState, static_cast<size_t>(InputKeys::Count)> m_keyStates {};
        KeyInputModifiers m_modifiers = KeyInputModifiers::None;
        Window* m_focusedWindow = nullptr;

        eastl::vector_map<Window*, WindowInputState> m_windowStates;

        eastl::array<eastl::string, static_cast<size_t>(InputKeys::Count)> m_keyLabels {};
        FlatHashMap<StringViewHash, InputKeys> m_labelToKey;

        [[nodiscard]] WindowInputState& GetOrCreateWindowState(Window* _window);
        [[nodiscard]] const WindowInputState* FindWindowState(Window* _window) const;

        /// @brief Applies a raw event to the polling state.
        void FoldEventIntoState(const InputEvent& _event);

        void UpdateActions();
    };
} // namespace KryneEngine
