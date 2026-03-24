/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#pragma once

#include <EASTL/optional.h>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Threads/SpinLock.hpp>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"

struct FT_FaceRec_;

namespace KryneEngine::Modules::TextRendering
{
    class FreetypeFontFile
    {
        friend class FontManager;

    public:
        FreetypeFontFile(FT_FaceRec_* _face, std::byte* _fileBuffer, AllocatorInstance _fileBufferAllocator);

        float GetAscender(float _fontSize) const;
        float GetDescender(float _fontSize) const;
        float GetLineHeight(float _fontSize) const;

        eastl::optional<float> GetHorizontalAdvance(u32 _unicodeCodepoint, float _fontSize);

        eastl::optional<GlyphLayoutMetrics> GetGlyphLayoutMetrics(u32 _unicodeCodepoint, float _fontSize);

        bool HasOutline(u32 _unicodeCodepoint) const;

        GlyphShape AcquireGlyphShape(u32 _unicodeCodepoint);
        void ReleaseGlyphShape(u32 _unicodeCodepoint, const GlyphShape& _glyphShape);

        void Destroy(AllocatorInstance _allocator) const;

    private:
        struct GlyphEntry
        {
            u32 m_glyphIndex;
            // Should be accessed as atomic ref in concurrent contexts. We don't store it as a std::atomic to allow for
            // vector map sorting
            bool m_loaded = false;

            u32 m_baseAdvanceX;

            u32 m_baseBearingX;
            u32 m_baseWidth;

            u32 m_baseBearingY;
            u32 m_baseHeight;

            u32 m_outlineStartPoint;
            u32 m_outlineFirstTag;
            u32 m_outlineTagCount;
        };

        FT_FaceRec_* m_face = nullptr;
        std::byte* m_fileBuffer = nullptr;
        eastl::vector<float2> m_points;
        eastl::vector<OutlineTag> m_tags;
        eastl::vector_map<u32, GlyphEntry> m_glyphs;
        SpinLock m_loadLock {};
        SpinLock m_outlinesLock {};

        void LoadGlyph(size_t _vectorMapIndex);
        void LoadGlyphSafe(size_t _vectorMapIndex);
    };
}
