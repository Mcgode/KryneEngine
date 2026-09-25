/**
 * @file
 * @author Max Godefroy
 * @date 15/11/2024.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Handles.hpp"
#include <KryneEngine/Core/Memory/SimplePool.hpp>

namespace KryneEngine::Modules::RenderGraph
{
    enum class ResourceType
    {
        RawTexture,
        Buffer,
        Sampler,
        TextureView,
        BufferView,
        RenderTargetView,
        Dummy,
    };

    /// @brief A range of a texture's sub-resources (array layers x mips) covered by a view.
    struct TextureSubResourceRange
    {
        static constexpr u16 kAllArrayLayers = 0xff'ff;
        static constexpr u8 kAllMipLevels = 0xff;

        u16 m_arrayStart = 0;
        u16 m_arrayCount = kAllArrayLayers;
        u8 m_mipStart = 0;
        u8 m_mipCount = kAllMipLevels;

        [[nodiscard]] bool IsPartial() const
        {
            return m_arrayStart != 0 || m_arrayCount != kAllArrayLayers || m_mipStart != 0 || m_mipCount != kAllMipLevels;
        }
    };

    struct RawTextureData
    {
        TextureHandle m_texture;

        static constexpr u16 kNoArrayPartialIndexing = 0;
        static constexpr u8 kNoMipPartialIndexing = 0;

        // Full extent of the texture, used to size per-sub-resource state tracking.
        u16 m_arraySize = kNoArrayPartialIndexing;
        u8 m_mipCount = kNoMipPartialIndexing;
    };

    struct BufferData
    {
        BufferHandle m_buffer;
    };

    struct SamplerData
    {
        SamplerHandle m_sampler;
    };

    struct TextureViewData
    {
        TextureViewHandle m_textureView;
        SimplePoolHandle m_textureResource;
        TextureSubResourceRange m_range;
    };

    struct BufferViewData
    {
        BufferViewHandle m_bufferView;
        SimplePoolHandle m_bufferResource;
    };

    struct RenderTargetViewData
    {
        RenderTargetViewHandle m_renderTargetView;
        SimplePoolHandle m_textureResource;
        TextureSubResourceRange m_range;
    };

    struct Resource
    {
        ResourceType m_type;
        bool m_owned;
        union {
            RawTextureData m_rawTextureData;
            BufferData m_bufferData;
            SamplerData m_samplerData;
            TextureViewData m_textureViewData;
            BufferViewData m_bufferViewData;
            RenderTargetViewData m_renderTargetViewData;
        };
#if !defined(KE_FINAL)
        eastl::string m_name;
#endif

        [[nodiscard]] bool IsTexture() const
        {
            switch (m_type)
            {
                case ResourceType::RawTexture:
                case ResourceType::TextureView:
                case ResourceType::RenderTargetView:
                    return true;
                default:
                    return false;
            }
        }

        [[nodiscard]] bool IsBuffer() const
        {
            switch (m_type)
            {
                case ResourceType::Buffer:
                case ResourceType::BufferView:
                    return true;
                default:
                    return false;
            }
        }

        /// @brief The sub-resource range this resource covers on its underlying texture.
        /// A raw texture always covers its own full extent; a view covers whatever range it was
        /// created/registered with.
        [[nodiscard]] TextureSubResourceRange GetTextureSubResourceRange() const
        {
            switch (m_type)
            {
                case ResourceType::TextureView:
                    return m_textureViewData.m_range;
                case ResourceType::RenderTargetView:
                    return m_renderTargetViewData.m_range;
                case ResourceType::RawTexture:
                default:
                    return {};
            }
        }
    };
} // namespace KryneEngine::Modules::RenderGraph
