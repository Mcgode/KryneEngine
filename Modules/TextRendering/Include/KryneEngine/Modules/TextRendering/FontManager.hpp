/**
 * @file
 * @author Max Godefroy
 * @date 03/01/2026.
 */

#pragma once

#include <EASTL/string_view.h>
#include <EASTL/vector.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Modules/Resources/IResourceManager.hpp>

#include "KryneEngine/Modules/TextRendering/SystemFont.hpp"

struct FT_LibraryRec_;

namespace KryneEngine::Modules::TextRendering
{
    class Font;

    class FontManager final: public Resources::IResourceManager
    {
    public:
        explicit FontManager(AllocatorInstance _allocator);

        ~FontManager() override;

        eastl::span<std::byte> LoadResource(Resources::ResourceEntry* _entry, eastl::string_view _path) override;

        void FinalizeResourceLoading(
            Resources::ResourceEntry* _entry,
            eastl::span<std::byte> _loadedResourceData,
            eastl::string_view _path) override;

        void ReportFailedLoad(Resources::ResourceEntry* _entry, eastl::string_view _path) override;

        [[nodiscard]] AllocatorInstance GetAllocator() const override { return m_allocator; }

        SystemFont& GetSystemFont() { return m_systemFont; }
        [[nodiscard]] Font* GetFont(u16 _fontId) const;

    private:
        AllocatorInstance m_allocator;
        SystemFont m_systemFont;
        eastl::vector<Font*> m_fonts;
        FT_LibraryRec_* m_ftLibrary {};
    };
}
