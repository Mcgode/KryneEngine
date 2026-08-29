/**
 * @file
 * @author Max Godefroy
 * @date 13/11/2024.
 */

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
#include <KryneEngine/Modules/RenderGraph/Descriptors/RenderTargetViewDesc.hpp>
#include <KryneEngine/Modules/RenderGraph/Registry.hpp>
#include <KryneEngine/Modules/RenderGraph/RenderGraph.hpp>
#include <KryneEngine/Modules/RenderGraph/Resource.hpp>
#include <KryneEngine/Modules/RenderGraph/ImGuiDebugWindow.hpp>

#include <Rendering/Fullscreen/ColorMappingPass.hpp>
#include <Rendering/Fullscreen/DeferredShadingPass.hpp>
#include <Rendering/Fullscreen/SkyPass.hpp>

#include "Rendering/DeferredShadowPass.hpp"
#include "Rendering/GiPass.hpp"
#include "Scene/SceneManager.hpp"

using namespace KryneEngine;
using namespace KryneEngine::Modules;
using namespace KryneEngine::Samples;
using namespace KryneEngine::Samples::RenderGraphDemo;

int main()
{
    TracySetProgramName("Render graph demo");

    KE_ZoneScoped("Render graph demo");

    AllocatorInstance allocator = AllocatorInstance();

    FibersManager fibersManager(0, allocator);

    GraphicsCommon::ApplicationInfo appInfo {};
    appInfo.m_features.m_present = true;
    appInfo.m_applicationName = "Render graph demo - Kryne Engine 2";
#if defined(KE_GRAPHICS_API_VK)
    appInfo.m_api = GraphicsCommon::Api::Vulkan_1_3;
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

    Modules::ImGui::Context* imGuiContext = nullptr;

    RenderGraph::RenderGraph renderGraph {};
    SceneManager sceneManager(allocator, mainWindow, renderGraph.GetRegistry());

    DeferredShadowPass deferredShadowPass { allocator };
    GiPass giPass { allocator };
    DeferredShadingPass deferredShadingPass { allocator };
    SkyPass skyPass { allocator };
    ColorMappingPass colorMappingPass { allocator };

    SimplePoolHandle
        gBuffer0,
        gBuffer0Rtv,
        gBuffer0View,
        gBuffer1,
        gBuffer1Rtv,
        gBuffer1View,
        gBufferDepth,
        gBufferDepthRtv,
        gBufferDepthView,
        deferredShadow,
        deferredShadowView,
        deferredGi,
        deferredGiView,
        hdr,
        hdrRtv,
        hdrView;

    constexpr auto gbuffer0Format = TextureFormat::RGBA8_UNorm;
    constexpr auto gbuffer1Format = TextureFormat::RGBA8_UNorm;
    constexpr auto gbufferDepthFormat = TextureFormat::D32F;
    constexpr auto hdrFormat = TextureFormat::RGBA16_Float;

    DynamicArray<SimplePoolHandle> swapChainTextures(allocator, graphicsContext->GetFrameContextCount());
    DynamicArray<SimplePoolHandle> swapChainRtvs(allocator, graphicsContext->GetFrameContextCount());

    {
        KE_ZoneScoped("Registration");

        const uint3 dimensions(graphicsContext->GetPresentFrameBufferSize(), 1);

        for (auto i = 0u; i < graphicsContext->GetFrameContextCount(); i++)
        {
            eastl::string name;

            swapChainTextures[i] = renderGraph.GetRegistry().RegisterRawTexture(
                graphicsContext->GetPresentTexture(i),
                name.sprintf("Swapchain buffer %u", i));
            swapChainRtvs[i] = renderGraph.GetRegistry().RegisterRenderTargetView(
                graphicsContext->GetPresentRenderTargetView(i),
                swapChainTextures[i],
                name.sprintf("Swapchain RTV %u", i));
        }

        gBuffer0 = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = gbuffer0Format,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer 0",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage | MemoryUsage::SampledImage,
            });
        gBuffer0Rtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = gBuffer0,
                .m_format = gbuffer0Format,
            },
            "GBuffer 0 RTV");
        gBuffer0View = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBuffer0,
            { .m_format = gbuffer0Format });

        gBuffer1 = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = gbuffer1Format,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer 1",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage | MemoryUsage::SampledImage,
            });
        gBuffer1Rtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = gBuffer1,
                .m_format = gbuffer1Format,
            },
            "GBuffer 1 RTV");
        gBuffer1View = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBuffer1,
            { .m_format = gbuffer1Format });

        gBufferDepth = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = gbufferDepthFormat,
                    .m_planes = TexturePlane::Depth,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer depth"
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage | MemoryUsage::ReadImage | MemoryUsage::SampledImage,
            });
        gBufferDepthRtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = gBufferDepth,
                .m_format = gbufferDepthFormat,
                .m_plane = TexturePlane::Depth,
            },
            "GBuffer depth RTV");
        gBufferDepthView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBufferDepth,
            { .m_format = gbufferDepthFormat, .m_plane = TexturePlane::Depth });

        deferredShadow = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::R8_UNorm,
