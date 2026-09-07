/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */


#include "Graphics/DirectX12/Dx12SwapChain.hpp"

#include "Graphics/DirectX12/Dx12GraphicsContext.hpp"
#include "Graphics/DirectX12/HelperFunctions.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"

namespace KryneEngine
{
    Dx12SwapChain::Dx12SwapChain(KryneEngine::AllocatorInstance _allocator)
        : m_renderTargetTextures(_allocator)
        , m_renderTargetViews(_allocator)
    {}

    Dx12SwapChain::~Dx12SwapChain()
    {
        KE_ASSERT(m_swapChain == nullptr);
    }

    void Dx12SwapChain::Init(
        const GraphicsCommon::ApplicationInfo &_appInfo,
        const SwapChainDesc& _desc,
        IDXGIFactory4 *_factory,
        ID3D12Device *_device,
        ID3D12CommandQueue *_directQueue,
        KryneEngine::Dx12Resources& _resources)
    {
        KE_ZoneScopedFunction("Dx12SwapChain::Dx12SwapChain");

        // Strict: the buffering mode dictates the image count.
        const u32 imageCount = static_cast<u32>(_appInfo.m_bufferingMode);

        // sRGB format is set by the RTV
        const auto format = DXGI_FORMAT_B8G8R8A8_UNORM;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
        swapChainDesc.BufferCount = imageCount;
        swapChainDesc.Width = _desc.m_dimensions.x;
        swapChainDesc.Height = _desc.m_dimensions.y;
        swapChainDesc.Format = format;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1; // Disable MultiSampling

        KE_ASSERT(_desc.m_nativeWindow.m_kind == NativeWindowHandle::Kind::Win32);
        const HWND hwndWindow = static_cast<HWND>(_desc.m_nativeWindow.m_windowHandle);

        ComPtr<IDXGISwapChain1> swapChain;
        Dx12Assert(_factory->CreateSwapChainForHwnd(
            _directQueue,
            hwndWindow,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
        ));

        Dx12Assert(_factory->MakeWindowAssociation(hwndWindow, DXGI_MWA_NO_ALT_ENTER));

        Dx12Assert(swapChain.As(&m_swapChain));
#if !defined(KE_FINAL)
        Dx12SetName(m_swapChain.Get(), L"Swap Chain");
#endif

        m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

        // Create frame render targets
        {
            m_renderTargetTextures.Resize(imageCount);
            m_renderTargetViews.Resize(imageCount);

            m_presentFormat = _desc.m_displayOptions.m_sRgbPresent == GraphicsCommon::SoftEnable::Disabled
                    ? TextureFormat::BGRA8_UNorm
                    : TextureFormat::BGRA8_sRGB;

            ID3D12Resource* renderTargetTexture = nullptr;
            for (u32 i = 0; i < imageCount; i++)
            {
                Dx12Assert(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargetTexture)));
#if !defined(KE_FINAL)
                Dx12SetName(renderTargetTexture, L"SwapChain Render Target Texture %d", i);
#endif
                const auto textureHandle = _resources.RegisterTexture(renderTargetTexture, nullptr);
                m_renderTargetTextures.Init(i, textureHandle);

                const RenderTargetViewDesc rtvDesc {
                    .m_texture = textureHandle,
                    .m_format = m_presentFormat,
                };
                m_renderTargetViews.Init(i, _resources.CreateRenderTargetView(rtvDesc, _device));
            }
        }
    }

    void Dx12SwapChain::Present() const
    {
        KE_ZoneScopedFunction("Dx12SwapChain::Present");

        Dx12Assert(m_swapChain->Present(0, 0));
    }

    void Dx12SwapChain::Destroy(Dx12Resources &_resources)
    {
        KE_ZoneScopedFunction("Dx12SwapChain::Destroy");

        for (const auto handle: m_renderTargetViews)
        {
            KE_ASSERT_MSG(_resources.FreeRenderTargetView(handle), "Handle was invalid. It shouldn't. Something went wrong with the lifecycle.");
        }
        m_renderTargetViews.Clear();

        for (const auto handle: m_renderTargetTextures)
        {
            // Free the texture from the gen pool, but don't do a release of the ID3D12Resource, as it's handled
            // by the swapchain
            KE_ASSERT_MSG(_resources.ReleaseTexture(handle, false), "Handle was invalid. It shouldn't. Something went wrong with the lifecycle.");
        }
        m_renderTargetTextures.Clear();

        SafeRelease(m_swapChain);
    }
} // KryneEngine