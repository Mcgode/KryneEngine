/**
 * @file
 * @author Max Godefroy
 * @date 02/01/2026.
 */

#pragma once

#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>

using namespace KryneEngine;

class TextureGenerator
{
public:
    TextureGenerator(AllocatorInstance _allocator, size_t _textureCount);

    void HandleUpload(GraphicsContext& _graphicsContext, CommandListHandle _transfer);

    [[nodiscard]] TextureViewHandle GetTextureView(size_t _index) const;

    void Destroy(GraphicsContext& _graphicsContext);

private:
    static constexpr size_t kTextureDimensions = 16;

    DynamicArray<TextureHandle> m_textures;
    DynamicArray<TextureViewHandle> m_textureViews;
    DynamicArray<BufferHandle> m_stagingBuffers;
    u64 m_uploadFrame = ~0ull;
};
