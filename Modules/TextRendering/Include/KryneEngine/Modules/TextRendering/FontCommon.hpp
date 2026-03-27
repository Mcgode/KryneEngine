/**
 * @file
 * @author Max Godefroy
 * @date 13/01/2026.
 */

#pragma once

#include <EASTL/span.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Modules::TextRendering
{
    enum class OutlineTag: u8
    {
        NewContour,
        Line,
        Conic,
        Cubic
    };

    struct GlyphLayoutMetrics
    {
        float m_advanceX;
        float m_bearingX;
        float m_width;
        float m_bearingY;
        float m_height;
    };

    struct GlyphShape
    {
        float2* m_points = nullptr;
        eastl::span<OutlineTag> m_tags {};
    };

    struct GlyphMsdfBitmap
    {
        eastl::span<std::byte> m_bitmap {};
        u16 m_pxRange = 0;
        u16 m_width = 0;
        u16 m_height = 0;
        u16 m_fontSize = 0;
        u16 m_baseLine = 0;
        bool m_allocated = true;
    };
}