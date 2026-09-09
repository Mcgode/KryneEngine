/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#include "KryneEngine/Core/Window/Input/InputManager.hpp"

#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    InputManager::InputManager(AllocatorInstance _allocator)
        : m_keyInputEventListeners(_allocator)
        , m_textInputEventListeners(_allocator)
        , m_cursorPosEventListeners(_allocator)
        , m_mouseInputEventListeners(_allocator)
        , m_scrollInputEventListeners(_allocator)
    {}

    u32 InputManager::RegisterKeyInputEventCallback(eastl::function<void(Window*, const KeyInputEvent&)>&& _callback)
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

    u32 InputManager::RegisterTextInputEventCallback(eastl::function<void(Window*, u32)>&& _callback)
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

    u32 InputManager::RegisterCursorPosEventCallback(eastl::function<void(Window*, float, float)>&& _callback)
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

    u32 InputManager::RegisterMouseInputEventCallback(eastl::function<void(Window*, const MouseInputEvent&)>&& _callback)
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

    u32 InputManager::RegisterScrollInputEventCallback(eastl::function<void(Window*, float, float)>&& _callback)
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

    void InputManager::OnKeyEvent(Window* _window, const KeyInputEvent& _event)
    {
        KE_ZoneScopedFunction("InputManager::OnKeyEvent");

        const auto lock = m_mutex.AutoLock();
        for (const auto& pair : m_keyInputEventListeners)
        {
            pair.second(_window, _event);
        }
    }

    void InputManager::OnTextEvent(Window* _window, u32 _codepoint)
    {
        KE_ZoneScopedFunction("InputManager::OnTextEvent");

        const auto lock = m_mutex.AutoLock();
        for (const auto& pair : m_textInputEventListeners)
        {
            pair.second(_window, _codepoint);
        }
    }

    void InputManager::OnCursorPosEvent(Window* _window, float _posX, float _posY)
    {
        KE_ZoneScopedFunction("InputManager::OnCursorPosEvent");

        m_cursorPos = { _posX, _posY };

        const auto lock = m_mutex.AutoLock();
        for (const auto& pair : m_cursorPosEventListeners)
        {
            pair.second(_window, _posX, _posY);
        }
    }

    void InputManager::OnMouseButtonEvent(Window* _window, const MouseInputEvent& _event)
    {
        KE_ZoneScopedFunction("InputManager::OnMouseButtonEvent");

        const auto lock = m_mutex.AutoLock();
        for (const auto& pair : m_mouseInputEventListeners)
        {
            pair.second(_window, _event);
        }
    }

    void InputManager::OnScrollEvent(Window* _window, float _scrollX, float _scrollY)
    {
        KE_ZoneScopedFunction("InputManager::OnScrollEvent");

        const auto lock = m_mutex.AutoLock();
        for (const auto& pair : m_scrollInputEventListeners)
        {
            pair.second(_window, _scrollX, _scrollY);
        }
    }
} // namespace KryneEngine
