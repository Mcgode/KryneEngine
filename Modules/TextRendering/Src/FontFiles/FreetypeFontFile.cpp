/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#include "KryneEngine/Modules/TextRendering/FontFiles/FreetypeFontFile.hpp"

#include "KryneEngine/Modules/TextRendering/Utils/FreetypeFunctionHelpers.hpp"

namespace KryneEngine::Modules::TextRendering
{
    FreetypeFontFile::FreetypeFontFile(
        FT_FaceRec_* _face,
        std::byte* _fileBuffer,
        const AllocatorInstance _fileBufferAllocator)
            : m_face(_face)
            , m_fileBuffer(_fileBuffer)
            , m_points(_fileBufferAllocator)
            , m_tags(_fileBufferAllocator)
            , m_glyphs(_fileBufferAllocator)
    {
    }

    float FreetypeFontFile::GetAscender(const float _fontSize) const
    {
        return _fontSize * static_cast<float>(m_face->ascender) / static_cast<float>(m_face->units_per_EM);
    }

    float FreetypeFontFile::GetDescender(const float _fontSize) const
    {
        return _fontSize * static_cast<float>(m_face->descender) / static_cast<float>(m_face->units_per_EM);
    }

    float FreetypeFontFile::GetLineHeight(const float _fontSize) const
    {
        return _fontSize * static_cast<float>(m_face->height) / static_cast<float>(m_face->units_per_EM);
    }

    eastl::optional<float> FreetypeFontFile::GetHorizontalAdvance(u32 _unicodeCodepoint, float _fontSize)
    {
        auto it = m_glyphs.find(_unicodeCodepoint);
        if (it != m_glyphs.end())
        {
            if (it == m_glyphs.end())
                it = m_glyphs.begin();
            GlyphEntry& entry = it->second;

            // Atomic relaxed load to check if loaded. If not, cache it.
            if (std::atomic_ref(entry.m_loaded).load(std::memory_order_relaxed) == false) [[unlikely]]
                LoadGlyphSafe(eastl::distance(m_glyphs.begin(), it));

            return _fontSize * static_cast<float>(entry.m_baseAdvanceX) / static_cast<float>(m_face->units_per_EM);
        }
        return {};
    }

    eastl::optional<GlyphLayoutMetrics> FreetypeFontFile::GetGlyphLayoutMetrics(u32 _unicodeCodepoint, float _fontSize)
    {
        auto it = m_glyphs.find(_unicodeCodepoint);
        if (it != m_glyphs.end())
        {
            if (it == m_glyphs.end())
                it = m_glyphs.begin();
            GlyphEntry& entry = it->second;

            // Atomic relaxed load to check if loaded. If not, cache it.
            if (std::atomic_ref(entry.m_loaded).load(std::memory_order_relaxed) == false) [[unlikely]]
                LoadGlyphSafe(eastl::distance(m_glyphs.begin(), it));

            const float emScale = 1.f / static_cast<float>(m_face->units_per_EM);
            return GlyphLayoutMetrics {
                _fontSize * emScale * static_cast<float>(entry.m_baseAdvanceX),
                _fontSize * emScale * static_cast<float>(entry.m_baseBearingX),
                _fontSize * emScale * static_cast<float>(entry.m_baseWidth),
                _fontSize * emScale * static_cast<float>(entry.m_baseBearingY),
                _fontSize * emScale * static_cast<float>(entry.m_baseHeight)
            };
        }
        return {};
    }

    bool FreetypeFontFile::HasOutline(u32 _unicodeCodepoint) const
    {
        return m_glyphs.find(_unicodeCodepoint) != m_glyphs.end();
    }

    GlyphShape FreetypeFontFile::AcquireGlyphShape(const u32 _unicodeCodepoint)
    {
        auto it = m_glyphs.find(_unicodeCodepoint);
        m_outlinesLock.Lock();
        return {
            .m_points = m_points.begin() + it->second.m_outlineStartPoint,
            .m_tags = eastl::span { m_tags.begin() + it->second.m_outlineFirstTag, it->second.m_outlineTagCount },
        };
    }

    void FreetypeFontFile::ReleaseGlyphShape(u32, const GlyphShape&)
    {
        m_outlinesLock.Unlock();
    }

    void FreetypeFontFile::Destroy(const AllocatorInstance _allocator) const
    {
        FT_Done_Face(m_face);
        _allocator.deallocate(m_fileBuffer);
    }

    void FreetypeFontFile::LoadGlyph(const size_t _vectorMapIndex)
    {
        GlyphEntry& glyphEntry = (m_glyphs.begin() + _vectorMapIndex)->second;

        if (m_face->glyph == nullptr || m_face->glyph->glyph_index != glyphEntry.m_glyphIndex)
        {
            {
                const FT_Error error = FT_Load_Glyph(m_face, glyphEntry.m_glyphIndex, FT_LOAD_NO_BITMAP);
                KE_ASSERT_MSG(error == FT_Err_Ok, FT_Error_String(error));
            }
        }

        const FT_GlyphSlot glyph = m_face->glyph;

        glyphEntry.m_baseAdvanceX = glyph->metrics.horiAdvance;

        glyphEntry.m_baseBearingX = glyph->metrics.horiBearingX;
        glyphEntry.m_baseWidth = glyph->metrics.width;

        glyphEntry.m_baseBearingY = glyph->metrics.horiBearingY;
        glyphEntry.m_baseHeight = glyph->metrics.height;

        m_outlinesLock.Lock();

        glyphEntry.m_outlineFirstTag = m_tags.size();
        glyphEntry.m_outlineStartPoint = m_points.size();

        Freetype::LoadOutline(m_face, m_points, m_tags);

        glyphEntry.m_outlineTagCount = m_tags.size() - glyphEntry.m_outlineFirstTag;

        m_outlinesLock.Unlock();
    }

    void FreetypeFontFile::LoadGlyphSafe(const size_t _vectorMapIndex)
    {
        const auto lock = m_loadLock.AutoLock();

        // Check that load hasn't been performed while waiting for spinlock.
        GlyphEntry& glyphEntry = (m_glyphs.begin() + _vectorMapIndex)->second;
        if (std::atomic_ref(glyphEntry.m_loaded).load(std::memory_order_acquire))
        {
            return;
        }

        LoadGlyph(_vectorMapIndex);

        // Load was performed, update status.
        std::atomic_ref(glyphEntry.m_loaded).store(true, std::memory_order_release);
    }
}
