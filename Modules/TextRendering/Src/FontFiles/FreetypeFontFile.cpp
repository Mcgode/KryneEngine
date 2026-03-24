/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#include "KryneEngine/Modules/TextRendering/FontFiles/FreetypeFontFile.hpp"

#include <freetype/freetype.h>

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
        const FT_Outline outline = glyph->outline;

        glyphEntry.m_baseAdvanceX = glyph->metrics.horiAdvance;

        glyphEntry.m_baseBearingX = glyph->metrics.horiBearingX;
        glyphEntry.m_baseWidth = glyph->metrics.width;

        glyphEntry.m_baseBearingY = glyph->metrics.horiBearingY;
        glyphEntry.m_baseHeight = glyph->metrics.height;

        m_outlinesLock.Lock();

        glyphEntry.m_outlineFirstTag = m_tags.size();
        glyphEntry.m_outlineStartPoint = m_points.size();

        const float2_simd scale { 1.f / static_cast<float>(m_face->units_per_EM) };

        // Based on `FT_Outline_Decompose()` implementation
        for (u32 i = 0; i < outline.n_contours; i++)
        {
            const u32 start = i > 0 ? outline.contours[i - 1] + 1 : 0;
            const u32 last = outline.contours[i];

            u8 tag = FT_CURVE_TAG(outline.tags[start]);

            float2_simd vStart = float2_simd(outline.points[start].x, outline.points[start].y) * scale;
            float2_simd vLast = float2_simd(outline.points[last].x, outline.points[last].y) * scale;
            float2_simd vControl = vStart;

            FT_Vector* pPoints = outline.points + start;
            u8* pTags = outline.tags + start;
            FT_Vector* end = outline.points + last;

            KE_ASSERT(tag != FT_CURVE_TAG_CUBIC);

            if (tag == FT_CURVE_TAG_CONIC)
            {
                if (FT_CURVE_TAG(outline.tags[last]) == FT_CURVE_TAG_ON)
                {
                    vStart = vLast;
                    end--;
                }
                else
                {
                    vStart = (vStart + vLast) / float2_simd(2);
                }
                pPoints--;
                pTags--;
            }

            // First point of the contour
            {
                m_tags.push_back(OutlineTag::NewContour);
                m_points.emplace_back(vStart);
            }

            while (pPoints < end)
            {
                pPoints++;
                pTags++;

                tag = FT_CURVE_TAG(*pTags);
                switch (tag)
                {
                case FT_CURVE_TAG_ON:
                    m_tags.push_back(OutlineTag::Line);
                    m_points.emplace_back(float2_simd(pPoints->x, pPoints->y) * scale);

                    // Close contour
                    if (pPoints == end)
                    {
                        m_tags.push_back(OutlineTag::Line);
                        m_points.emplace_back(vStart);
                    }
                    break;
                case FT_CURVE_TAG_CONIC:
                {
                    m_tags.push_back(OutlineTag::Conic);

                    vControl = float2_simd(pPoints->x, pPoints->y) * scale;
                    m_points.emplace_back(vControl);

                    if (pPoints < end)
                    {
                        tag = FT_CURVE_TAG(pTags[1]);
                        float2_simd vec = float2_simd(pPoints[1].x, pPoints[1].y) * scale;

                        if (tag == FT_CURVE_TAG_ON)
                        {
                            m_points.emplace_back(vec);

                            // We consumed a point, advance.
                            pPoints++;
                            pTags++;
                        }
                        // We are chaining conic arcs, so we take the median point of the two consecutive control points
                        else if (tag == FT_CURVE_TAG_CONIC)
                        {
                            m_points.emplace_back((vControl + vec) / float2_simd(2));
                            // The control point hasn't been consumed yet (we created a median point instead),
                            // so we don't need to advance
                        }
                        else
                        {
                            KE_ERROR("Invalid tag");
                        }
                    }
                    // If there is no more point available, it means we have closed the contour loop, and the last
                    // point is the start point
                    else
                    {
                        m_points.emplace_back(vStart);
                    }

                    break;
                }
                default: // case FT_CURVE_TAG_CUBIC:
                {
                    KE_ASSERT_FATAL(pPoints + 1 <= end && FT_CURVE_TAG(pTags[1]) == FT_CURVE_TAG_CUBIC);

                    m_tags.push_back(OutlineTag::Cubic);

                    float2_simd v1 = float2_simd(pPoints->x, pPoints->y) * scale;
                    pPoints++;
                    float2_simd v2 = float2_simd(pPoints->x, pPoints->y) * scale;
                    pPoints++;

                    m_points.emplace_back(v1);
                    m_points.emplace_back(v2);

                    if (pPoints <= end)
                    {
                        m_points.emplace_back(float2_simd(pPoints->x, pPoints->y) * scale);
                    }
                    // Close contour
                    else
                    {
                        m_points.emplace_back(vStart);
                    }

                    break;
                }
                }
            }

            if (vStart != vLast)
            {
                m_tags.push_back(OutlineTag::Line);
                m_points.emplace_back(vStart);
            }
        }

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
