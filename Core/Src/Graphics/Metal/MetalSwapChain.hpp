/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2024.
 */

#pragma once

#include <QuartzCore/QuartzCore.hpp>

#include "Graphics/Metal/MetalTypes.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class MetalResources;

    class MetalSwapChain
    {
        friend class MetalGraphicsContext;

    public:

        void Init(
            AllocatorInstance _allocator,
            MTL::Device& _device,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            const SwapChainDesc& _desc,
            MetalResources& _resources,
            u8 _initialFrameIndex);

        void Resize(uint2 _newSize);

        void Destroy(MetalResources& _resources);

        void UpdateNextDrawable(u8 _frameIndex, MetalResources& _resources);

        [[nodiscard]] CA::MetalDrawable* GetDrawable() const
        {
            return m_drawable.get();
        }

        [[nodiscard]] uint2 GetDrawableSize() const
        {
            return {  m_metalLayer->drawableSize().width, m_metalLayer->drawableSize().height };
        }

        [[nodiscard]] MTL::PixelFormat GetPixelFormat() const
        {
            return m_metalLayer->pixelFormat();
        }

        [[nodiscard]] RenderTargetViewHandle GetRenderTargetView(u8 _index) const { return m_rtvs[_index]; }
        [[nodiscard]] TextureHandle GetTexture(u8 _index) const { return m_textures[_index]; }
        [[nodiscard]] u8 GetImageCount() const { return static_cast<u8>(m_textures.Size()); }

    private:
        CA::MetalLayer* m_metalLayer;
        NsPtr<CA::MetalDrawable> m_drawable;
        DynamicArray<TextureHandle> m_textures;
        DynamicArray<RenderTargetViewHandle> m_rtvs;
        u8 m_index;
    };
} // namespace KryneEngine
