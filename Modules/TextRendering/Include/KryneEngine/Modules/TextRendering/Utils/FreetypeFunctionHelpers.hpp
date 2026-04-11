/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Modules::TextRendering::Freetype
{
    inline s32 SelectBestUnicodeCharmap(FT_Face _face)
    {
        s32 bestCharMap = -1;
        s32 bestPriority = eastl::numeric_limits<s32>::max();
        for (s32 charMapIndex = 0; charMapIndex < _face->num_charmaps; ++charMapIndex)
        {
            FT_CharMap const charMap = _face->charmaps[charMapIndex];
            if (charMap->encoding != FT_ENCODING_UNICODE)
            {
                continue;
            }

            s32 priority;
            if (charMap->platform_id == 3 && charMap->encoding_id == 10) // Microsoft UTF-32
                priority = 0;
            else if (charMap->platform_id == 3) // Apple
                priority = 10;
            else if (charMap->platform_id == 1 && charMap->encoding_id == 1) // Microsoft UTF-16
                priority = 20;
            else
                priority = 50 + charMapIndex; // Tie-breaker

            if (priority < bestPriority)
            {
                bestPriority = priority;
                bestCharMap = charMapIndex;
            }
        }

        return bestCharMap;
    }

    inline void LoadOutline(FT_Face _face, eastl::vector<float2>& _points, eastl::vector<OutlineTag>& _tags)
    {
        const FT_Outline outline = _face->glyph->outline;

        const float2_simd scale { 1.f / static_cast<float>(_face->units_per_EM) };

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
            u8* pTags = reinterpret_cast<u8*>(outline.tags) + start;
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
                _tags.push_back(OutlineTag::NewContour);
                _points.emplace_back(vStart);
            }

            while (pPoints < end)
            {
                pPoints++;
                pTags++;

                tag = FT_CURVE_TAG(*pTags);
                switch (tag)
                {
                case FT_CURVE_TAG_ON:
                    _tags.push_back(OutlineTag::Line);
                    _points.emplace_back(float2_simd(pPoints->x, pPoints->y) * scale);

                    // Close contour
                    if (pPoints == end)
                    {
                        _tags.push_back(OutlineTag::Line);
                        _points.emplace_back(vStart);
                    }
                    break;
                case FT_CURVE_TAG_CONIC:
                {
                    _tags.push_back(OutlineTag::Conic);

                    vControl = float2_simd(pPoints->x, pPoints->y) * scale;
                    _points.emplace_back(vControl);

                    if (pPoints < end)
                    {
                        tag = FT_CURVE_TAG(pTags[1]);
                        float2_simd vec = float2_simd(pPoints[1].x, pPoints[1].y) * scale;

                        if (tag == FT_CURVE_TAG_ON)
                        {
                            _points.emplace_back(vec);

                            // We consumed a point, advance.
                            pPoints++;
                            pTags++;
                        }
                        // We are chaining conic arcs, so we take the median point of the two consecutive control points
                        else if (tag == FT_CURVE_TAG_CONIC)
                        {
                            _points.emplace_back((vControl + vec) / float2_simd(2));
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
                        _points.emplace_back(vStart);
                    }

                    break;
                }
                default: // case FT_CURVE_TAG_CUBIC:
                {
                    KE_ASSERT_FATAL(pPoints + 1 <= end && FT_CURVE_TAG(pTags[1]) == FT_CURVE_TAG_CUBIC);

                    _tags.push_back(OutlineTag::Cubic);

                    float2_simd v1 = float2_simd(pPoints->x, pPoints->y) * scale;
                    pPoints++;
                    float2_simd v2 = float2_simd(pPoints->x, pPoints->y) * scale;
                    pPoints++;

                    _points.emplace_back(v1);
                    _points.emplace_back(v2);

                    if (pPoints <= end)
                    {
                        _points.emplace_back(float2_simd(pPoints->x, pPoints->y) * scale);
                    }
                    // Close contour
                    else
                    {
                        _points.emplace_back(vStart);
                    }

                    break;
                }
                }
            }

            if (vStart != vLast)
            {
                _tags.push_back(OutlineTag::Line);
                _points.emplace_back(vStart);
            }
        }
    }
}
