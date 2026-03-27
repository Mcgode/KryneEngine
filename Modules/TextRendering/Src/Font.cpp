/**
 * @file
 * @author Max Godefroy
 * @date 03/01/2026.
 */

#include "KryneEngine/Modules/TextRendering/Font.hpp"

#include <ft2build.h>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include FT_FREETYPE_H

#include "KryneEngine/Modules/TextRendering/FontManager.hpp"
#include "KryneEngine/Modules/TextRendering/MsdfAtlasManager.hpp"
#include "KryneEngine/Modules/TextRendering/Utils/MsdfGenFunctionHelpers.hpp"

namespace KryneEngine::Modules::TextRendering
{
    Font::~Font()
    {
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            m_freetypeFile.Destroy(m_fileBufferAllocator);
            break;
        case FontFileType::PreBaked:
            m_preBakedFile.Destroy(m_fileBufferAllocator);
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
        case FontFileType::PreBaked:
            return m_preBakedFile.GetAscender(_fontSize);
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
        case FontFileType::PreBaked:
            return m_preBakedFile.GetDescender(_fontSize);
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
        case FontFileType::PreBaked:
            return m_preBakedFile.GetLineHeight(_fontSize);
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
        case FontFileType::PreBaked:
            advance = m_preBakedFile.GetHorizontalAdvance(_unicodeCodepoint, _fontSize);
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
        case FontFileType::PreBaked:
            metrics = m_preBakedFile.GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize);
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

    GlyphMsdfBitmap Font::GetMsdf(
        const u32 _unicodeCodepoint,
        const u16 _fontSize,
        AllocatorInstance _allocator)
    {
        bool hasOutline = false;
        switch (m_fileType)
        {
        case FontFileType::Freetype:
            hasOutline = m_freetypeFile.HasOutline(_unicodeCodepoint);
            break;
        case FontFileType::PreBaked:
            {
                const eastl::optional<u32> glyphIndex = m_preBakedFile.GetGlyphIndex(_unicodeCodepoint);
                if (glyphIndex.has_value() && m_preBakedFile.HasMsdfBitmaps())
                {
                    return m_preBakedFile.GetMsdfBitmap(*glyphIndex, _allocator);
                }
                hasOutline = glyphIndex.has_value() && m_preBakedFile.HasOutlines();
            }
        }

        if (!hasOutline)
        {
            if (IsNoFallback())
                return {};

            if (IsSystemFontFallback())
            {
                const u16 pxRange = MsdfAtlasManager::GetPxRange(_fontSize);
                return m_resourceManager->GetSystemFont().GenerateMsdf(_unicodeCodepoint, _fontSize, pxRange, _allocator);
            }

            Font* font = m_resourceManager->GetFont(m_fallbackFontId);
            return font != nullptr
                ? font->GetMsdf(_unicodeCodepoint, _fontSize, _allocator)
                : GlyphMsdfBitmap {};
        }

        KE_ZoneScopedF("Generate MSDF for U+%x", _unicodeCodepoint);

        msdfgen::Shape shape;
        {
            KE_ZoneScoped("Retrieve shape");

            GlyphShape glyphShape;
            switch (m_fileType)
            {
            case FontFileType::Freetype:
                glyphShape = m_freetypeFile.AcquireGlyphShape(_unicodeCodepoint);
                break;
            case FontFileType::PreBaked:
                glyphShape = eastl::move(m_preBakedFile.GetGlyphShape(_unicodeCodepoint, _allocator));
                break;
            }

            MsdfGen::LoadShape(shape, glyphShape);

            switch (m_fileType)
            {
            case FontFileType::Freetype:
                m_freetypeFile.ReleaseGlyphShape(_unicodeCodepoint, glyphShape);
                break;
            case FontFileType::PreBaked:
                m_preBakedFile.ReleaseGlyphShape(glyphShape, _allocator);
                break;
            }
        }

        const GlyphLayoutMetrics metrics = GetGlyphLayoutMetrics(_unicodeCodepoint, _fontSize);
        return MsdfGen::GenerateMsdf(
            shape,
            metrics,
            _fontSize,
            MsdfAtlasManager::GetPxRange(_fontSize),
            _allocator);
    }

    Font::Font(AllocatorInstance _allocator, FontManager* _fontManager, size_t _version)
        : ResourceBase(_allocator, _fontManager, _version)
    {
    }
} // namespace KryneEngine::Modules::TextRendering