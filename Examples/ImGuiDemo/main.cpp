/**
 * @file
 */

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/RenderPass.hpp"
#include "KryneEngine/Core/Memory/Allocators/TlsfAllocator.hpp"
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>
#include <iostream>

using namespace KryneEngine;
namespace KEModules = KryneEngine::Modules;

void Job0(void* _counterPtr)
{
    ZoneScoped;
    (*static_cast<std::atomic<u32>*>(_counterPtr))++;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void Job1(void*)
{
    ZoneScoped;
    std::atomic<u32> counter = 0;

    std::cout << "Counter value: " << counter << std::endl;

    auto* fibersManager = FibersManager::GetInstance();

    static constexpr u32 kCount = 1'000;

    const auto syncCounter = fibersManager->InitAndBatchJobs(Job0, &counter, kCount);

    fibersManager->WaitForCounterAndReset(syncCounter);

    std::cout << "Counter value: " << counter << std::endl;
}

void MainFunc(void* _pAllocator)
{
    AllocatorInstance allocator = *static_cast<AllocatorInstance*>(_pAllocator);

    auto appInfo = GraphicsCommon::ApplicationInfo {
        .m_applicationName { "ImGuiDemo - Kryne Engine 2", allocator }
    };
#if defined(KE_GRAPHICS_API_VK)
    appInfo.m_api = GraphicsCommon::Api::Vulkan_1_3;
    appInfo.m_applicationName += " - Vulkan";
#elif defined(KE_GRAPHICS_API_DX12)
    appInfo.m_api = KryneEngine::GraphicsCommon::Api::DirectX12_1;
    appInfo.m_applicationName += " - DirectX 12";
#elif defined(KE_GRAPHICS_API_MTL)
    appInfo.m_api = KryneEngine::GraphicsCommon::Api::Metal_3;
    appInfo.m_applicationName += " - Metal";
#endif
    Window mainWindow(appInfo, allocator);
    GraphicsContext* graphicsContext = mainWindow.GetGraphicsContext();

    DynamicArray<RenderPassHandle> renderPassHandles(allocator);
    renderPassHandles.Resize(graphicsContext->GetFrameContextCount());
    for (auto i = 0u; i < renderPassHandles.Size(); i++)
    {
        RenderPassDesc desc;
        desc.m_colorAttachments.push_back(RenderPassDesc::Attachment {
            KryneEngine::RenderPassDesc::Attachment::LoadOperation::Clear,
            KryneEngine::RenderPassDesc::Attachment::StoreOperation::Store,
            TextureLayout::Unknown,
            TextureLayout::Present,
            graphicsContext->GetPresentRenderTargetView(i),
            float4(0, 1, 1, 1)
        });
#if !defined(KE_FINAL)
        desc.m_debugName.sprintf("PresentRenderPass[%d]", i);
#endif
        renderPassHandles[i] = graphicsContext->CreateRenderPass(desc);
    }

    KEModules::ImGui::Context imGuiContext { &mainWindow, renderPassHandles[0], allocator };

    do
    {
        KE_ZoneScoped("Main loop");

        CommandListHandle commandList = graphicsContext->BeginGraphicsCommandList();

        imGuiContext.NewFrame(&mainWindow);

        {
            static bool open;
            ImGui::ShowDemoWindow(&open);
        }

        imGuiContext.PrepareToRenderFrame(graphicsContext, commandList);

        const u8 index = graphicsContext->GetCurrentPresentImageIndex();
        graphicsContext->BeginRenderPass(commandList, renderPassHandles[index]);

        imGuiContext.RenderFrame(graphicsContext, commandList);

        graphicsContext->EndRenderPass(commandList);

        graphicsContext->EndGraphicsCommandList(commandList);
    }
    while (graphicsContext->EndFrame());

    graphicsContext->WaitForLastFrame();

    imGuiContext.Shutdown(&mainWindow);

    for (auto handle: renderPassHandles)
    {
        graphicsContext->DestroyRenderPass(handle);
    }
}

int main()
{
    std::cout << "Hello, World!" << std::endl;

    TlsfAllocator* tlsfAllocator = TlsfAllocator::Create(AllocatorInstance(), 32 << 20); // 32 MiB heap
    AllocatorInstance allocator(tlsfAllocator);

    {
        auto fibersManager = FibersManager(0, allocator);

        const auto syncCounter = fibersManager.InitAndBatchJobs(Job1, nullptr);

#if !defined(__APPLE__)
        const auto mainCounter = fibersManager.InitAndBatchJobs(
            MainFunc,
            &allocator,
            1,
            FiberJob::Priority::High,
            true);

        fibersManager.WaitForCounterAndReset(mainCounter);
#else
        MainFunc(&allocator);
#endif

        fibersManager.WaitForCounterAndReset(syncCounter);
    }

    return 0;
}
