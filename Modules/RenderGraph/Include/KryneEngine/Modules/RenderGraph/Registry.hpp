/**
 * @file
 * @author Max Godefroy
 * @date 14/11/2024.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"
#include <KryneEngine/Core/Memory/SimplePool.hpp>

namespace KryneEngine
{
    class GraphicsContext;
    struct TextureCreateDesc;
    struct TextureViewDesc;
}

namespace KryneEngine::Modules::RenderGraph
{
    struct RenderTargetViewDesc;

    class Registry
    {
        friend class Builder;

    public:
        Registry();
        ~Registry();

    public:
        SimplePoolHandle RegisterRawTexture(
            TextureHandle _texture,
            u16 _arraySize = RawTextureData::kNoArrayPartialIndexing,
            u8 _mipCount = RawTextureData::kNoMipPartialIndexing,
            const eastl::string_view& _name = "");
        SimplePoolHandle RegisterRawBuffer(BufferHandle _buffer, const eastl::string_view& _name = "");
        SimplePoolHandle RegisterTextureView(
            TextureViewHandle _textureView,
            SimplePoolHandle _textureResource,
            const TextureSubResourceRange& _range = {},
            const eastl::string_view& _name = {});
        SimplePoolHandle RegisterBufferView(
            BufferViewHandle _bufferView,
            SimplePoolHandle _bufferResource,
            const eastl::string_view& _name = {});
        SimplePoolHandle RegisterRenderTargetView(
            RenderTargetViewHandle _rtv,
            SimplePoolHandle _textureResource,
            const TextureSubResourceRange& _range = {},
            const eastl::string_view& _name = {});

        SimplePoolHandle RegisterDummy(const eastl::string_view& _name = {});

        SimplePoolHandle CreateRawTexture(
            GraphicsContext* _graphicsContext,
            const TextureCreateDesc& _desc);
        SimplePoolHandle CreateRenderTargetView(
            GraphicsContext* _graphicsContext,
            const RenderTargetViewDesc& _desc,
            eastl::string_view _name = {});
        SimplePoolHandle CreateTextureView(
            GraphicsContext* _graphicsContext,
            SimplePoolHandle _texture,
            const KryneEngine::TextureViewDesc& _desc,
            eastl::string_view _name = {});

        [[nodiscard]] SimplePoolHandle GetUnderlyingResource(SimplePoolHandle _resource) const;

        [[nodiscard]] const Resource& GetResource(SimplePoolHandle _resource) const;

        [[nodiscard]] bool IsRenderTargetView(SimplePoolHandle _resource) const;
        [[nodiscard]] RenderTargetViewHandle GetRenderTargetView(SimplePoolHandle _resource) const;
        [[nodiscard]] TextureViewHandle GetTextureView(SimplePoolHandle _resource) const;

    private:
        SimplePool<Resource, void, true> m_resources;
    };
} // namespace KryneEngine::Modules::RenderGraph

