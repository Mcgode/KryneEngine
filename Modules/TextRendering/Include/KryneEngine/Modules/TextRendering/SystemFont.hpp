/**
 * @file
 * @author Max Godefroy
 * @date 13/01/2026.
 */

#pragma once

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Threads/SpinLock.hpp>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"


namespace KryneEngine::Modules::TextRendering
{
    class SystemFont
    {
        friend class FontManager;
    public:
        float GetHorizontalAdvance(u32 _unicodeCodePoint, float _fontSize);

        GlyphLayoutMetrics GetGlyphLayoutMetrics(u32 _unicodeCodePoint, float _fontSize);

        [[nodiscard]] GlyphMsdfBitmap GenerateMsdf(
            u32 _unicodeCodepoint,
            u16 _fontSize,
            u16 _pxRange,
            AllocatorInstance _allocator);

    private:
        explicit SystemFont(AllocatorInstance _allocator);

        struct GlyphEntry
        {
            u32 m_fontAscender;
            u32 m_fontDescender;
            u32 m_fontLineHeight;
            u32 m_unitsPerEm;

            s32 m_advanceX;
            s32 m_bearingX;
            s32 m_bearingY;
            u32 m_width;
            u32 m_height;

            u32 m_outlineStartPoint;
            u32 m_outlineFirstTag;
            u32 m_outlineTagCount;
        };

        eastl::vector_map<u32, GlyphEntry> m_glyphs;
        eastl::vector<OutlineTag> m_tags;
        eastl::vector<float2> m_glyphPositions;
        SpinLock m_lock {};

        const GlyphEntry& RetrieveGlyph(u32 _unicodeCodePoint);
    };
}
