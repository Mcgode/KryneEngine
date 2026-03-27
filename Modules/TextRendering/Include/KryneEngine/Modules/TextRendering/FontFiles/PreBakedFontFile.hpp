/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2026.
 */

#pragma once

#include <fstream>
#include <EASTL/optional.h>
#include <EASTL/span.h>
#include <KryneEngine/Core/Common/BitUtils.hpp>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Hashing.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"

struct ZSTD_DCtx_s;
struct ZSTD_DDict_s;

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
     *
     * Recommended file extension is '.ke_pbf'
     */
    class PreBakedFontFile
    {
        friend class FontManager;

    public:
        PreBakedFontFile(std::ifstream& _file, size_t _fileSize, AllocatorInstance _allocator);

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
            u16 m_msdfBakedFontSize;

            u32 m_outlineStartPoint;
            u32 m_outlineFirstTag;
            u32 m_outlineTagCount;
        };

        void Destroy(AllocatorInstance _allocator) const;

        [[nodiscard]] float GetAscender(float _fontSize) const;
        [[nodiscard]] float GetDescender(float _fontSize) const;
        [[nodiscard]] float GetLineHeight(float _fontSize) const;

        [[nodiscard]] eastl::optional<float> GetHorizontalAdvance(u32 _unicodeCodepoint, float _fontSize) const;

        [[nodiscard]] eastl::optional<GlyphLayoutMetrics> GetGlyphLayoutMetrics(u32 _unicodeCodepoint, float _fontSize) const;

        [[nodiscard]] eastl::optional<u32> GetGlyphIndex(u32 _unicodeCodepoint) const;

        [[nodiscard]] bool HasMsdfBitmaps() const;
        [[nodiscard]] bool HasOutlines() const;

        [[nodiscard]] GlyphMsdfBitmap GetMsdfBitmap(u32 _glyphIndex, AllocatorInstance _allocator) const;

        [[nodiscard]] GlyphShape GetGlyphShape(u32 _codepoint, AllocatorInstance _allocatorInstance) const;
        void ReleaseGlyphShape(const GlyphShape& _glyphShape, AllocatorInstance _allocator) const;

        static bool IsPreBakedFontFile(eastl::span<const std::byte> _data);

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
            u16 m_bakedFontSize;
        };

        struct OutlineEntry
        {
            u32 m_pointsOffset;
            u32 m_tagsOffset;
            u32 m_tagsCount;
        };

        static constexpr u64 kMagicNumber = Hashing::Hash64Static("PreBakedFontFile");

        Header m_header {};
        GlyphEntry* m_glyphs = nullptr;
        MsdfEntry* m_msdfEntries = nullptr;
        OutlineEntry* m_outlineEntries = nullptr;
        eastl::span<std::byte> m_data {};
        eastl::span<std::byte> m_dictBuffer {};
        ZSTD_DCtx_s* m_ctx = nullptr;
        ZSTD_DDict_s* m_dict = nullptr;

        [[nodiscard]] GlyphEntry* FindGlyphEntry(u32 _codePoint) const;
    };

    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(PreBakedFontFile::BakedRenderInfo);
}
