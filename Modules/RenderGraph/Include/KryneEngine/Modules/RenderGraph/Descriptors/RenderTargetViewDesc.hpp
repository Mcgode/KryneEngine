/**
 * @file
 * @author Max Godefroy
 * @date 10/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Enums.hpp"
#include <KryneEngine/Core/Memory/SimplePool.hpp>

namespace KryneEngine::Modules::RenderGraph
{
    struct RenderTargetViewDesc
    {
        SimplePoolHandle m_textureResource = ~0ull;
        TextureFormat m_format = TextureFormat::NoFormat;
        TextureTypes m_type = TextureTypes::Single2D;
        TexturePlane m_plane = TexturePlane::Color;

        union {
            u16 m_arrayRangeStart = 0;
            u16 m_depthStartSlice;
        };
        union {
            u16 m_arrayRangeSize = 1;
            u16 m_depthSlicesSize;
        };

        u8 m_mipLevel = 0;
        bool m_isReadOnly = false;
    };
}