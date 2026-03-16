/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2024.
 */

#pragma once

#include <QuartzCore/QuartzCore.hpp>

#include "Graphics/Metal/MetalTypes.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class MetalResources;
    class Window;

    class MetalSwapChain
    {
        friend class MetalGraphicsContext;

    public:

        void Init(
            AllocatorInstance _allocator,
            MTL::Device& _device,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            const Window* _window,
            MetalResources& _resources,
            u8 _initialFrameIndex);

        void Resize(Window* _window);

        void Present(CommandList _commandList, u8 _frameIndex);

        void UpdateNextDrawable(u8 _frameIndex, MetalResources& _resources);

        [[nodiscard]] uint2 GetDrawableSize() const
        {
            return {  m_metalLayer->drawableSize().width, m_metalLayer->drawableSize().height };
        }

    private:
        CA::MetalLayer* m_metalLayer;
        NsPtr<CA::MetalDrawable> m_drawable;
        DynamicArray<TextureHandle> m_textures;
        DynamicArray<RenderTargetViewHandle> m_rtvs;
        u8 m_index;
    };
} // namespace KryneEngine
