/**
 * @file
 * @author Max Godefroy
 * @date 28/08/2026.
 */

#pragma once


#include <KryneEngine/Core/Graphics/Enums.hpp>


namespace KryneEngine::Samples::PhysicsDemo
{
    static constexpr auto kGBufferAlbedoFormat = TextureFormat::RGBA8_UNorm;
    static constexpr auto kGBufferNormalFormat = TextureFormat::RGBA8_UNorm;
    static constexpr auto kGBufferDepthFormat = TextureFormat::D32F;

    static constexpr auto kShadowFormat = TextureFormat::D16;
}