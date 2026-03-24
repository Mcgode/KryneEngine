/**
 * @file
 * @author Max Godefroy
 * @date 03/01/2026.
 */

#include "KryneEngine/Modules/TextRendering/Font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include "KryneEngine/Modules/TextRendering/FontManager.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <msdfgen.h>

namespace KryneEngine::Modules::TextRendering
{
    Font::~Font()
    {
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            m_freetypeFile.Destroy(m_fileBufferAllocator);
            break;
        default:
            KE_ERROR("Unreachable code (unsupported font type: %d)", static_cast<int>(m_fileType));
        }
    }

    float Font::GetAscender(const float _fontSize) const
    {
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            return m_freetypeFile.GetAscender(_fontSize);
        default:
            KE_ERROR("Unreachable code (unsupported font type: %d)", static_cast<int>(m_fileType));
            return 0;
        }
    }

    float Font::GetDescender(const float _fontSize) const
    {
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            return m_freetypeFile.GetDescender(_fontSize);
        default:
            KE_ERROR("Unreachable code (unsupported font type: %d)", static_cast<int>(m_fileType));
            return 0;
        }
    }

    float Font::GetLineHeight(const float _fontSize) const
    {
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            return m_freetypeFile.GetLineHeight(_fontSize);
        default:
            KE_ERROR("Unreachable code (unsupported font type: %d)", static_cast<int>(m_fileType));
            return 0;
        }
    }

    float Font::GetHorizontalAdvance(const u32 _unicodeCodepoint, const float _fontSize)
    {
        eastl::optional<float> advance;
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            advance = m_freetypeFile.GetHorizontalAdvance(_unicodeCodepoint, _fontSize);
            break;
        }

        if (advance.has_value())
            return advance.value();

        if (IsNoFallback())
            return 0;
        if (IsSystemFontFallback())
        {
            return m_resourceManager->GetSystemFont().GetHorizontalAdvance(_unicodeCodepoint, _fontSize);
        }

        Font* font = m_resourceManager->GetFont(m_fallbackFontId);
        return font != nullptr ? font->GetHorizontalAdvance(_unicodeCodepoint, _fontSize) : 0;
    }

    GlyphLayoutMetrics Font::GetGlyphLayoutMetrics(u32 _unicodeCodepoint, float _fontSize)
    {
        eastl::optional<GlyphLayoutMetrics> metrics;
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            metrics = m_freetypeFile.GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize);
            break;
        }

        if (metrics.has_value())
            return metrics.value();

        if (IsNoFallback())
            return { 0, 0, 0, 0, 0 };
        if (IsSystemFontFallback())
        {
            return m_resourceManager->GetSystemFont().GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize);
        }

        Font* font = m_resourceManager->GetFont(m_fallbackFontId);
        return font != nullptr
            ? font->GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize)
            : GlyphLayoutMetrics { 0, 0, 0, 0, 0 };
    }

    float* Font::GenerateMsdf(
        const u32 _unicodeCodepoint,
        const float _fontSize,
        const u16 _pxRange,
        AllocatorInstance _allocator)
    {
        bool hasOutline = false;
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            hasOutline = m_freetypeFile.HasOutline(_unicodeCodepoint);
            break;
        }

        if (!hasOutline)
        {
            if (IsNoFallback())
                return nullptr;

            if (IsSystemFontFallback())
                return m_resourceManager->GetSystemFont().GenerateMsdf(_unicodeCodepoint, _fontSize, _pxRange, _allocator);

            Font* font = m_resourceManager->GetFont(m_fallbackFontId);
            return font != nullptr
                ? font->GenerateMsdf(_unicodeCodepoint, _fontSize, _pxRange, _allocator)
                : nullptr;

        }

        KE_ZoneScopedF("Generate MSDF for U+%x", _unicodeCodepoint);

        GlyphLayoutMetrics metrics = GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize);


        msdfgen::Vector2 scale = 1;
        msdfgen::Vector2 translate = 0;

        const auto pxRange = static_cast<double>(_pxRange);
        const auto glyphWidth = metrics.m_width;
        const auto glyphXMin = metrics.m_bearingX;
        const auto glyphHeight = metrics.m_height;
        const auto glyphYBearing = metrics.m_bearingY;

        const double baseLineYOffset = std::ceil(glyphHeight - glyphYBearing);

        const uint2 finalGlyphDims {
            std::ceil(glyphWidth) + static_cast<float>(_pxRange),
            baseLineYOffset + std::ceil(glyphYBearing) + _pxRange
        };

        scale = _fontSize;
        translate.set(
            -static_cast<double>(glyphXMin / _fontSize),
            baseLineYOffset / _fontSize);
        translate += (pxRange * 0.5) / scale;

        msdfgen::Shape shape;
        {
            KE_ZoneScoped("Retrieve shape");

            GlyphShape glyphShape;
            switch (m_fileType)
            {
            case FontFileType::Freetype:
                glyphShape = m_freetypeFile.AcquireGlyphShape(_unicodeCodepoint);
                break;
            }

            const float2* pPoints = glyphShape.m_points;
            const OutlineTag* pTags = glyphShape.m_tags.begin();
            const OutlineTag* pTagsEnd = glyphShape.m_tags.end();

            msdfgen::Vector2 currentPoint;

            for (; pTags < pTagsEnd; pTags++)
            {
                switch (*pTags)
                {
                case OutlineTag::NewContour:
                    shape.addContour();
                    currentPoint.set(pPoints->x, pPoints->y);
                    pPoints++;
                    break;
                case OutlineTag::Line:
                {
                    const msdfgen::Vector2 nextPoint(pPoints->x, pPoints->y);
                    shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, nextPoint));
                    currentPoint = nextPoint;
                    pPoints++;
                    break;
                }
                case OutlineTag::Conic:
                {
                    const msdfgen::Vector2 controlPoint(pPoints[0].x, pPoints[0].y);
                    const msdfgen::Vector2 nextPoint(pPoints[1].x, pPoints[1].y);
                    shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, controlPoint, nextPoint));
                    currentPoint = nextPoint;
                    pPoints += 2;
                    break;
                }
                case OutlineTag::Cubic:
                {
                    const msdfgen::Vector2 controlPoint0(pPoints[0].x, pPoints[0].y);
                    const msdfgen::Vector2 controlPoint1(pPoints[1].x, pPoints[1].y);
                    const msdfgen::Vector2 nextPoint(pPoints[2].x, pPoints[2].y);
                    shape.contours.back().addEdge(msdfgen::EdgeHolder(currentPoint, controlPoint0, controlPoint1, nextPoint));
                    currentPoint = nextPoint;
                    pPoints += 3;
                    break;
                }
                }
            }

            switch (m_fileType)
            {
            case FontFileType::Freetype:
                m_freetypeFile.ReleaseGlyphShape(_unicodeCodepoint, glyphShape);
                break;
            }
        }

        KE_ASSERT(shape.validate());

        msdfgen::edgeColoringByDistance(shape, 3);

        const msdfgen::SDFTransformation transformation {
            msdfgen::Projection(scale, translate),
            msdfgen::Range(_pxRange / scale.x)
        };
        auto* pixels = _allocator.Allocate<float>(3 * finalGlyphDims.x * finalGlyphDims.y);
        const msdfgen::BitmapSection<float, 3> bitmapSection {
            pixels,
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
            shape,
            transformation,
            generatorConfig);

        return pixels;
    }

    Font::Font(AllocatorInstance _allocator, FontManager* _fontManager, size_t _version)
        : ResourceBase(_allocator, _fontManager, _version)
    {
    }
} // namespace KryneEngine::Modules::TextRendering