/**
 * @file
 * @author Max Godefroy
 * @date 16/08/2024.
 */

#pragma once

#include "KryneEngine/Core/Window/Input/Enums.hpp"

namespace KryneEngine::GLFW
{
    [[nodiscard]] InputKeys ToInputPhysicalKeys(s32 _glfwKey);
    /// @brief Reverse of #ToInputPhysicalKeys. Returns `GLFW_KEY_UNKNOWN` for `InputKeys::Unknown`.
    [[nodiscard]] s32 FromInputPhysicalKeys(InputKeys _key);
    [[nodiscard]] InputActionType ToInputEventAction(s32 _glfwAction);
    [[nodiscard]] KeyInputModifiers ToInputEventModifiers(s32 _glfwMods);
    [[nodiscard]] MouseInputButton ToMouseInputButton(s32 _glfwMouse);
}
