/**
 * @file
 * @author Max Godefroy
 * @date 03/01/2026.
 */

#pragma once

#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Modules/Resources/ResourceBase.hpp>
#include <KryneEngine/Modules/Resources/ResourceTypeId.hpp>

#include "KryneEngine/Modules/TextRendering/FontCommon.hpp"
#include "KryneEngine/Modules/TextRendering/FontManager.hpp"
#include "KryneEngine/Modules/TextRendering/FontFiles/FreetypeFontFile.hpp"
#include "KryneEngine/Modules/TextRendering/FontFiles/PreBakedFontFile.hpp"

struct FT_FaceRec_;

namespace KryneEngine::Modules::TextRendering
{
    class Font final: public Resources::ResourceBase<FontManager>
    {
        friend FontManager;

    public:
        ~Font() override;

        static constexpr Resources::ResourceTypeId kTypeId = Resources::GenerateResourceTypeId("Font");

        float GetAscender(float _fontSize) const;
        float GetDescender(float _fontSize) const;
        float GetLineHeight(float _fontSize) const;

        float GetHorizontalAdvance(u32 _unicodeCodepoint, float _fontSize);
        GlyphLayoutMetrics GetGlyphLayoutMetrics(u32 _unicodeCodepoint, float _fontSize);

        float* GenerateMsdf(u32 _unicodeCodepoint, float _fontSize, u16 _pxRange, AllocatorInstance _allocator);

        void SetFallbackFont(const Font* _fallbackFont) { m_fallbackFontId = _fallbackFont->GetId(); }
        void SetFallbackSystemFont() { m_fallbackFontId = kSystemFontFallback; }
        void SetNoFallback() { m_fallbackFontId = kNoFallback; }

        [[nodiscard]] bool IsNoFallback() const { return m_fallbackFontId == kNoFallback; }
        [[nodiscard]] bool IsSystemFontFallback() const { return m_fallbackFontId == kSystemFontFallback; }
        [[nodiscard]] u16 GetFallbackFont() const { return m_fallbackFontId; }

        [[nodiscard]] u16 GetId() const { return m_fontId; }

    private:
        explicit Font(AllocatorInstance _allocator, FontManager* _fontManager, size_t _version);

        enum class FontFileType: u8
        {
            Freetype,
        };

        static constexpr u32 kSystemFontFallback = 0x10000;
        static constexpr u32 kNoFallback = 0x20000;

        u16 m_fontId = 0;
        FontFileType m_fileType = FontFileType::Freetype;
        union
        {
            FreetypeFontFile m_freetypeFile;
        };
        AllocatorInstance m_fileBufferAllocator {};
        u32 m_fallbackFontId = kSystemFontFallback;
    };
}
