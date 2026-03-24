/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2026.
 */

#include "KryneEngine/Modules/TextRendering/PreBakedFontFile.hpp"

#include <KryneEngine/Core/Common/Utils/Alignment.hpp>

namespace KryneEngine::Modules::TextRendering
{
    PreBakedFontFile::PreBakedFontFile(const eastl::span<std::byte> _data)
        : m_data(_data)
    {}

    eastl::span<std::byte> PreBakedFontFile::Bake(
        const BakedRenderInfo _renderInfo,
        const FontMetrics _fontMetrics,
        const eastl::span<const GlyphBakeInfo> _glyphs,
        const eastl::span<float2> _outlinePoints,
        const eastl::span<OutlineTag> _outlineTags,
        const AllocatorInstance _allocator)
    {
        size_t totalSize = 0;

        totalSize += sizeof(Header);

        totalSize += _glyphs.size() * sizeof(GlyphEntry);

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            totalSize += _glyphs.size() * sizeof(MsdfEntry);
            for (const auto& glyph : _glyphs)
                totalSize += Alignment::AlignUp<size_t>(glyph.m_msdfBitmap.size(), 4ull);
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            totalSize += _glyphs.size() * sizeof(OutlineEntry);

            for (const auto& glyph : _glyphs)
            {
                size_t pointsCount = 0;
                for (const OutlineTag tag: eastl::span(_outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount))
                {
                    switch (tag)
                    {
                    case OutlineTag::NewContour:
                    case OutlineTag::Line:
                        pointsCount += 1;
                        break;
                    case OutlineTag::Conic:
                        pointsCount += 2;
                        break;
                    case OutlineTag::Cubic:
                        pointsCount += 3;
                        break;
                    }
                }
                totalSize += pointsCount * sizeof(float2);
                totalSize += Alignment::AlignUp<size_t>(glyph.m_outlineTagCount * sizeof(OutlineTag), 4ull);
            }
        }

        auto* bytes = _allocator.Allocate<std::byte>(totalSize);
        const eastl::span bytesSpan { bytes, totalSize };

        *reinterpret_cast<Header*>(bytes) = Header {
            .m_magicNumber = kMagicNumber,
            .m_fontMetrics = _fontMetrics,
            .m_renderInfo = _renderInfo,
            .m_glyphCount = static_cast<u32>(_glyphs.size())
        };
        bytes += sizeof(Header);

        for (const auto& glyph : _glyphs)
        {
            *reinterpret_cast<GlyphEntry*>(bytes) = glyph.m_glyph;
            bytes += sizeof(GlyphEntry);
        }

        MsdfEntry* msdfEntries = nullptr;
        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            msdfEntries = reinterpret_cast<MsdfEntry*>(bytes);
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                msdfEntries[i] = {
                    .m_offset = 0,
                    .m_glyphWidth = _glyphs[i].m_msdfWidth,
                    .m_glyphHeight = _glyphs[i].m_msdfHeight,
                    .m_baseline = _glyphs[i].m_msdfBaseline,
                };
            }
            bytes += _glyphs.size() * sizeof(MsdfEntry);
        }

        OutlineEntry* outlineEntries = nullptr;
        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            outlineEntries = reinterpret_cast<OutlineEntry*>(bytes);
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                outlineEntries[i] = {
                    .m_pointsOffset = 0,
                    .m_tagsOffset = 0,
                    .m_tagsCount = _glyphs[i].m_outlineTagCount,
                };
            }
            bytes += _glyphs.size() * sizeof(OutlineEntry);
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                const auto& glyph = _glyphs[i];
                memcpy(bytes, glyph.m_msdfBitmap.data(), glyph.m_msdfBitmap.size());
                msdfEntries[i].m_offset = bytes - bytesSpan.data();
                bytes += Alignment::AlignUp<size_t>(glyph.m_msdfBitmap.size(), 4ull);
            }
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                const auto& glyph = _glyphs[i];
                outlineEntries[i].m_pointsOffset = bytes - bytesSpan.data();

                size_t pointsCount = 0;
                for (const OutlineTag tag: eastl::span(_outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount))
                {
                    switch (tag)
                    {
                    case OutlineTag::NewContour:
                    case OutlineTag::Line:
                        pointsCount += 1;
                        break;
                    case OutlineTag::Conic:
                        pointsCount += 2;
                        break;
                    case OutlineTag::Cubic:
                        pointsCount += 3;
                        break;
                    }
                }
                memcpy(bytes, _outlinePoints.begin() + glyph.m_outlineStartPoint, pointsCount * sizeof(float2));
                bytes += pointsCount * sizeof(float2);

                outlineEntries[i].m_tagsOffset = bytes - bytesSpan.data();
                memcpy(bytes, _outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount * sizeof(OutlineTag));
                bytes += Alignment::AlignUp<size_t>(glyph.m_outlineTagCount * sizeof(OutlineTag), 4ull);
            }
        }

        KE_ASSERT(bytes == bytesSpan.end());

        return bytesSpan;
    }
}
