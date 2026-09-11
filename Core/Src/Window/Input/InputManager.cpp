/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#include "KryneEngine/Core/Window/Input/InputManager.hpp"

#include <EASTL/algorithm.h>

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    namespace
    {
        [[nodiscard]] bool IsWindowLifecycleEvent(InputEventType _type)
        {
            return _type == InputEventType::WindowFocus
                || _type == InputEventType::WindowResize
                || _type == InputEventType::WindowDpiChange
                || _type == InputEventType::WindowCloseRequest;
        }
    }

    InputManager* InputManager::s_instance = nullptr;

    InputManager::InputManager(AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_frameEvents(_allocator)
        , m_consumers(_allocator)
        , m_actions(_allocator)
        , m_windowStates(_allocator)
    {
        KE_ASSERT_FATAL_MSG(s_instance == nullptr, "Only one InputManager may exist at a time");
        s_instance = this;
    }

    InputManager::~InputManager()
    {
        KE_ASSERT(s_instance == this);
        s_instance = nullptr;
    }

    InputManager& InputManager::Get()
    {
        KE_ASSERT_FATAL_MSG(s_instance != nullptr, "No InputManager instance exists");
        return *s_instance;
    }

    void InputManager::OnKeyEvent(Window* _window, const KeyInputEvent& _event)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::Key,
            .m_window = _window,
            .m_key = _event,
        });
    }

    void InputManager::OnTextEvent(Window* _window, u32 _codepoint)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::Text,
            .m_window = _window,
            .m_codepoint = _codepoint,
        });
    }

    void InputManager::OnCursorPosEvent(Window* _window, float _posX, float _posY)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::MouseMove,
            .m_window = _window,
            .m_mousePos = { _posX, _posY },
        });
    }

    void InputManager::OnMouseButtonEvent(Window* _window, const MouseInputEvent& _event)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::MouseButton,
            .m_window = _window,
            .m_mouseButton = _event,
        });
    }

    void InputManager::OnScrollEvent(Window* _window, float _scrollX, float _scrollY)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::Scroll,
            .m_window = _window,
            .m_scroll = { _scrollX, _scrollY },
        });
    }

    void InputManager::OnWindowFocusEvent(Window* _window, bool _focused)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::WindowFocus,
            .m_window = _window,
            .m_focused = _focused,
        });
    }

    void InputManager::OnWindowResizeEvent(Window* _window, uint2 _size)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::WindowResize,
            .m_window = _window,
            .m_newSize = _size,
        });
    }

    void InputManager::OnWindowDpiChangeEvent(Window* _window, float2 _dpiScale)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::WindowDpiChange,
            .m_window = _window,
            .m_dpiScale = _dpiScale,
        });
    }

    void InputManager::OnWindowCloseRequestEvent(Window* _window)
    {
        m_frameEvents.push_back(InputEvent {
            .m_type = InputEventType::WindowCloseRequest,
            .m_window = _window,
        });
    }

    void InputManager::Update()
    {
        KE_ZoneScopedFunction("InputManager::Update");

        for (KeyState& state : m_keyStates)
        {
            state.m_justPressed = false;
            state.m_justReleased = false;
        }
        for (auto& pair : m_windowStates)
        {
            pair.second.m_cursorDelta = {};
            pair.second.m_scrollDelta = {};
            for (KeyState& state : pair.second.m_mouseButtons)
            {
                state.m_justPressed = false;
                state.m_justReleased = false;
            }
        }

        for (const InputEvent& event : m_frameEvents)
        {
            bool consumed = false;
            for (const ConsumerEntry& entry : m_consumers)
            {
                if (entry.m_consumer->HandleEvent(event))
                {
                    consumed = true;
                    break;
                }
            }

            if (!consumed || IsWindowLifecycleEvent(event.m_type))
            {
                FoldEventIntoState(event);
            }
        }

        m_frameEvents.clear();

        UpdateActions();
    }

    void InputManager::FoldEventIntoState(const InputEvent& _event)
    {
        switch (_event.m_type)
        {
        case InputEventType::Key:
        {
            m_modifiers = _event.m_key.m_modifiers;
            if (_event.m_key.m_action == InputActionType::KeepPressing)
                break;

            KeyState& state = m_keyStates[static_cast<size_t>(_event.m_key.m_physicalKey)];
            const bool pressed = _event.m_key.m_action == InputActionType::StartPress;
            if (pressed && !state.m_pressed)
                state.m_justPressed = true;
            else if (!pressed && state.m_pressed)
                state.m_justReleased = true;
            state.m_pressed = pressed;
            break;
        }
        case InputEventType::Text:
            break;
        case InputEventType::MouseMove:
        {
            WindowInputState& state = GetOrCreateWindowState(_event.m_window);
            state.m_cursorDelta = state.m_cursorDelta + (_event.m_mousePos - state.m_cursorPos);
            state.m_cursorPos = _event.m_mousePos;
            break;
        }
        case InputEventType::MouseButton:
        {
            m_modifiers = _event.m_mouseButton.m_modifiers;
            if (_event.m_mouseButton.m_action == InputActionType::KeepPressing)
                break;

            WindowInputState& state = GetOrCreateWindowState(_event.m_window);
            KeyState& buttonState = state.m_mouseButtons[static_cast<size_t>(_event.m_mouseButton.m_mouseButton)];
            const bool pressed = _event.m_mouseButton.m_action == InputActionType::StartPress;
            if (pressed && !buttonState.m_pressed)
                buttonState.m_justPressed = true;
            else if (!pressed && buttonState.m_pressed)
                buttonState.m_justReleased = true;
            buttonState.m_pressed = pressed;
            break;
        }
        case InputEventType::Scroll:
        {
            WindowInputState& state = GetOrCreateWindowState(_event.m_window);
            state.m_scrollDelta = state.m_scrollDelta + _event.m_scroll;
            break;
        }
        case InputEventType::WindowFocus:
            if (_event.m_focused)
                m_focusedWindow = _event.m_window;
            else if (m_focusedWindow == _event.m_window)
                m_focusedWindow = nullptr;
            break;
        case InputEventType::WindowResize:
        case InputEventType::WindowDpiChange:
        case InputEventType::WindowCloseRequest:
            break;
        }
    }

    InputManager::WindowInputState& InputManager::GetOrCreateWindowState(Window* _window)
    {
        auto it = m_windowStates.find(_window);
        if (it == m_windowStates.end())
            it = m_windowStates.emplace(_window, WindowInputState {}).first;
        return it->second;
    }

    const InputManager::WindowInputState* InputManager::FindWindowState(Window* _window) const
    {
        const auto it = m_windowStates.find(_window);
        return it != m_windowStates.end() ? &it->second : nullptr;
    }

    bool InputManager::IsKeyPressed(InputKeys _key) const
    {
        return m_keyStates[static_cast<size_t>(_key)].m_pressed;
    }

    bool InputManager::WasKeyJustPressed(InputKeys _key) const
    {
        return m_keyStates[static_cast<size_t>(_key)].m_justPressed;
    }

    bool InputManager::WasKeyJustReleased(InputKeys _key) const
    {
        return m_keyStates[static_cast<size_t>(_key)].m_justReleased;
    }

    float2 InputManager::GetCursorPosition(Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr ? state->m_cursorPos : float2 {};
    }

    float2 InputManager::GetCursorDelta(Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr ? state->m_cursorDelta : float2 {};
    }

    float2 InputManager::GetScrollDelta(Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr ? state->m_scrollDelta : float2 {};
    }

    bool InputManager::IsMouseButtonPressed(MouseInputButton _button, Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr && state->m_mouseButtons[static_cast<size_t>(_button)].m_pressed;
    }

    bool InputManager::WasMouseButtonJustPressed(MouseInputButton _button, Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr && state->m_mouseButtons[static_cast<size_t>(_button)].m_justPressed;
    }

    bool InputManager::WasMouseButtonJustReleased(MouseInputButton _button, Window* _window) const
    {
        Window* window = _window != nullptr ? _window : m_focusedWindow;
        const WindowInputState* state = window != nullptr ? FindWindowState(window) : nullptr;
        return state != nullptr && state->m_mouseButtons[static_cast<size_t>(_button)].m_justReleased;
    }

    void InputManager::PushConsumer(InputConsumer* _consumer, s32 _priority)
    {
        const auto it = eastl::find_if(
            m_consumers.begin(),
            m_consumers.end(),
            [_priority](const ConsumerEntry& _entry) { return _entry.m_priority < _priority; });
        m_consumers.insert(it, ConsumerEntry { _consumer, _priority });
    }

    void InputManager::RemoveConsumer(InputConsumer* _consumer)
    {
        m_consumers.erase(
            eastl::remove_if(
                m_consumers.begin(),
                m_consumers.end(),
                [_consumer](const ConsumerEntry& _entry) { return _entry.m_consumer == _consumer; }),
            m_consumers.end());
    }

    ActionId InputManager::RegisterAction(const eastl::string_view& _name, const InputAction& _action)
    {
        const u32 id = static_cast<u32>(m_actions.size());
        m_actions.push_back(RegisteredAction {
            .m_name = eastl::string(_name.data(), _name.size(), m_allocator),
            .m_action = _action,
        });
        return static_cast<ActionId>(id);
    }

    bool InputManager::IsActionPressed(ActionId _action) const
    {
        const size_t index = static_cast<size_t>(_action);
        return index < m_actions.size() && m_actions[index].m_pressed;
    }

    bool InputManager::WasActionJustPressed(ActionId _action) const
    {
        const size_t index = static_cast<size_t>(_action);
        return index < m_actions.size() && m_actions[index].m_justPressed;
    }

    bool InputManager::WasActionJustReleased(ActionId _action) const
    {
        const size_t index = static_cast<size_t>(_action);
        return index < m_actions.size() && m_actions[index].m_justReleased;
    }

    float InputManager::GetActionValue(ActionId _action) const
    {
        const size_t index = static_cast<size_t>(_action);
        return index < m_actions.size() ? m_actions[index].m_value : 0.f;
    }

    void InputManager::UpdateActions()
    {
        for (RegisteredAction& action : m_actions)
        {
            action.m_justPressed = false;
            action.m_justReleased = false;

            float value = 0.f;
            for (const InputBinding& binding : action.m_action.m_bindings)
            {
                switch (binding.m_source)
                {
                case InputBinding::Source::Key:
                    if (IsKeyPressed(static_cast<InputKeys>(binding.m_code)))
                        value += binding.m_scale;
                    break;
                case InputBinding::Source::MouseButton:
                    if (IsMouseButtonPressed(static_cast<MouseInputButton>(binding.m_code)))
                        value += binding.m_scale;
                    break;
                case InputBinding::Source::MouseAxis:
                {
                    float axisValue = 0.f;
                    switch (static_cast<MouseAxis>(binding.m_code))
                    {
                    case MouseAxis::X:
                        axisValue = GetCursorDelta().x;
                        break;
                    case MouseAxis::Y:
                        axisValue = GetCursorDelta().y;
                        break;
                    case MouseAxis::ScrollX:
                        axisValue = GetScrollDelta().x;
                        break;
                    case MouseAxis::ScrollY:
                        axisValue = GetScrollDelta().y;
                        break;
                    }
                    value += axisValue * binding.m_scale;
                    break;
                }
                }
            }

            const float absValue = value < 0.f ? -value : value;
            if (absValue < action.m_action.m_deadzone)
                value = 0.f;

            const bool pressed = value != 0.f;
            if (pressed && !action.m_pressed)
                action.m_justPressed = true;
            else if (!pressed && action.m_pressed)
                action.m_justReleased = true;
            action.m_pressed = pressed;
            action.m_value = value;
        }
    }
} // namespace KryneEngine
