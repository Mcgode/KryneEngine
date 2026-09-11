/**
 * @file
 * @author Max Godefroy
 * @date 11/09/2026.
 */

#pragma once

namespace KryneEngine
{
    struct InputEvent;

    /**
     * @brief Priority-stacked event sink for the @ref InputManager's per-frame drain.
     *
     * @details Push onto the stack with @ref InputManager::PushConsumer. During
     * @ref InputManager::Update, each raw event is offered to consumers from highest to lowest
     * priority; the first one whose #HandleEvent returns `true` stops the event from reaching
     * lower-priority consumers and from folding into the polling state (window-lifecycle events are
     * the exception: they always fold into state regardless of consumption).
     */
    class InputConsumer
    {
    public:
        virtual ~InputConsumer() = default;

        /// @return `true` to consume the event (stop propagation to lower-priority consumers).
        virtual bool HandleEvent(const InputEvent& _event) = 0;
    };
} // namespace KryneEngine