#if !defined(KE_FINAL)
                    .m_debugName = "Deferred shadow",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ReadWriteImage | MemoryUsage::SampledImage,
            });
        deferredShadowView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            deferredShadow,
            {
                .m_format = TextureFormat::R8_UNorm,
                .m_accessType = TextureViewAccessType::ReadWrite
            },
            "Deferred shadow SRV");

        deferredGi = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::RGBA16_Float,
#if !defined(KE_FINAL)
                    .m_debugName = "Deferred GI",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ReadWriteImage | MemoryUsage::SampledImage,
            });
        deferredGiView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            deferredGi,
            {
                .m_format = TextureFormat::RGBA16_Float,
                .m_accessType = TextureViewAccessType::ReadWrite,
            },
            "Deferred GI SRV");

        hdr = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = hdrFormat,
#if !defined(KE_FINAL)
                    .m_debugName = "HDR render texture",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage | MemoryUsage::ColorTargetImage,
            });
        hdrRtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = hdr,
                .m_format = hdrFormat,
            },
            "HDR render RTV");
        hdrView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            hdr,
            { .m_format = hdrFormat },
            "HDR render SRV"
        );
    }

    deferredShadowPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBufferDepthView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredShadowView).m_textureViewData.m_textureView);
    giPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBuffer0View).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBuffer1View).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBufferDepthView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredGiView).m_textureViewData.m_textureView);
    deferredShadingPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBuffer0View).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBuffer1View).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBufferDepthView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredShadowView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredGiView).m_textureViewData.m_textureView);
    skyPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout());
    colorMappingPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(hdrView).m_textureViewData.m_textureView);

    sceneManager.PreparePsos(graphicsContext, {
        .m_numColorAttachments = 2,
        .m_colorFormats = { gbuffer0Format, gbuffer1Format },
        .m_depthStencilFormat = gbufferDepthFormat,
    });
    deferredShadingPass.CreatePso(graphicsContext, {
        .m_numColorAttachments = 1,
        .m_colorFormats = { hdrFormat },
    });
    skyPass.CreatePso(graphicsContext, {
        .m_numColorAttachments = 1,
        .m_colorFormats = { hdrFormat },
        .m_depthStencilFormat = gbufferDepthFormat,
    });
    colorMappingPass.CreatePso(graphicsContext, {
        .m_numColorAttachments = 1,
        .m_colorFormats = { graphicsContext->GetPresentTextureFormat() },
    });

    do
    {
        if (imGuiContext == nullptr)
        {
            KE_ZoneScoped("Init ImGui context");

            imGuiContext = allocator.New<Modules::ImGui::Context>(
                &mainWindow,
                graphicsContext->GetPresentTextureFormat(),
                allocator);
        }

        imGuiContext->NewFrame(&mainWindow);

        {
            const DescriptorSetHandle sceneConstantsDescriptorSet =
                sceneManager.GetSceneDescriptorSet(graphicsContext->GetCurrentFrameContextIndex());
            deferredShadowPass.UpdateSceneConstants(sceneConstantsDescriptorSet);
            giPass.UpdateSceneConstants(sceneConstantsDescriptorSet);
            deferredShadingPass.UpdateSceneConstants(sceneConstantsDescriptorSet);
            skyPass.UpdateSceneConstants(sceneConstantsDescriptorSet);
            colorMappingPass.UpdateSceneConstants(sceneConstantsDescriptorSet);
        }

        RenderGraph::Builder& builder = renderGraph.BeginFrame(*graphicsContext);

        SimplePoolHandle swapChainTexture = swapChainTextures[graphicsContext->GetCurrentPresentImageIndex()];
        SimplePoolHandle swapChainRtv = swapChainRtvs[graphicsContext->GetCurrentPresentImageIndex()];

        {
            KE_ZoneScoped("Build render graph");

            sceneManager.DeclareDataTransferPass(graphicsContext, builder, imGuiContext);

            const RenderGraph::Dependency frameCBufferReadDep {
                .m_resource = sceneManager.GetSceneConstantsCbv(),
                .m_targetAccessFlags = BarrierAccessFlags::ConstantBuffer,
            };

            builder
                .DeclarePass(RenderGraph::PassType::Render)
                    .SetName("GBuffer pass")
                    .SetExecuteFunction([&sceneManager](const auto& _, const auto& _passData)
                        {
                            KE_ZoneScoped("Render GBuffer");
                            sceneManager.RenderGBuffer(_passData.m_graphicsContext, _passData.m_renderEncoder);
                        })
                    .AddColorAttachment(gBuffer0Rtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .AddColorAttachment(gBuffer1Rtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .SetDepthAttachment(gBufferDepthRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .SetClearDepthStencil(0.f, 0)
                        .Done()
                    .ReadDependency(frameCBufferReadDep)
                    .Done()
                .DeclarePass(RenderGraph::PassType::Compute)
                    .SetName("Deferred shadow pass")
                    .SetExecuteFunction([&deferredShadowPass](const auto&, const auto& _passData) { deferredShadowPass.Render(_passData); })
                    .ReadDependency(frameCBufferReadDep)
                    .ReadDependency({
                        .m_resource = gBufferDepthView,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                        .m_planes = TexturePlane::Depth,
                    })
                    .WriteDependency({
                        .m_resource = deferredShadowView,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                        .m_targetLayout = TextureLayout::UnorderedAccess,
                    })
                    .Done()
                .DeclarePass(RenderGraph::PassType::Compute)
                    .SetName("Deferred 'GI' pass")
                    .SetExecuteFunction([&giPass](const auto&, const auto& _passData) { giPass.Render(_passData); })
                    .ReadDependency(frameCBufferReadDep)
                    .ReadDependency({
                        .m_resource = gBuffer0View,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .ReadDependency({
                        .m_resource = gBuffer1View,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .ReadDependency({
                        .m_resource = gBufferDepthView,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                        .m_planes = TexturePlane::Depth,
                    })
                    .WriteDependency({
                        .m_resource = deferredGiView,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                        .m_targetLayout = TextureLayout::UnorderedAccess,
                    })
                    .Done()
                .DeclarePass(Modules::RenderGraph::PassType::Render)
                    .SetName("Deferred shading pass")
                    .SetExecuteFunction([&deferredShadingPass](const auto& _, const auto& _passData) { deferredShadingPass.Render(_, _passData); })
                    .AddColorAttachment(hdrRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .ReadDependency(frameCBufferReadDep)
                    .ReadDependency({
                        .m_resource = gBuffer0View,
                        .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .ReadDependency({
                        .m_resource = gBuffer1View,
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
                        .m_resource = deferredShadowView,
                        .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .ReadDependency({
                        .m_resource = deferredGiView,
                        .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .Done()
                .DeclarePass(Modules::RenderGraph::PassType::Render)
                    .SetName("Sky pass")
                    .SetExecuteFunction([&skyPass](const auto& _renderGraph, const auto& _passData) { skyPass.Render(_renderGraph, _passData); })
                    .AddColorAttachment(hdrRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .SetDepthAttachment(gBufferDepthRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::DontCare)
                        .Done()
                    .ReadDependency(frameCBufferReadDep)
                    .Done()
                .DeclarePass(Modules::RenderGraph::PassType::Render)
                    .SetName("Color mapping pass")
                    .SetExecuteFunction([&colorMappingPass](const auto& _renderGraph, const auto& _passData) { colorMappingPass.Render(_renderGraph, _passData); })
                    .AddColorAttachment(swapChainRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .ReadDependency({
                        .m_resource = hdrView,
                        .m_targetSyncStage = BarrierSyncStageFlags::FragmentShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .Done()
                .DeclareTargetResource(swapChainTexture);
        }

        {
            KE_ZoneScoped("Build ImGui pass");

            const auto executeFunction = [&](
                                             RenderGraph::RenderGraph& _renderGraph,
                                             RenderGraph::PassExecutionData& _passData)
            {
                imGuiContext->RenderFrame(graphicsContext, _passData.m_renderEncoder);
            };

            builder
                .DeclarePass(RenderGraph::PassType::Render)
                .SetName("ImGui pass")
                .SetExecuteFunction(executeFunction)
                .AddColorAttachment(swapChainRtv)
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done();
        }

        {
            KE_ZoneScoped("Process scene");
            sceneManager.Process(graphicsContext);
        }

        {
            KE_ZoneScoped("Builder debug");

            builder.BuildDag();
            RenderGraph::ImGuiDebugWindow::DebugBuilder(builder, renderGraph.GetRegistry(), allocator);
        }

        {
            KE_ZoneScoped("Execute render graph");

            renderGraph.SubmitFrame(*graphicsContext, nullptr);
        }
    }
    while (graphicsContext->EndFrame());

    if (imGuiContext)
    {
        imGuiContext->Shutdown(&mainWindow);
        allocator.Delete(imGuiContext);
    }

    return 0;
}