/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */


#include "Graphics/DirectX12/Dx12SwapChain.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Graphics/DirectX12/Dx12GraphicsContext.hpp"
#include "Graphics/DirectX12/HelperFunctions.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

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
        const Window* _processWindow,
        IDXGIFactory4 *_factory,
        ID3D12Device *_device,
        ID3D12CommandQueue *_directQueue,
        KryneEngine::Dx12Resources& _resources)
    {
        KE_ZoneScopedFunction("Dx12SwapChain::Dx12SwapChain");

        const auto& displayInfo = _appInfo.m_displayOptions;

        u32 imageCount = 2;
        if (displayInfo.m_tripleBuffering != GraphicsCommon::SoftEnable::Disabled)
        {
            imageCount++;
        }

        // sRGB format is set by the RTV
        const auto format = DXGI_FORMAT_B8G8R8A8_UNORM;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
        swapChainDesc.BufferCount = imageCount;
        swapChainDesc.Width = displayInfo.m_width * _processWindow->GetDpiScale().x;
        swapChainDesc.Height = displayInfo.m_height * _processWindow->GetDpiScale().x;
        swapChainDesc.Format = format;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1; // Disable MultiSampling

        ComPtr<IDXGISwapChain1> swapChain;
        auto hwndWindow = glfwGetWin32Window(_processWindow->GetGlfwWindow());
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
                    .m_format = displayInfo.m_sRgbPresent == GraphicsCommon::SoftEnable::Disabled
                            ? TextureFormat::BGRA8_UNorm
                            : TextureFormat::BGRA8_sRGB
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