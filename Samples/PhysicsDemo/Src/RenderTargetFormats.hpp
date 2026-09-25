/**
 * @file
 * @author Max Godefroy
 * @date 28/08/2026.
 */

#pragma once


#include <KryneEngine/Core/Graphics/Enums.hpp>


namespace KryneEngine::Samples::PhysicsDemo
{
    static constexpr auto kGBuffer0Format = TextureFormat::RGBA8_UNorm;
    static constexpr auto kGBuffer1Format = TextureFormat::RGBA8_UNorm;
    static constexpr auto kGBuffer2Format = TextureFormat::RGBA16_Float;
    static constexpr auto kGBufferDepthFormat = TextureFormat::D32F;

    static constexpr auto kShadowFormat = TextureFormat::D16;

    static constexpr auto kDeferredShadowsFormat = TextureFormat::R8_UNorm;

    static constexpr auto kHdrFormat = TextureFormat::RGBA16_Float;
}