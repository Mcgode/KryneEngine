/**
 * @file
 * @author Max Godefroy
 * @date 01/05/2025.
 */

#pragma once

#include "KryneEngine/Core/Common/BitUtils.hpp"
#include "KryneEngine/Core/Graphics/Enums.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"

namespace KryneEngine
{
    enum class TextureViewAccessType : u8
    {
        Read            = 1 << 0,
        Write           = 1 << 1,
        ReadWrite       = Read | Write,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(TextureViewAccessType)

    struct TextureViewDesc
    {
        TextureHandle m_texture;

        Texture4ComponentsMapping m_componentsMapping { kDefaultTexture4ComponentsMapping };

        u16 m_arrayStart = 0;
        u16 m_arrayRange = 1;

        TextureFormat m_format;

        TextureTypes m_viewType = TextureTypes::Single2D;
        u8 m_minMip = 0;
        u8 m_maxMip = 0;
        TexturePlane m_plane = TexturePlane::Color;

        TextureViewAccessType m_accessType = TextureViewAccessType::Read;

#if !defined(KE_FINAL)
        eastl::string m_debugName = "";
#endif
    };
}