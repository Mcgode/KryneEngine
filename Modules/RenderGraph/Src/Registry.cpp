/**
 * @file
 * @author Max Godefroy
 * @date 14/11/2024.
 */

#include "KryneEngine/Modules/RenderGraph/Registry.hpp"

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include <KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Memory/SimplePool.inl>

#include "KryneEngine/Modules/RenderGraph/Descriptors/RenderTargetViewDesc.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    Registry::Registry() = default;
    Registry::~Registry() = default;

    SimplePoolHandle Registry::RegisterRawTexture(
        const TextureHandle _texture,
        const u16 _arraySize,
        const u8 _mipCount,
        const eastl::string_view& _name)
    {
        const SimplePoolHandle handle = m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::RawTexture,
            .m_owned = false,
            .m_rawTextureData = {
                .m_texture = _texture,
                .m_arraySize = _arraySize,
                .m_mipCount = _mipCount,
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
        return handle;
    }

    SimplePoolHandle Registry::RegisterRawBuffer(const BufferHandle _buffer, const eastl::string_view& _name)
    {
        const SimplePoolHandle handle = m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::Buffer,
            .m_owned = false,
            .m_bufferData = {
                .m_buffer = _buffer,
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
        return handle;
    }

    SimplePoolHandle Registry::RegisterTextureView(
        const TextureViewHandle _textureView,
        const SimplePoolHandle _textureResource,
        const TextureSubResourceRange& _range,
        const eastl::string_view& _name)
    {
        KE_ASSERT(m_resources.Get(_textureResource).m_type == ResourceType::RawTexture);

        // Add ref to underlying texture resource
        m_resources.AddRef(_textureResource);

        const SimplePoolHandle handle = m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::TextureView,
            .m_owned = false,
            .m_textureViewData = {
                .m_textureView = _textureView,
                .m_textureResource = _textureResource,
                .m_range = _range,
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
        return handle;
    }

    SimplePoolHandle Registry::RegisterBufferView(
        const BufferViewHandle _bufferView,
        const SimplePoolHandle _bufferResource,
        const eastl::string_view& _name)
    {
        KE_ASSERT(m_resources.Get(_bufferResource).m_type == ResourceType::Buffer);

        m_resources.AddRef(_bufferResource);
        const SimplePoolHandle handle = m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::BufferView,
            .m_owned = false,
            .m_bufferViewData = {
                .m_bufferView = _bufferView,
                .m_bufferResource = _bufferResource,
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
        return handle;
    }

    SimplePoolHandle Registry::RegisterRenderTargetView(
        const RenderTargetViewHandle _rtv,
        const SimplePoolHandle _textureResource,
        const TextureSubResourceRange& _range,
        const eastl::string_view& _name)
    {

        KE_ASSERT(m_resources.Get(_textureResource).m_type == ResourceType::RawTexture);

        // Add ref to underlying texture resource
        m_resources.AddRef(_textureResource);

        const SimplePoolHandle handle = m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::RenderTargetView,
            .m_owned = false,
            .m_renderTargetViewData = {
                .m_renderTargetView = _rtv,
                .m_textureResource = _textureResource,
                .m_range = _range,
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
        return handle;
    }

    SimplePoolHandle Registry::RegisterDummy(const eastl::string_view& _name)
    {
        return m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::Dummy,
            .m_owned = false,
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
    }

    SimplePoolHandle Registry::CreateRawTexture(
        GraphicsContext* _graphicsContext,
        const TextureCreateDesc& _desc)
    {
        const TextureHandle texture = _graphicsContext->CreateTexture(_desc);
        return m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::RawTexture,
            .m_owned = true,
            .m_rawTextureData = {
                .m_texture = texture,
                .m_arraySize = _desc.m_desc.m_arraySize,
                .m_mipCount = _desc.m_desc.m_mipCount,
            },
#if !defined(KE_FINAL)
            .m_name { _desc.m_desc.m_debugName, m_resources.GetAllocator() },
#endif
        });
    }

    SimplePoolHandle Registry::CreateRenderTargetView(
        GraphicsContext* _graphicsContext,
        const RenderTargetViewDesc& _desc,
        const eastl::string_view _name)
    {
        const Resource& resource = m_resources.Get(_desc.m_textureResource);
        VERIFY_OR_RETURN(resource.m_type == ResourceType::RawTexture, ~0ull);

        const KryneEngine::RenderTargetViewDesc desc {
            .m_texture = resource.m_rawTextureData.m_texture,
            .m_format = _desc.m_format,
            .m_type = _desc.m_type,
            .m_plane = _desc.m_plane,
            .m_arrayRangeStart = _desc.m_arrayRangeStart,
            .m_arrayRangeSize = _desc.m_arrayRangeSize,
            .m_mipLevel = _desc.m_mipLevel,
#if !defined(KE_FINAL)
            .m_debugName = _name.data(),
#endif
        };

        return m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::RenderTargetView,
            .m_owned = true,
            .m_renderTargetViewData = {
                .m_renderTargetView = _graphicsContext->CreateRenderTargetView(desc),
                .m_textureResource = _desc.m_textureResource,
                .m_range = {
                    .m_arrayStart = _desc.m_arrayRangeStart,
                    .m_arrayCount = _desc.m_arrayRangeSize,
                    .m_mipStart = _desc.m_mipLevel,
                    .m_mipCount = 1,
                },
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
    }

    SimplePoolHandle Registry::CreateTextureView(
        GraphicsContext* _graphicsContext,
        const SimplePoolHandle _texture,
        const TextureViewDesc& _desc,
        const eastl::string_view _name)
    {
        const Resource& resource = m_resources.Get(_texture);

        TextureViewDesc desc = _desc;
        desc.m_texture = resource.m_rawTextureData.m_texture;

        return m_resources.AllocateAndInit(Resource {
            .m_type = ResourceType::TextureView,
            .m_owned = true,
            .m_textureViewData = {
                .m_textureView = _graphicsContext->CreateTextureView(desc),
                .m_textureResource = _texture,
                .m_range = {
                    .m_arrayStart = desc.m_arrayStart,
                    .m_arrayCount = desc.m_arrayRange,
                    .m_mipStart = desc.m_minMip,
                    .m_mipCount = static_cast<u8>(desc.m_maxMip - desc.m_minMip + 1),
                },
            },
#if !defined(KE_FINAL)
            .m_name { _name, m_resources.GetAllocator() },
#endif
        });
    }

    SimplePoolHandle Registry::GetUnderlyingResource(const SimplePoolHandle _resource) const
    {
        const Resource& resource = m_resources.Get(_resource);

        switch (resource.m_type)
        {
        case ResourceType::TextureView:
            return resource.m_textureViewData.m_textureResource;
        case ResourceType::BufferView:
            return resource.m_bufferViewData.m_bufferResource;
        case ResourceType::RenderTargetView:
            return resource.m_renderTargetViewData.m_textureResource;
        case ResourceType::RawTexture:
        case ResourceType::Buffer:
        case ResourceType::Sampler:
        case ResourceType::Dummy:
            return _resource;
        }
        return ~0ull;
    }

    const Resource& Registry::GetResource(const SimplePoolHandle _resource) const
    {
        return m_resources.Get( _resource);
    }

    bool Registry::IsRenderTargetView(const SimplePoolHandle _resource) const
    {
        return m_resources.Get(_resource).m_type == ResourceType::RenderTargetView;
    }

    RenderTargetViewHandle Registry::GetRenderTargetView(const SimplePoolHandle _resource) const
    {
        const Resource& resource = m_resources.Get(_resource);
        VERIFY_OR_RETURN(resource.m_type == ResourceType::RenderTargetView, RenderTargetViewHandle { GenPool::kInvalidHandle });
        return resource.m_renderTargetViewData.m_renderTargetView;
    }

    TextureViewHandle Registry::GetTextureView(const SimplePoolHandle _resource) const
    {
        const Resource& resource = m_resources.Get(_resource);
        VERIFY_OR_RETURN(resource.m_type == ResourceType::TextureView, TextureViewHandle { GenPool::kInvalidHandle });
        return resource.m_textureViewData.m_textureView;
    }
} // namespace KryneEngine::Modules::RenderGraph

