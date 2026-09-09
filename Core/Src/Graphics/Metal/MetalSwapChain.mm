/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2024.
 */

#include "Graphics/Metal/MetalSwapChain.hpp"

#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>
#include <EASTL/fixed_string.h>

#include "Graphics/Metal/MetalResources.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

namespace KryneEngine
{
    void MetalSwapChain::Init(
        AllocatorInstance _allocator,
        MTL::Device& _device,
        const GraphicsCommon::ApplicationInfo& _appInfo,
        const SwapChainDesc& _desc,
        MetalResources& _resources,
        u8 _initialFrameIndex)
    {
        KE_ASSERT(_desc.m_nativeWindow.m_kind == NativeWindowHandle::Kind::Cocoa);
        auto* metalWindow = (__bridge NSWindow*)_desc.m_nativeWindow.m_windowHandle;

        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = (__bridge id<MTLDevice>)&_device;
        if (_desc.m_displayOptions.m_sRgbPresent == GraphicsCommon::SoftEnable::Disabled)
        {
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        }
        else
        {
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        }

        metalLayer.displaySyncEnabled = YES;

        const u8 imageCount = static_cast<u8>(_appInfo.m_bufferingMode);
        metalLayer.maximumDrawableCount = static_cast<NSUInteger>(imageCount);

        metalLayer.contentsScale = metalWindow.backingScaleFactor;

        m_textures.SetAllocator(_allocator);
        m_rtvs.SetAllocator(_allocator);

        m_textures.Resize(imageCount);
        m_rtvs.Resize(imageCount);

        metalLayer.framebufferOnly = YES;

        metalWindow.contentView.layer = metalLayer;
        metalWindow.contentView.wantsLayer = YES;

        m_metalLayer = reinterpret_cast<CA::MetalLayer*>(metalLayer);

        const RenderTargetViewDesc rtvDesc {
            .m_texture = { GenPool::kInvalidHandle },
            .m_format = metalLayer.pixelFormat == MTLPixelFormatBGRA8Unorm_sRGB
                ? TextureFormat::BGRA8_sRGB
                : TextureFormat::BGRA8_UNorm,
        };

        {
            m_drawable = nullptr;
            for (size_t i = 0; i < imageCount; i++)
            {
                m_textures[i] = _resources.RegisterSystemTexture();
                m_rtvs[i] = _resources.RegisterSystemRtv(rtvDesc);
            }
            UpdateNextDrawable(_initialFrameIndex, _resources);
        }

        m_index = _initialFrameIndex;
    }

    void MetalSwapChain::Resize(uint2 _newSize)
    {
        m_metalLayer->setDrawableSize(CGSizeMake(_newSize.x, _newSize.y));
    }

    void MetalSwapChain::Destroy(MetalResources& _resources)
    {
        for (const RenderTargetViewHandle handle : m_rtvs)
            _resources.UnregisterRtv(handle);
        for (const TextureHandle handle : m_textures)
            _resources.UnregisterTexture(handle);
        m_rtvs.Clear();
        m_textures.Clear();
        m_drawable.reset();
        m_metalLayer = nullptr;
    }

    void MetalSwapChain::UpdateNextDrawable(u8 _frameIndex, MetalResources& _resources)
    {
        KE_AUTO_RELEASE_POOL;
        CA::MetalDrawable* drawable = m_metalLayer->nextDrawable()->retain();
        m_drawable.reset(drawable);
#if !defined(KE_FINAL)
        eastl::fixed_string<char, 64, false> label;
        label.sprintf("Drawable texture %u", _frameIndex);
        m_drawable->texture()->setLabel(NS::String::string(label.c_str(), NS::UTF8StringEncoding));
#endif
        KE_ASSERT_FATAL(m_drawable != nullptr);
        _resources.UpdateSystemTexture(m_textures[_frameIndex], drawable->texture());
        _resources.UpdateSystemTexture(m_rtvs[_frameIndex], drawable->texture());

        m_index = (_frameIndex + 1) % m_textures.Size();
    }
} // namespace KryneEngine