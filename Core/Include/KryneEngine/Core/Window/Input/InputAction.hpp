/**
 * @file
 * @author Max Godefroy
 * @date 11/09/2026.
 */

#pragma once

#include <EASTL/fixed_vector.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Window/Input/Enums.hpp"

namespace KryneEngine
{
    /// @brief Handle for a registered @ref InputAction, returned by @ref InputManager::RegisterAction.
    enum class ActionId : u32 { Invalid = ~0u };

    /// @brief One physical input mapped into an @ref InputAction, with a per-binding scale (negate via -1).
    struct InputBinding
    {
        enum class Source : u8 { Key, MouseButton, MouseAxis };

        Source m_source = Source::Key;
        /// @brief An `InputKeys`, `MouseInputButton` or `MouseAxis` value, depending on #m_source.
        s32 m_code = 0;
        float m_scale = 1.f;
    };

    /**
     * @brief A named, data-shaped gameplay action, resolved every @ref InputManager::Update from its
     * bindings' polled state.
     *
     * @details Registration is hardcoded for now (see the windowing/input redesign plan, section 5.3);
     * the shape stays config-file-friendly so loading bindings from disk is additive later. Mapping
     * contexts (stackable/prioritised action scopes) are an intentional gap, left for when a sample
     * needs more than one input scope.
     */
    struct InputAction
    {
        eastl::fixed_vector<InputBinding, 4> m_bindings;
        float m_deadzone = 0.15f;
    };
} // namespace KryneEngine
