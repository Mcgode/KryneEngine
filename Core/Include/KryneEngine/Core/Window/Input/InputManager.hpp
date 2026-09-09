/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include <EASTL/vector_map.h>

#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"
#include "KryneEngine/Core/Window/Input/KeyInputEvent.hpp"
#include "KryneEngine/Core/Window/Input/MouseInputEvent.hpp"

namespace KryneEngine
{
    /**
     * @brief Per-application input hub.
     *
     * @details
     * Owned and fed by the @ref WindowManager, which translates the raw GLFW callbacks into engine
     * events and forwards them through the `On*Event` entry points. Consumers register `eastl::function`
     * listeners; a full event-queue / consumer-stack / action-map rewrite is planned (Phase 4).
     */
    class InputManager
    {
    public:
        explicit InputManager(AllocatorInstance _allocator);

        [[nodiscard]] u32 RegisterKeyInputEventCallback(eastl::function<void(const KeyInputEvent&)>&& _callback);
        void UnregisterKeyInputEventCallback(u32 _id);

        [[nodiscard]] u32 RegisterTextInputEventCallback(eastl::function<void(u32)>&& _callback);
        void UnregisterTextInputEventCallback(u32 _id);

        [[nodiscard]] u32 RegisterCursorPosEventCallback(eastl::function<void(float, float)>&& _callback);
        void UnregisterCursorPosEventCallback(u32 _id);
        [[nodiscard]] const float2& GetCursorPos() const { return m_cursorPos; }

        [[nodiscard]] u32 RegisterMouseInputEventCallback(eastl::function<void(const MouseInputEvent&)>&& _callback);
        void UnregisterMouseInputEventCallback(u32 _id);

        [[nodiscard]] u32 RegisterScrollInputEventCallback(eastl::function<void(float, float)>&& _callback);
        void UnregisterScrollInputEventCallback(u32 _id);

        // --- Fed by the WindowManager's GLFW callbacks (already translated to engine events) ---
        void OnKeyEvent(const KeyInputEvent& _event);
        void OnTextEvent(u32 _codepoint);
        void OnCursorPosEvent(float _posX, float _posY);
        void OnMouseButtonEvent(const MouseInputEvent& _event);
        void OnScrollEvent(float _scrollX, float _scrollY);

    protected:
        LightweightMutex m_mutex;

        eastl::vector_map<u32, eastl::function<void(const KeyInputEvent&)>> m_keyInputEventListeners;
        u32 m_keyInputEventCounter { 0 };

        eastl::vector_map<u32, eastl::function<void(u32)>> m_textInputEventListeners;
        u32 m_textInputEventCounter { 0 };

        eastl::vector_map<u32, eastl::function<void(float, float)>> m_cursorPosEventListeners;
        u32 m_cursorPosEventCounter = 0;
        float2 m_cursorPos;

        eastl::vector_map<u32, eastl::function<void(const MouseInputEvent&)>> m_mouseInputEventListeners;
        u32 m_mouseInputEventCounter = 0;

        eastl::vector_map<u32, eastl::function<void(float, float)>> m_scrollInputEventListeners;
        u32 m_scrollInputEventCounter = 0;
    };
} // namespace KryneEngine
