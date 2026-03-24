/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2026.
 */

#pragma once

#include <EASTL/span.h>
#include <KryneEngine/Core/Common/BitUtils.hpp>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Hashing.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"

namespace KryneEngine::Modules::TextRendering
{
    /**
     * @brief A representation of a binary pre-baked font file. This file has data type modularity and is
     * streaming-ready.
     *
     * @details
     * The file is laid out so that a good chunk of the baked data can be streamed on demand if needed.
     * This is achieved by storing all the baked data at the tail of the file. Baked data is also packed into per glyph
     * blobs for this purpose.
     * The indexing tables are all stored contiguously right after the general gly data table, using the same glyph
     * index. They contain all the info for determining the data blob span, so they should be loaded during initial load
     * if streaming.
     *
     * The layout looks like this:
     * - header
     * - general glyph data table
     * - indexing tables
     *   - MSDF entries table (if applicable)
     *   - outline entries table (if applicable)
     * - zstd dictionary (if compressed)
     * - streamable data blobs
     *   - per glyph MSDF bitmaps (if applicable)
     *   - per glyph outline data (if applicable)
     *     - points array
     *     - outline tags array
     */
    class PreBakedFontFile
    {
    public:
        explicit PreBakedFontFile(eastl::span<std::byte> _data);

        enum class BakedRenderInfo
        {
            Msdf =      1 << 0,
            Outlines =  1 << 1,
        };

        struct FontMetrics
        {
            float m_ascender;
            float m_descender;
            float m_lineHeight;
            u32 m_msdfBakedFontSize;
        };

        struct GlyphEntry
        {
            u32 m_codePoint;
            float m_advanceX;
            float m_bearingX;
            float m_bearingY;
            float m_width;
            float m_height;
        };

        struct GlyphBakeInfo
        {
            GlyphEntry m_glyph;

            eastl::span<std::byte> m_msdfBitmap;
            u16 m_msdfWidth;
            u16 m_msdfHeight;
            u16 m_msdfBaseline;

            u32 m_outlineStartPoint;
            u32 m_outlineFirstTag;
            u32 m_outlineTagCount;
        };

        static eastl::span<std::byte> Bake(
            BakedRenderInfo _renderInfo,
            FontMetrics _fontMetrics,
            eastl::span<const GlyphBakeInfo> _glyphs,
            eastl::span<float2> _outlinePoints,
            eastl::span<OutlineTag> _outlineTags,
            AllocatorInstance _allocator);

        static eastl::span<std::byte> BakeCompressed(
            BakedRenderInfo _renderInfo,
            FontMetrics _fontMetrics,
            eastl::span<const GlyphBakeInfo> _glyphs,
            eastl::span<float2> _outlinePoints,
            eastl::span<OutlineTag> _outlineTags,
            AllocatorInstance _allocator);

    private:
        eastl::span<std::byte> m_data;

        struct Header
        {
            u64 m_magicNumber;
            FontMetrics m_fontMetrics;
            struct
            {
                bool m_compressed: 1;
                BakedRenderInfo m_renderInfo : 31;
            } m_options;
            u32 m_glyphCount;
        };

        struct MsdfEntry
        {
            u32 m_offset;
            u16 m_glyphWidth;
            u16 m_glyphHeight;
            u16 m_baseline;
        };

        struct OutlineEntry
        {
            u32 m_pointsOffset;
            u32 m_tagsOffset;
            u32 m_tagsCount;
        };

        static constexpr u64 kMagicNumber = Hashing::Hash64Static("PreBakedFontFile");
    };

    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(PreBakedFontFile::BakedRenderInfo);
}
