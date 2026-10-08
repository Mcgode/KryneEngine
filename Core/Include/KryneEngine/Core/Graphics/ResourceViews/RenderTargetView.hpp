/**
 * @file
 * @author Max Godefroy
 * @date 14/02/2024.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Enums.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"

namespace KryneEngine
{
    struct RenderTargetViewDesc
    {
        TextureHandle m_texture;

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

#if !defined(KE_FINAL)
        eastl::string m_debugName;
#endif
    };
}
