/**
 * @file
 * @author Max Godefroy
 * @date 30/11/2025.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Enums.hpp"


#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine::Modules::GuiLib
{
    /**
     * @brief Represents a 2D region within a texture resource to be rendered by the GUI.
     */
    struct TextureRegion
    {
        /// The view to the texture to render.
        TextureViewHandle m_textureView;

        /// An optional custom sampler to use instead of the default one.
        SamplerHandle m_customSampler =  { GenPool::kInvalidHandle };

        /// @brief The type of the texture.
        ///
        /// @details
        /// Behaviour will change between texture types:
        /// - Texture1D (and Texture1DArray) will be displayed as a row of texels, i.e., a SIZE x 1 2D texture.
        /// - Texture2D (and Texture2DArray) will be displayed as expected of a 2D texture.
        /// - TextureCube (and TextureCubeArray) will only display one face at a time, face is selected using
        ///   `m_arrayLayer`. Array layer formula is `m_arrayLayer = 6 * arrayLayer + faceIndex`.
        ///   Order is as follows: [X+, X-, Y+, Y-, Z+, Z-].
        /// - Texture3D is not supported
        TextureTypes m_textureType = TextureTypes::Single2D;

        u8 m_mipLevel = 0xff;
        u16 m_arrayLayer = 0;

        /// The texture region offset in normalized UV coordinates
        float2 m_offset = { 0, 0 };

        /// The final region size in normalized UV units
        float2 m_size = { 1, 1 };
    };
}