/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#include "KryneEngine/Core/Window/Input/InputManager.hpp"

#include <GLFW/glfw3.h>

#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "KryneEngine/Core/Window/Window.hpp"
#include "KryneEngine/Core/Window/GLFW/Input/KeyInputEvent.hpp"

namespace KryneEngine
{
    InputManager::InputManager(Window* _window, AllocatorInstance _allocator)
        : m_keyInputEventListeners(_allocator)
        , m_textInputEventListeners(_allocator)
        , m_cursorPosEventListeners(_allocator)
        , m_mouseInputEventListeners(_allocator)
        , m_scrollInputEventListeners(_allocator)
    {
        GLFWwindow* glfwWindow = _window->GetGlfwWindow();

        glfwSetKeyCallback(glfwWindow, KeyCallback);
        glfwSetCharCallback(glfwWindow, TextCallback);
        glfwSetCursorPosCallback(glfwWindow, CursorPosCallback);
        glfwSetMouseButtonCallback(glfwWindow, MouseButtonInputCallback);
        glfwSetScrollCallback(glfwWindow, ScrollCallback);
    }

    u32 InputManager::RegisterKeyInputEventCallback(eastl::function<void(const KeyInputEvent&)>&& _callback)
    {
        const auto lock = m_mutex.AutoLock();

        const u32 id = m_keyInputEventCounter++;
        m_keyInputEventListeners.emplace(id, _callback);
        return id;
    }

    void InputManager::UnregisterKeyInputEventCallback(u32 _id)
    {
        const auto lock = m_mutex.AutoLock();
        m_keyInputEventListeners.erase(_id);
    }

    u32 InputManager::RegisterTextInputEventCallback(eastl::function<void(u32)>&& _callback)
    {
        const auto lock = m_mutex.AutoLock();

        const u32 id = m_textInputEventCounter++;
        m_textInputEventListeners.emplace(id, _callback);
        return id;
    }

    void InputManager::UnregisterTextInputEventCallback(u32 _id)
    {
        const auto lock = m_mutex.AutoLock();
        m_textInputEventListeners.erase(_id);
    }

    u32 InputManager::RegisterCursorPosEventCallback(eastl::function<void(float, float)>&& _callback)
    {
        const auto lock = m_mutex.AutoLock();

        const u32 id = m_cursorPosEventCounter++;
        m_cursorPosEventListeners.emplace(id, _callback);
        return id;
    }

    void InputManager::UnregisterCursorPosEventCallback(u32 _id)
    {
        const auto lock = m_mutex.AutoLock();
        m_cursorPosEventListeners.erase(_id);
    }

    u32 InputManager::RegisterMouseInputEventCallback(eastl::function<void(const MouseInputEvent&)>&& _callback)
    {
        const auto lock = m_mutex.AutoLock();

        const u32 id = m_mouseInputEventCounter++;
        m_mouseInputEventListeners.emplace(id, _callback);
        return id;
    }

    void InputManager::UnregisterMouseInputEventCallback(u32 _id)
    {
        const auto lock = m_mutex.AutoLock();
        m_mouseInputEventListeners.erase(_id);
    }

    u32 InputManager::RegisterScrollInputEventCallback(eastl::function<void(float, float)>&& _callback)
    {
        const auto lock = m_mutex.AutoLock();

        const u32 id = m_scrollInputEventCounter++;
        m_scrollInputEventListeners.emplace(id, _callback);
        return id;
    }

    void InputManager::UnregisterScrollInputEventCallback(u32 _id)
    {
        const auto lock = m_mutex.AutoLock();
        m_scrollInputEventListeners.erase(_id);
    }

    void InputManager::KeyCallback(GLFWwindow* _window, s32 _key, s32 _scancode, s32 _action, s32 _mods)
    {
        KE_ZoneScopedFunction("InputManager::KeyCallback");

        InputManager* inputManager = (static_cast<Window*>(glfwGetWindowUserPointer(_window)))->GetInputManager();

        const KeyInputEvent keyInputEvent {
            .m_physicalKey = GLFW::ToInputPhysicalKeys(_key),
            .m_customCode = _scancode,
            .m_action = GLFW::ToInputEventAction(_action),
            .m_modifiers = GLFW::ToInputEventModifiers(_mods),
        };

        const auto lock = inputManager->m_mutex.AutoLock();

        for (const auto& pair : inputManager->m_keyInputEventListeners)
        {
            pair.second(keyInputEvent);
        }
    }

    void InputManager::TextCallback(GLFWwindow* _window, u32 _char)
    {
        KE_ZoneScopedFunction("InputManager::TextCallback");

        InputManager* inputManager = (static_cast<Window*>(glfwGetWindowUserPointer(_window)))->GetInputManager();

        const auto lock = inputManager->m_mutex.AutoLock();

        for (const auto& pair: inputManager->m_textInputEventListeners)
        {
            pair.second(_char);
        }
    }

    void InputManager::CursorPosCallback(GLFWwindow* _window, double _posX, double _posY)
    {
        KE_ZoneScopedFunction("InputManager::CursorPosCallback");

        InputManager* inputManager = (static_cast<Window*>(glfwGetWindowUserPointer(_window)))->GetInputManager();
        inputManager->m_cursorPos = {
            _posX,
            _posY
        };

        const auto lock = inputManager->m_mutex.AutoLock();

        for (const auto& pair : inputManager->m_cursorPosEventListeners)
        {
            pair.second(static_cast<float>(_posX), static_cast<float>(_posY));
        }
    }

    void InputManager::MouseButtonInputCallback(GLFWwindow* _window, s32 _button, s32 _action, s32 _mods)
    {
        KE_ZoneScopedFunction("InputManager::MouseButtonInputCallback");

        InputManager* inputManager = (static_cast<Window*>(glfwGetWindowUserPointer(_window)))->GetInputManager();

        const MouseInputEvent mouseInputEvent{
            .m_mouseButton = GLFW::ToMouseInputButton(_button),
            .m_action = GLFW::ToInputEventAction(_action),
            .m_modifiers = GLFW::ToInputEventModifiers(_mods),
        };

        const auto lock = inputManager->m_mutex.AutoLock();

        for (const auto& pair : inputManager->m_mouseInputEventListeners)
        {
            pair.second(mouseInputEvent);
        }
    }

    void InputManager::ScrollCallback(GLFWwindow* _window, double _xScroll, double _yScroll)
    {
        KE_ZoneScopedFunction("InputManager::ScrollCallback");

        InputManager* inputManager = (static_cast<Window*>(glfwGetWindowUserPointer(_window)))->GetInputManager();

        const auto lock = inputManager->m_mutex.AutoLock();

        for (const auto& pair : inputManager->m_scrollInputEventListeners)
        {
            pair.second(static_cast<float>(_xScroll), static_cast<float>(_yScroll));
        }
    }
} // namespace KryneEngine