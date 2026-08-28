/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#pragma once

#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "Graphics/DirectX12/Dx12Resources.h"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class Window;

    class Dx12SwapChain
    {
        friend class Dx12GraphicsContext;

    public:
        Dx12SwapChain(AllocatorInstance _allocator);
        ~Dx12SwapChain();

        void Init(
            const GraphicsCommon::ApplicationInfo &_appInfo,
            const Window* _processWindow,
            IDXGIFactory4 *_factory,
            ID3D12Device *_device,
            ID3D12CommandQueue *_directQueue,
            KryneEngine::Dx12Resources& _resources);

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

    private:
        ComPtr<IDXGISwapChain3> m_swapChain;

        DynamicArray<TextureHandle> m_renderTargetTextures;
        DynamicArray<RenderTargetViewHandle> m_renderTargetViews;

        TextureFormat m_presentFormat = TextureFormat::NoFormat;

        u8 m_currentFrame;
    };
} // KryneEngine