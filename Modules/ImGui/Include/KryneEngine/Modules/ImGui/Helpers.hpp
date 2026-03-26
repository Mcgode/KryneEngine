/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#pragma once

#include <imgui.h>
#include <KryneEngine/Core/Common/Types.hpp>

namespace KryneEngine::Modules::ImGui::Helpers
{
    bool Spinner(const char* _strId, float _radius, float _thickness, const ImU32& _color);
}
