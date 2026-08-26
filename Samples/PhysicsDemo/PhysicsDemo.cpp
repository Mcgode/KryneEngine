/**
 * @file
 * @author Max Godefroy
 * @date 18/08/2026.
 */

#include "Src/SceneManager.hpp"


#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/Box3D/Context.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
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

            // Even if it's a dummy pass, the generated render pass should match signature with the one in the render
            // graph for the ImGui pass, so it will be reused there.

            RenderGraph::PassDeclaration imguiDummyPass(RenderGraph::PassType::Render, 0);
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

        RenderGraph::Builder& builder = renderGraph.BeginFrame(*graphicsContext);

        SimplePoolHandle swapChainTexture = swapChainTextures[graphicsContext->GetCurrentPresentImageIndex()];
        SimplePoolHandle swapChainRtv = swapChainRtvs[graphicsContext->GetCurrentPresentImageIndex()];

        ::ImGui::ShowDemoWindow();

        builder
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