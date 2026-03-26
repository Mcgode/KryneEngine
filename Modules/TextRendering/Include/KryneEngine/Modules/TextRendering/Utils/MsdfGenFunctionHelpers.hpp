/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#pragma once

#include <msdfgen.h>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"

namespace KryneEngine::Modules::TextRendering::MsdfGen
{
    inline void LoadShape(msdfgen::Shape& _shape, const GlyphShape& _glyphShape)
    {
        const float2* pPoints = _glyphShape.m_points;
        const OutlineTag* pTags = _glyphShape.m_tags.begin();
        const OutlineTag* pTagsEnd = _glyphShape.m_tags.end();

        msdfgen::Vector2 currentPoint;

        for (; pTags < pTagsEnd; pTags++)
        {
            switch (*pTags)
            {
            case OutlineTag::NewContour:
                _shape.addContour();
                currentPoint.set(pPoints->x, pPoints->y);
                pPoints++;
                break;
            case OutlineTag::Line:
                {
                    const msdfgen::Vector2 nextPoint(pPoints->x, pPoints->y);
                    _shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, nextPoint));
                    currentPoint = nextPoint;
                    pPoints++;
                    break;
                }
            case OutlineTag::Conic:
                {
                    const msdfgen::Vector2 controlPoint(pPoints[0].x, pPoints[0].y);
                    const msdfgen::Vector2 nextPoint(pPoints[1].x, pPoints[1].y);
                    _shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, controlPoint, nextPoint));
                    currentPoint = nextPoint;
                    pPoints += 2;
                    break;
                }
            case OutlineTag::Cubic:
                {
                    const msdfgen::Vector2 controlPoint0(pPoints[0].x, pPoints[0].y);
                    const msdfgen::Vector2 controlPoint1(pPoints[1].x, pPoints[1].y);
                    const msdfgen::Vector2 nextPoint(pPoints[2].x, pPoints[2].y);
                    _shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, controlPoint0, controlPoint1, nextPoint));
                    currentPoint = nextPoint;
                    pPoints += 3;
                    break;
                }
            }
        }
    }

    inline GlyphMsdfBitmap GenerateMsdf(
        msdfgen::Shape& _shape,
        const GlyphLayoutMetrics& _metrics,
        const u16 _fontSize,
        const u16 _pxRange,
        const AllocatorInstance _allocator)
    {
        KE_ASSERT(_shape.validate());

        const double baseLineYOffset = std::ceil(_metrics.m_height - _metrics.m_bearingY);

        const auto pxRange = static_cast<double>(_pxRange);
        const uint2 finalGlyphDims {
            std::ceil(_metrics.m_width) + pxRange,
            baseLineYOffset + std::ceil(_metrics.m_bearingY) + _pxRange
        };

        msdfgen::edgeColoringByDistance(_shape, 3);

        const msdfgen::Vector2 scale {static_cast<double>(_fontSize) };
        const msdfgen::Vector2 translate {
            (-_metrics.m_bearingX + 0.5 * pxRange) / _fontSize,
            (baseLineYOffset + 0.5 * pxRange) / _fontSize
        };

        const msdfgen::SDFTransformation transformation {
            msdfgen::Projection(scale, translate),
            msdfgen::Range(_pxRange / scale.x)
        };
        auto* floatPixels = _allocator.Allocate<float>(3 * finalGlyphDims.x * finalGlyphDims.y);
        const msdfgen::BitmapSection<float, 3> bitmapSection {
            floatPixels,
            static_cast<s32>(finalGlyphDims.x),
            static_cast<s32>(finalGlyphDims.y),
            msdfgen::Y_DOWNWARD
        };
        msdfgen::MSDFGeneratorConfig generatorConfig {
            true,
            msdfgen::ErrorCorrectionConfig { msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY }
        };
        msdfgen::generateMSDF(
            bitmapSection,
            _shape,
            transformation,
            generatorConfig);

        auto* pixels = _allocator.Allocate<std::byte>(4 * finalGlyphDims.x * finalGlyphDims.y);
        for (u32 y = 0; y < finalGlyphDims.y; ++y)
        {
            for (u32 x = 0; x < finalGlyphDims.x; ++x)
            {
                for (u32 i = 0; i < 3; ++i)
                {
                    const size_t index = 3 * (x + y * finalGlyphDims.x) + i;
                    const float pixel = floatPixels[index];
                    pixels[index] = static_cast<std::byte>(std::round(std::clamp(pixel, 0.f, 1.f) * 255.0f));
                }
            }
        }
        _allocator.deallocate(floatPixels, 3 * finalGlyphDims.x * finalGlyphDims.y * sizeof(float));

        return {
            .m_bitmap = { pixels, 3 * finalGlyphDims.x * finalGlyphDims.y },
            .m_pxRange = _pxRange,
            .m_width = static_cast<u16>(finalGlyphDims.x),
            .m_height = static_cast<u16>(finalGlyphDims.y),
            .m_fontSize = _fontSize,
            .m_baseLine = static_cast<u16>(std::ceil(_metrics.m_bearingY) + static_cast<float>(_pxRange) * 0.5f),
        };
    }
}