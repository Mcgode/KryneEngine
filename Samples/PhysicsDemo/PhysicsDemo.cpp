/**
 * @file
 * @author Max Godefroy
 * @date 18/08/2026.
 */

#include "KryneEngine/Core/Window/Input/InputManager.hpp"
#include "Src/RenderTargetFormats.hpp"
#include "Src/SceneManager.hpp"


#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Core/Window/WindowManager.hpp>
#include <KryneEngine/Modules/Box3D/Context.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
#include <KryneEngine/Modules/RenderGraph/Descriptors/RenderTargetViewDesc.hpp>
#include <KryneEngine/Modules/RenderGraph/Registry.hpp>
#include <KryneEngine/Modules/RenderGraph/RenderGraph.hpp>


using namespace KryneEngine;
using namespace KryneEngine::Modules;
using namespace Samples::PhysicsDemo;


int main()
{
    TracySetProgramName("Physics demo");

    KE_ZoneScoped("Physics demo");

    AllocatorInstance allocator {};

    FibersManager fibersManager(0, allocator);

    GraphicsCommon::ApplicationInfo appInfo {};
    appInfo.m_features.m_present = true;
    appInfo.m_applicationName = "Physics demo - Kryne Engine";
#if defined(KE_GRAPHICS_API_VK)
    appInfo.m_api = GraphicsCommon::Api::Vulkan_1_0;
    appInfo.m_applicationName += " - Vulkan";
#elif defined(KE_GRAPHICS_API_DX12)
    appInfo.m_api = GraphicsCommon::Api::DirectX12_1;
    appInfo.m_applicationName += " - DirectX 12";
#elif defined(KE_GRAPHICS_API_MTL)
    appInfo.m_api = GraphicsCommon::Api::Metal_4;
    appInfo.m_applicationName += " - Metal";
#endif
    constexpr GraphicsCommon::DisplayOptions displayOptions {};
    WindowManager windowManager { allocator };
    Window* mainWindow = windowManager.SpawnWindow(appInfo.m_applicationName, displayOptions);
    GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, allocator);

    SwapChainHandle mainSwapChain = graphicsContext->CreateSwapChain({
        .m_nativeWindow = mainWindow->GetNativeHandle(),
        .m_dimensions = mainWindow->GetSize(),
        .m_displayOptions = displayOptions,
    });

    Box3D::Context box3dContext(&fibersManager);
    Box3D::Context::SetAllocator(allocator);

    b3WorldId world;
    b3BodyId ground;
    {
        {
            b3WorldDef worldDef;
            box3dContext.InitWorldDef(worldDef);
            float3 gravity = Math::UpVector() * -9.81f;
            worldDef.gravity = *reinterpret_cast<b3Vec3*>(&gravity);
            world = b3CreateWorld(&worldDef);
        }

        {
            b3BodyDef groundDef = b3DefaultBodyDef();
            groundDef.type = b3_staticBody;
            ground = b3CreateBody(world, &groundDef);
        }

        {
            b3ShapeDef shapeDef = b3DefaultShapeDef();
            b3BoxHull hull = b3MakeBoxHull(100, 100, 0);
            b3CreateHullShape(ground, &shapeDef, &hull.base);
        }
    }

    SceneManager sceneManager(allocator, graphicsContext, mainSwapChain, &fibersManager, world);

    Modules::ImGui::Context* imGuiContext = nullptr;

    RenderGraph::RenderGraph renderGraph {};

    DynamicArray<SimplePoolHandle> swapChainTextures(allocator, graphicsContext->GetFrameContextCount());
    DynamicArray<SimplePoolHandle> swapChainRtvs(allocator, graphicsContext->GetFrameContextCount());

    {
        eastl::string nameTmp(allocator);

        for (u32 i = 0; i < graphicsContext->GetFrameContextCount(); i++)
        {
            swapChainTextures[i] = renderGraph.GetRegistry().RegisterRawTexture(
                graphicsContext->GetSwapChainTexture(mainSwapChain, i),
                nameTmp.sprintf("Swap chain texture %d", i));

            swapChainRtvs[i] = renderGraph.GetRegistry().RegisterRenderTargetView(
                graphicsContext->GetSwapChainRenderTargetView(mainSwapChain, i),
                swapChainTextures[i],
                nameTmp.sprintf("Swap chain RTV %d", i));
        }
    }

    SimplePoolHandle
        fullscreenConstants,
        gBuffer0,
        gBuffer0View,
        gBuffer0Rtv,
        gBuffer1,
        gBuffer1View,
        gBuffer1Rtv,
        gBuffer2,
        gBuffer2View,
        gBuffer2Rtv,
        gBufferDepth,
        gBufferDepthView,
        gBufferDepthRtv,
        deferredShadows,
        deferredShadowsView,
        hdr,
        hdrView,
        hdrRtv;

    {
        fullscreenConstants = renderGraph.GetRegistry().RegisterDummy("Fullscreen Constants");

        {
            gBuffer0 = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kGBuffer0Format,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer0",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
           });

            gBuffer0View = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBuffer0,
                {
                    .m_format = kGBuffer0Format,
                },
                "GBuffer0 view");

            gBuffer0Rtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBuffer0,
                    .m_format = kGBuffer0Format,
                },
                "GBuffer0 RTV");
        }

        {
            gBuffer1 = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kGBuffer1Format,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer1",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
            });

            gBuffer1View = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBuffer1,
                {
                    .m_format = kGBuffer1Format,
                },
                "GBuffer1 view");

            gBuffer1Rtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBuffer1,
                    .m_format = kGBuffer1Format,
                },
                "GBuffer1 RTV");
        }

        {
            gBuffer2 = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kGBuffer2Format,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer2",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage,
            });

            gBuffer2View = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBuffer2,
                {
                    .m_format = kGBuffer2Format,
                },
                "GBuffer2 View");

            gBuffer2Rtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBuffer2,
                    .m_format = kGBuffer2Format,
                },
                "GBuffer2 RTV");
        }

        {
            gBufferDepth = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kGBufferDepthFormat,
                    .m_planes = TexturePlane::Depth,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer Depth",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
            });

            gBufferDepthView = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBufferDepth,
                {
                    .m_format = kGBufferDepthFormat,
                    .m_plane = TexturePlane::Depth,
                },
                "GBuffer Depth view");

            gBufferDepthRtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBufferDepth,
                    .m_format = kGBufferDepthFormat,
                    .m_plane = TexturePlane::Depth,
                },
                "GBuffer Depth RTV");
        }

        {
            deferredShadows = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kDeferredShadowsFormat,
#if !defined(KE_FINAL)
                    .m_debugName = "Deferred shadows"
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage | MemoryUsage::ReadWriteImage,
            });
            deferredShadowsView = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                deferredShadows,
                {
                    .m_format = kDeferredShadowsFormat,
                    .m_accessType = TextureViewAccessType::ReadWrite,
                },
                "Deferred shadows view");
        }

        {
            hdr = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetSwapChainSize(mainSwapChain), 1 },
                    .m_format = kHdrFormat,
