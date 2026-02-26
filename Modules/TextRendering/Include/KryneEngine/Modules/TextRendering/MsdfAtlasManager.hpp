/**
 * @file
 * @author Max Godefroy
 * @date 04/01/2026.
 */

#pragma once

#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Threads/SpinLock.hpp>
#include <KryneEngine/Modules/GraphicsUtils/Allocators/AtlasShelfAllocator.hpp>
#include <moodycamel/concurrentqueue.h>

namespace KryneEngine::Modules::TextRendering
{
    class Font;
    class FontManager;

    class MsdfAtlasManager
    {
    public:
        MsdfAtlasManager(
            AllocatorInstance _allocator,
            GraphicsContext* _graphicsContext,
            FontManager* _fontManager,
            u32 _atlasSize,
            u32 _glyphBaseSize);

        ~MsdfAtlasManager();

        struct GlyphRegion
        {
            u16 m_x = 0;
            u16 m_y = 0;
            u16 m_width = 0;
            u16 m_height = 0;
            u16 m_baseline = 0;
            u16 m_pxRange = 0;

            [[nodiscard]] bool IsValid() const { return m_pxRange > 0; }
        };

        GlyphRegion GetGlyphRegion(Font* _font, u32 _unicodeCodepoint, u32 _fontSize = 0);

        void FlushLoads(GraphicsContext& _graphicsContext, CommandListHandle _transfer);

        FontManager* GetFontManager() const { return m_fontManager; }

        TextureViewHandle GetAtlasView() const { return m_atlasView; }

        static u16 GetPxRange(u32 _fontSize);

        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

    private:
        struct StagingBuffer
        {
            BufferHandle m_buffer {};
            u32 m_size = 0;
        };

        struct GlyphKey
        {
            Font* m_font;
            u32 m_unicodeCodepoint;

            bool operator==(const GlyphKey& _other) const noexcept
            {
                return m_font == _other.m_font && m_unicodeCodepoint == _other.m_unicodeCodepoint;
            }

            bool operator<(const GlyphKey& _other) const noexcept
            {
                return m_font < _other.m_font || (m_font == _other.m_font && m_unicodeCodepoint < _other.m_unicodeCodepoint);
            }
        };

        struct GlyphSlot
        {
            u16 m_offsetX = 0;
            u16 m_offsetY = 0;
            u16 m_width = 0;
            u16 m_height = 0;
            u16 m_baseline = 0;
            u16 m_fontSize = 0;
            u32 m_allocatorSlot = 0;
        };

        struct GlyphLoadRequest
        {
            GlyphSlot m_slot {};
            Rect m_dstRegion {};
            float* m_buffer = nullptr;
        };

        AllocatorInstance m_allocator;
        FontManager* m_fontManager;
        GraphicsUtils::AtlasShelfAllocator m_atlasAllocator;
        DynamicArray<StagingBuffer> m_stagingBuffers;
        TextureHandle m_atlasTexture {};
        SubResourceIndexing m_atlasTextureSubresourceIndex {};
        TextureMemoryFootprint m_atlasFootprint {};
        u32 m_atlasSize;
        SpinLock m_lock {};
        eastl::vector_map<GlyphKey, GlyphSlot> m_glyphSlotMap;
        moodycamel::ConcurrentQueue<GlyphLoadRequest> m_loadQueue;
        TextureViewHandle m_atlasView {};
    };
}
