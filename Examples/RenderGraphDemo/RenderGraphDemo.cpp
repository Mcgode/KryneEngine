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

#include "Rendering/ColorMappingPass.hpp"
#include "Rendering/DeferredShadingPass.hpp"
#include "Rendering/DeferredShadowPass.hpp"
#include "Rendering/GiPass.hpp"
#include "Rendering/SkyPass.hpp"
#include "Scene/SceneManager.hpp"

using namespace KryneEngine;
using namespace KryneEngine::Modules;
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
    appInfo.m_api = GraphicsCommon::Api::Metal_3;
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
        gBufferAlbedo,
        gBufferAlbedoRtv,
        gBufferAlbedoView,
        gBufferNormal,
        gBufferNormalRtv,
        gBufferNormalView,
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

        gBufferAlbedo = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::RGBA8_UNorm,
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer albedo",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage | MemoryUsage::SampledImage,
            });
        gBufferAlbedoRtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = gBufferAlbedo,
                .m_format = TextureFormat::RGBA8_UNorm,
            },
            "GBuffer albedo RTV");
        gBufferAlbedoView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBufferAlbedo,
            { .m_format = TextureFormat::RGBA8_UNorm });

        gBufferNormal = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::RGBA8_UNorm, // TODO: Implement RGB10A2 format support
#if !defined(KE_FINAL)
                    .m_debugName = "GBuffer normal",
#endif
                },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage | MemoryUsage::SampledImage,
            });
        gBufferNormalRtv = renderGraph.GetRegistry().CreateRenderTargetView(
            graphicsContext,
            RenderGraph::RenderTargetViewDesc {
                .m_textureResource = gBufferNormal,
                .m_format = TextureFormat::RGBA8_UNorm, // TODO: Implement RGB10A2 format support
            },
            "GBuffer normal RTV");
        gBufferNormalView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBufferNormal,
            { .m_format = TextureFormat::RGBA8_UNorm });

        gBufferDepth = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::D32F,
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
                .m_format = TextureFormat::D32F,
                .m_plane = TexturePlane::Depth,
            },
            "GBuffer depth RTV");
        gBufferDepthView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            gBufferDepth,
            { .m_format = TextureFormat::D32F, .m_plane = TexturePlane::Depth });

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
            "Deferred GI SRV");;

        hdr = renderGraph.GetRegistry().CreateRawTexture(
            graphicsContext,
            {
                .m_desc = {
                    .m_dimensions = dimensions,
                    .m_format = TextureFormat::RGBA16_Float,
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
                .m_format = TextureFormat::RGBA16_Float,
            },
            "HDR render RTV");
        hdrView = renderGraph.GetRegistry().CreateTextureView(
            graphicsContext,
            hdr,
            { .m_format = TextureFormat::RGBA16_Float },
            "HDR render SRV"
        );
    }

    // Init scene PSOs
    {
        RenderGraph::PassDeclaration gBufferDummyPass(Modules::RenderGraph::PassType::Render, 0);
        RenderGraph::PassDeclarationBuilder(gBufferDummyPass, nullptr)
            .SetName("GBuffer pass")
            .AddColorAttachment(gBufferAlbedoRtv)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .Done()
            .AddColorAttachment(gBufferNormalRtv)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .Done()
            .SetDepthAttachment(gBufferDepthRtv)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .SetClearDepthStencil(0.f, 0)
                .Done();
        gBufferDummyPass.m_colorAttachments[0].m_layoutBefore = TextureLayout::ColorAttachment;
        gBufferDummyPass.m_colorAttachments[0].m_layoutAfter = TextureLayout::ShaderResource;
        gBufferDummyPass.m_colorAttachments[1].m_layoutBefore = TextureLayout::ColorAttachment;
        gBufferDummyPass.m_colorAttachments[1].m_layoutAfter = TextureLayout::ShaderResource;
        gBufferDummyPass.m_depthAttachment.value().m_layoutBefore = TextureLayout::DepthStencilAttachment;
        gBufferDummyPass.m_depthAttachment.value().m_layoutAfter = TextureLayout::ShaderResource;

        sceneManager.PreparePsos(
            graphicsContext,
            renderGraph.FetchRenderPass(*graphicsContext, gBufferDummyPass));
    }

    deferredShadowPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBufferDepthView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredShadowView).m_textureViewData.m_textureView);
    giPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBufferAlbedoView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBufferNormalView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBufferDepthView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(deferredGiView).m_textureViewData.m_textureView);
    deferredShadingPass.Initialize(
        graphicsContext,
        sceneManager.GetDescriptorSetLayout(),
        renderGraph.GetRegistry().GetResource(gBufferAlbedoView).m_textureViewData.m_textureView,
        renderGraph.GetRegistry().GetResource(gBufferNormalView).m_textureViewData.m_textureView,
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

    do
    {
        if (imGuiContext == nullptr)
        {
            KE_ZoneScoped("Init ImGui context");

            // Even if it's a dummy pass, the generated render pass should match signature with the one in the render
            // graph for the ImGui pass, so it will be reused there.

            RenderGraph::PassDeclaration imguiDummyPass(Modules::RenderGraph::PassType::Render, 0);
            RenderGraph::PassDeclarationBuilder(imguiDummyPass, nullptr)
                .SetName("ImGui pass")
                .AddColorAttachment(swapChainRtvs[0])
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done();
            imguiDummyPass.m_colorAttachments[0].m_layoutBefore = TextureLayout::ColorAttachment;
            imguiDummyPass.m_colorAttachments[0].m_layoutAfter = TextureLayout::ColorAttachment;

            imGuiContext = allocator.New<Modules::ImGui::Context>(
                &mainWindow,
                renderGraph.FetchRenderPass(*graphicsContext, imguiDummyPass),
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
                            sceneManager.RenderGBuffer(_passData.m_graphicsContext, _passData.m_commandList);
                        })
                    .AddColorAttachment(gBufferAlbedoRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .AddColorAttachment(gBufferNormalRtv)
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
                        .m_resource = gBufferAlbedoView,
                        .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                        .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                        .m_targetLayout = TextureLayout::ShaderResource,
                    })
                    .ReadDependency({
                        .m_resource = gBufferNormalView,
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
                    .SetRenderPassCallback([&deferredShadingPass](auto* _graphicsContext, RenderPassHandle _renderPass) { deferredShadingPass.CreatePso(_graphicsContext, _renderPass); })
                    .SetExecuteFunction([&deferredShadingPass](const auto& _, const auto& _passData) { deferredShadingPass.Render(_, _passData); })
                    .AddColorAttachment(hdrRtv)
                        .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::DontCare)
                        .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                        .Done()
                    .ReadDependency(frameCBufferReadDep)
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
                    .SetRenderPassCallback([&skyPass](auto* _graphicsContext, RenderPassHandle _renderPass) { skyPass.CreatePso(_graphicsContext, _renderPass); })
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
                    .SetRenderPassCallback([&colorMappingPass](auto* _graphicsContext, auto _renderPass) { colorMappingPass.CreatePso(_graphicsContext, _renderPass); })
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
                imGuiContext->RenderFrame(graphicsContext, _passData.m_commandList);
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