#if !defined(KE_FINAL)
                    .m_debugName = "HDR"
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
            });

            hdrView = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                hdr,
                {
                    .m_format = kHdrFormat,
                },
                "HDR view");

            hdrRtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = hdr,
                    .m_format = kHdrFormat,
                },
                "HDR RTV");
        }
    }

    sceneManager.InitPso(
        *graphicsContext,
        graphicsContext->GetSwapChainFormat(mainSwapChain),
        renderGraph.GetRegistry().GetTextureView(gBuffer0View),
        renderGraph.GetRegistry().GetTextureView(gBuffer1View),
        renderGraph.GetRegistry().GetTextureView(gBuffer2View),
        renderGraph.GetRegistry().GetTextureView(gBufferDepthView),
        renderGraph.GetRegistry().GetTextureView(deferredShadowsView),
        renderGraph.GetRegistry().GetTextureView(hdrView));

    auto lastFrameTimePoint = std::chrono::high_resolution_clock::now();
    do
    {
        windowManager.PollEvents();
        windowManager.GetInput().Update();

        if (imGuiContext == nullptr)
        {
            KE_ZoneScoped("Init ImGui context");

            imGuiContext = allocator.New<Modules::ImGui::Context>(
                mainWindow,
                &windowManager,
                graphicsContext,
                graphicsContext->GetSwapChainFormat(mainSwapChain),
                allocator);
        }

        imGuiContext->NewFrame(mainWindow, graphicsContext, mainSwapChain);

        auto timePoint = std::chrono::high_resolution_clock::now();
        const double deltaTime = std::chrono::duration<double> { timePoint - lastFrameTimePoint }.count();
        sceneManager.Process(graphicsContext, static_cast<float>(deltaTime));
        lastFrameTimePoint = timePoint;

        RenderGraph::Builder& builder = renderGraph.BeginFrame(*graphicsContext);

        SimplePoolHandle swapChainTexture = swapChainTextures[graphicsContext->GetSwapChainCurrentImageIndex(mainSwapChain)];
        SimplePoolHandle swapChainRtv = swapChainRtvs[graphicsContext->GetSwapChainCurrentImageIndex(mainSwapChain)];

        const uint2 frameBufferSize = graphicsContext->GetSwapChainSize(mainSwapChain);

        ::ImGui::ShowDemoWindow();

        builder
            .DeclarePass(RenderGraph::PassType::Transfer)
                .SetName("Upload fullscreen constants")
                .WriteDependency({ .m_resource = fullscreenConstants })
                .SetExecuteFunction([&sceneManager, frameBufferSize](const auto&, const auto& _executionData)
                {
                    sceneManager.UpdateFullscreenConstantsBuffer(_executionData.m_graphicsContext, _executionData.m_transferEncoder, frameBufferSize);
                })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("GBuffer pass")
                .AddColorAttachment(gBuffer0Rtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .AddColorAttachment(gBuffer1Rtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .AddColorAttachment(gBuffer2Rtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .SetDepthAttachment(gBufferDepthRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .SetClearDepthStencil(0.f)
                    .Done()
                .SetExecuteFunction([](const auto&, const auto&) { /* TODO*/ })
                .Done()
            .DeclarePass(RenderGraph::PassType::Compute)
                .SetName("Deferred shadows pass")
                .ReadDependency({
                    .m_resource = gBufferDepthView,
                    .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                    .m_planes = TexturePlane::Depth,
                })
                .WriteDependency({
                    .m_resource = deferredShadows,
                    .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                    .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                    .m_targetLayout = TextureLayout::UnorderedAccess,
                })
                .SetExecuteFunction([](const auto&, const auto&) { /* TODO*/ })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("Deferred shading pass")
                .AddColorAttachment(hdrRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .ReadDependency({ .m_resource = fullscreenConstants })
                .ReadDependency({
                    .m_resource = gBuffer0,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .ReadDependency({
                    .m_resource = gBuffer1,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .ReadDependency({
                    .m_resource = gBuffer2,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .ReadDependency({
                    .m_resource = gBufferDepth,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                    .m_planes = TexturePlane::Depth,
                })
                .ReadDependency({
                    .m_resource = deferredShadows,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .SetExecuteFunction([&sceneManager, frameBufferSize](const auto& _renderGraph, const auto& _executionPass)
                {
                    sceneManager.GetDeferredShadingPass().Render(_renderGraph, _executionPass, frameBufferSize);
                })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("Sky pass")
                .AddColorAttachment(hdrRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .SetDepthAttachment(gBufferDepthRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::DontCare)
                    .SetReadOnlyDepthStencil()
                    .Done()
                .ReadDependency({ .m_resource = fullscreenConstants })
                .SetExecuteFunction([&sceneManager, frameBufferSize](const auto& _renderGraph, const auto& _executionPass)
                {
                    sceneManager.GetSkyPass().Render(_renderGraph, _executionPass, frameBufferSize);
                })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("Color mapping pass")
                .AddColorAttachment(swapChainRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .ReadDependency({ .m_resource = fullscreenConstants })
                .ReadDependency({
                    .m_resource = hdr,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .SetExecuteFunction([&sceneManager, frameBufferSize](const auto& _renderGraph, const auto& _executionPass)
                {
                    sceneManager.GetColorPass().Render(_renderGraph, _executionPass, frameBufferSize);
                })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("ImGui pass")
                .SetPrePassTransferFunction([imGuiContext](GraphicsContext* _graphicsContext, const TransferCommandEncoderHandle _transferEncoder)
                {
                    imGuiContext->PrepareToRenderFrame(_graphicsContext, _transferEncoder);
                })
                .SetExecuteFunction([imGuiContext](RenderGraph::RenderGraph&, const RenderGraph::PassExecutionData& _executionData)
                {
                    imGuiContext->RenderFrame(_executionData.m_graphicsContext, _executionData.m_renderEncoder);
                })
                .AddColorAttachment(swapChainRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .Done()
            .DeclareTargetResource(swapChainTexture);

        builder.BuildDag();
        renderGraph.SubmitFrame(*graphicsContext, &fibersManager);

        graphicsContext->EndFrame(imGuiContext->GetSwapChainsToPresent());
    }
    while (!windowManager.AllWindowsClosed());

    if (imGuiContext)
    {
        imGuiContext->Shutdown(&windowManager, graphicsContext);
        allocator.Delete(imGuiContext);
    }

    graphicsContext->WaitForLastFrame();
    graphicsContext->DestroySwapChain(mainSwapChain);

    windowManager.DestroyWindow(mainWindow);

    GraphicsContext::Destroy(graphicsContext);

    return 0;
}