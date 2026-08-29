/**
 * @file
 * @author Max Godefroy
 * @date 18/08/2026.
 */

#include "Src/RenderTargetFormats.hpp"
#include "Src/SceneManager.hpp"


#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
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
    Window mainWindow(appInfo, allocator);
    GraphicsContext* graphicsContext = mainWindow.GetGraphicsContext();

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

    SceneManager sceneManager(allocator, *graphicsContext, &fibersManager, world);

    Modules::ImGui::Context* imGuiContext = nullptr;

    RenderGraph::RenderGraph renderGraph {};

    DynamicArray<SimplePoolHandle> swapChainTextures(allocator, graphicsContext->GetFrameContextCount());
    DynamicArray<SimplePoolHandle> swapChainRtvs(allocator, graphicsContext->GetFrameContextCount());

    {
        eastl::string nameTmp(allocator);

        for (u32 i = 0; i < graphicsContext->GetFrameContextCount(); i++)
        {
            swapChainTextures[i] = renderGraph.GetRegistry().RegisterRawTexture(
                graphicsContext->GetPresentTexture(i),
                nameTmp.sprintf("Swap chain texture %d", i));

            swapChainRtvs[i] = renderGraph.GetRegistry().RegisterRenderTargetView(
                graphicsContext->GetPresentRenderTargetView(i),
                swapChainTextures[i],
                nameTmp.sprintf("Swap chain RTV %d", i));
        }
    }

    SimplePoolHandle
        gBufferAlbedo,
        gBufferAlbedoView,
        gBufferAlbedoRtv,
        gBufferNormal,
        gBufferNormalView,
        gBufferNormalRtv,
        gBufferDepth,
        gBufferDepthView,
        gBufferDepthRtv,
        deferredShadows,
        deferredShadowsView,
        hdr,
        hdrView,
        hdrRtv;

    {
        {
            gBufferAlbedo = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetPresentFrameBufferSize(), 1 },
                    .m_format = kGBufferAlbedoFormat,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer Albedo",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
           });

            gBufferAlbedoView = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBufferAlbedo,
                {
                    .m_format = kGBufferAlbedoFormat,
                },
                "GBuffer Albedo view");

            gBufferAlbedoRtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBufferAlbedo,
                    .m_format = kGBufferAlbedoFormat,
                },
                "GBuffer Albedo RTV");
        }

        {
            gBufferNormal = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetPresentFrameBufferSize(), 1 },
                    .m_format = kGBufferNormalFormat,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer Normal",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage,
            });

            gBufferNormalView = renderGraph.GetRegistry().CreateTextureView(
                graphicsContext,
                gBufferNormal,
                {
                    .m_format = kGBufferNormalFormat,
                },
                "GBuffer Normal view");

            gBufferNormalRtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBufferNormal,
                    .m_format = kGBufferNormalFormat,
                },
                "GBuffer Normal RTV");
        }

        {
            gBufferDepth = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetPresentFrameBufferSize(), 1 },
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
                },
                "GBuffer Depth view");

            gBufferDepthRtv = renderGraph.GetRegistry().CreateRenderTargetView(
                graphicsContext,
                RenderGraph::RenderTargetViewDesc {
                    .m_textureResource = gBufferDepth,
                    .m_format = kGBufferDepthFormat,
                },
                "GBuffer Depth RTV");
        }

        {
            deferredShadows = renderGraph.GetRegistry().CreateRawTexture(graphicsContext, {
                .m_desc = {
                    .m_dimensions { graphicsContext->GetPresentFrameBufferSize(), 1 },
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
                    .m_dimensions { graphicsContext->GetPresentFrameBufferSize(), 1 },
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

    auto lastFrameTimePoint = std::chrono::high_resolution_clock::now();
    do
    {
        auto timePoint = std::chrono::high_resolution_clock::now();
        const double deltaTime = std::chrono::duration<double> { timePoint - lastFrameTimePoint }.count();
        sceneManager.Process(deltaTime);
        lastFrameTimePoint = timePoint;

        if (imGuiContext == nullptr)
        {
            KE_ZoneScoped("Init ImGui context");

            imGuiContext = allocator.New<Modules::ImGui::Context>(
                &mainWindow,
                graphicsContext->GetPresentTextureFormat(),
                allocator);
        }

        imGuiContext->NewFrame(&mainWindow);

        RenderGraph::Builder& builder = renderGraph.BeginFrame(*graphicsContext);

        SimplePoolHandle swapChainTexture = swapChainTextures[graphicsContext->GetCurrentPresentImageIndex()];
        SimplePoolHandle swapChainRtv = swapChainRtvs[graphicsContext->GetCurrentPresentImageIndex()];

        ::ImGui::ShowDemoWindow();

        builder
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("GBuffer pass")
                .AddColorAttachment(gBufferAlbedoRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .AddColorAttachment(gBufferAlbedoRtv)
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
                .ReadDependency({
                    .m_resource = gBufferAlbedoView,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .ReadDependency({
                    .m_resource = gBufferNormalView,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .ReadDependency({
                    .m_resource = gBufferDepthView,
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
                .SetExecuteFunction([](const auto&, const auto&) { /* TODO*/ })
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
                .SetExecuteFunction([](const auto&, const auto&) { /* TODO*/ })
                .Done()
            .DeclarePass(RenderGraph::PassType::Render)
                .SetName("Color mapping pass")
                .AddColorAttachment(swapChainRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .ReadDependency({
                    .m_resource = hdr,
                    .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                    .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                    .m_targetLayout = TextureLayout::ShaderResource,
                })
                .SetExecuteFunction([](const auto&, const auto&) { /* TODO*/ })
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
                    .SetClearColor({ .3f, 1.f, 1.f, 1.f })
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .Done()
            .DeclareTargetResource(swapChainTexture);

        builder.BuildDag();
        renderGraph.SubmitFrame(*graphicsContext, &fibersManager);
    }
    while (graphicsContext->EndFrame());

    if (imGuiContext)
    {
        imGuiContext->Shutdown(&mainWindow);
        allocator.Delete(imGuiContext);
    }

    return 0;
}