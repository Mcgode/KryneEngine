/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#include "KryneEngine/Modules/ImGui/Helpers.hpp"

#include <imgui_internal.h>

namespace KryneEngine::Modules::ImGui::Helpers
{
    bool Spinner(const char* _strId, const float _radius, const float _thickness, const ImU32& _color) {
        ImGuiWindow* window = ::ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(_strId);

        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 size((_radius )*2, (_radius + style.FramePadding.y)*2);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ::ImGui::ItemSize(bb, style.FramePadding.y);
        if (!::ImGui::ItemAdd(bb, id))
            return false;

        // Render
        window->DrawList->PathClear();

        constexpr u32 numSegments = 30;
        const float start = fabs(ImSin(g.Time * 1.8f) * (numSegments - 5));

        const float aMin = IM_PI * 2.0f * start / static_cast<float>(numSegments);
        constexpr float aMax = IM_PI * 2.0f * (static_cast<float>(numSegments) - 3) / static_cast<float>(numSegments);

        const ImVec2 center (pos.x+_radius, pos.y+_radius+style.FramePadding.y);

        for (int i = 0; i < numSegments; i++) {
            const float a = aMin + (static_cast<float>(i) / static_cast<float>(numSegments)) * (aMax - aMin);
            window->DrawList->PathLineTo(ImVec2(
                center.x + ImCos(a + g.Time * 8) * _radius,
                center.y + ImSin(a + g.Time * 8) * _radius));
        }

        window->DrawList->PathStroke(_color, false, _thickness);

        return true;
    }
}
