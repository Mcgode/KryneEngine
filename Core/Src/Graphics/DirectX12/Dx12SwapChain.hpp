/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#pragma once

#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "Graphics/DirectX12/Dx12Resources.h"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class Dx12SwapChain
    {
        friend class Dx12GraphicsContext;

    public:
        Dx12SwapChain(AllocatorInstance _allocator);
        ~Dx12SwapChain();

        void Init(
            const GraphicsCommon::ApplicationInfo &_appInfo,
            const SwapChainDesc& _desc,
            IDXGIFactory4 *_factory,
            ID3D12Device *_device,
            ID3D12CommandQueue *_directQueue,
            KryneEngine::Dx12Resources& _resources);

        bool Resize(ID3D12Device* _device, Dx12Resources& _resources, uint2 _newSize);

        [[nodiscard]] u8 GetBackBufferIndex() const
        {
	        return m_swapChain->GetCurrentBackBufferIndex();
        }

        void Present() const;

        void Destroy(Dx12Resources& _resources);

        [[nodiscard]] TextureFormat GetPresentTextureFormat() const
        {
            return m_presentFormat;
        }

        [[nodiscard]] RenderTargetViewHandle GetRenderTargetView(u8 _index) const { return m_renderTargetViews[_index]; }
        [[nodiscard]] TextureHandle GetTexture(u8 _index) const { return m_renderTargetTextures[_index]; }
        [[nodiscard]] u8 GetImageCount() const { return static_cast<u8>(m_renderTargetViews.Size()); }
        [[nodiscard]] uint2 GetSize() const { return m_size; }

    private:
        ComPtr<IDXGISwapChain3> m_swapChain;

        DynamicArray<TextureHandle> m_renderTargetTextures;
        DynamicArray<RenderTargetViewHandle> m_renderTargetViews;

        TextureFormat m_presentFormat = TextureFormat::NoFormat;
        uint2 m_size {};

        u8 m_currentFrame;

        void _CreateRenderTargets(ID3D12Device* _device, Dx12Resources& _resources, u32 _imageCount);
        void _ReleaseRenderTargets(Dx12Resources& _resources);
    };
} // KryneEngine