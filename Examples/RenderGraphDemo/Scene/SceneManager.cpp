/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#include "SceneManager.hpp"

#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include <KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
#include <KryneEngine/Modules/RenderGraph/Registry.hpp>

#include "OrbitCamera.hpp"
#include "SunLight.hpp"
#include "TorusKnot.hpp"

namespace KryneEngine::Samples::RenderGraphDemo
{
    struct alignas(16) SceneConstants
    {
        float4x4 m_torusKnotModel;

        float4x4 m_viewProjection;

        float4x4 m_torusKnotInverseWorldMatrix;

        float3 m_torusKnotAlbedo;
        u32 m_torusKnotQ;

        float3 m_sunLightDirection;
        u32 m_torusKnotP;

        float3 m_sunDiffuse;
        float m_tanHalfFov;

        float2 m_screenResolution;
        float2 m_depthLinearizationConstants;

        float4 m_cameraQuaternion;

        float3 m_cameraTranslation;
        float m_torusKnotTubeRadius;

        float m_torusKnotRadius;
        float m_torusRoughness;
        float m_torusMetalness;
        uint m_padding[1];
    };

    SceneManager::SceneManager(
        AllocatorInstance _allocator,
        Window& _window,
        Modules::RenderGraph::Registry& _registry)
            : m_allocator(_allocator)
            , m_torusKnot(nullptr, _allocator)
            , m_orbitCamera(nullptr, _allocator)
            , m_sunLight(nullptr, _allocator)
            , m_sceneConstantsBuffer(_allocator)
            , m_sceneCbvs(_allocator)
            , m_sceneDescriptorSetIndices(_allocator)
            , m_sceneDescriptorSets(_allocator)
    {
        m_torusKnot.reset(m_allocator.New<TorusKnot>(m_allocator));

        GraphicsContext* graphicsContext = _window.GetGraphicsContext();
        m_windowSize = graphicsContext->GetPresentFrameBufferSize();
        const float aspectRatio = static_cast<float>(m_windowSize.x) / static_cast<float>(m_windowSize.y);
        m_orbitCamera.reset(m_allocator.New<OrbitCamera>(_window.GetInputManager(), aspectRatio));

        m_sunLight.reset(m_allocator.New<SunLight>());

        m_sceneConstantsBuffer.Init(
            graphicsContext,
            {
                .m_desc = {
                    .m_size = sizeof(SceneConstants),
#if !defined(KE_FINAL)
                    .m_debugName = "SceneConstants",
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer | MemoryUsage::ConstantBuffer,
            },
            graphicsContext->GetFrameContextCount());
        m_sceneCbvs.Resize(graphicsContext->GetFrameContextCount());
        for (auto i = 0u; i < graphicsContext->GetFrameContextCount(); ++i)
        {
            m_sceneCbvs[i] = graphicsContext->CreateBufferView(
                BufferViewDesc{
                    .m_buffer = m_sceneConstantsBuffer.GetBuffer(i),
                    .m_size = sizeof(SceneConstants),
                    .m_offset = 0,
                    .m_accessType = BufferViewAccessType::Constant,
                });
        }

        const DescriptorBindingDesc bindings[] {
            {
                .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                .m_visibility = ShaderVisibility::All
            }
        };
        const DescriptorSetDesc sceneDesc = {
            .m_bindings = bindings
        };

        m_sceneDescriptorSetIndices.resize(sceneDesc.m_bindings.size());
        m_sceneDescriptorSetLayout = graphicsContext->CreateDescriptorSetLayout(
            sceneDesc,
            m_sceneDescriptorSetIndices.data());

        m_sceneDescriptorSets.Resize(m_sceneCbvs.Size());
        for (auto i = 0; i < m_sceneDescriptorSets.Size(); ++i)
        {
            DescriptorSetHandle& set = m_sceneDescriptorSets[i];
            set = graphicsContext->CreateDescriptorSet(m_sceneDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData data[] {{ .m_handle = m_sceneCbvs[i].m_handle }};
            const DescriptorSetWriteInfo writeInfo {
                .m_index = m_sceneDescriptorSetIndices[0],
                .m_descriptorData = data,
            };
            graphicsContext->UpdateDescriptorSet(set, {&writeInfo, 1}, false);
        }

        m_cbRenderGraphHandles.Resize(graphicsContext->GetFrameContextCount());
        m_cbvRenderGraphHandles.Resize(graphicsContext->GetFrameContextCount());
        for (auto i = 0u; i < m_cbRenderGraphHandles.Size(); ++i)
        {
            m_cbRenderGraphHandles[i] = _registry.RegisterRawBuffer(m_sceneConstantsBuffer.GetBuffer(i));
            m_cbvRenderGraphHandles[i] = _registry.RegisterBufferView(m_sceneCbvs[i], m_cbRenderGraphHandles[i]);
        }
    }

    SceneManager::~SceneManager() = default;

    void SceneManager::PreparePsos(GraphicsContext* _graphicsContext, RenderPassHandle _dummyGBufferRenderPass)
    {
        m_torusKnot->BuildPso(
            _graphicsContext,
            _dummyGBufferRenderPass,
            m_sceneDescriptorSetLayout);
    }

    void SceneManager::DeclareDataTransferPass(
        const GraphicsContext* _graphicsContext,
        Modules::RenderGraph::Builder& _builder,
        Modules::ImGui::Context* _imGuiContext)
    {
        const u8 index = _graphicsContext->GetCurrentFrameContextIndex();;

        m_currentCbv = m_cbvRenderGraphHandles[index];

        const auto transferExecuteFunction =
            [this, _imGuiContext](Modules::RenderGraph::RenderGraph& _renderGraph, Modules::RenderGraph::PassExecutionData _passData)
        {
            ExecuteTransfers(_passData.m_graphicsContext, _passData.m_commandList);
            _imGuiContext->PrepareToRenderFrame(_passData.m_graphicsContext, _passData.m_commandList);
        };

        _builder
            .DeclarePass(Modules::RenderGraph::PassType::Transfer)
            .SetName("Scene data transfer pass")
            .SetExecuteFunction(transferExecuteFunction)
            .WriteDependency({
                .m_resource = m_cbRenderGraphHandles[index],
                .m_finalSyncStage { BarrierSyncStageFlags::All },
                .m_finalAccessFlags { BarrierAccessFlags::ConstantBuffer }
            });
    }

    void SceneManager::Process(GraphicsContext* _graphicsContext)
    {
        m_torusKnot->Process(_graphicsContext);
        m_orbitCamera->Process();
        m_sunLight->Process();

        auto* sceneConstants = static_cast<SceneConstants*>(m_sceneConstantsBuffer.Map(
            _graphicsContext,
            _graphicsContext->GetCurrentFrameContextIndex()));

        sceneConstants->m_screenResolution = float2(m_windowSize);

        sceneConstants->m_torusKnotModel = m_torusKnot->GetModelMatrix();
        sceneConstants->m_torusKnotInverseWorldMatrix = m_torusKnot->GetModelMatrix().Inverse();
        sceneConstants->m_torusKnotAlbedo = m_torusKnot->GetAlbedo();
        sceneConstants->m_torusKnotRadius = m_torusKnot->GetKnotRadius();
        sceneConstants->m_torusKnotTubeRadius = m_torusKnot->GetTubeRadius();
        sceneConstants->m_torusKnotP = m_torusKnot->GetPValue();
        sceneConstants->m_torusKnotQ = m_torusKnot->GetQValue();
        sceneConstants->m_torusRoughness = m_torusKnot->GetRoughness();
        sceneConstants->m_torusMetalness = m_torusKnot->GetMetalness();

        sceneConstants->m_sunLightDirection = m_sunLight->GetDirection();
        sceneConstants->m_sunDiffuse = m_sunLight->GetDiffuse();

        sceneConstants->m_tanHalfFov = tanf(m_orbitCamera->GetFov() * 0.5f);
        sceneConstants->m_viewProjection = m_orbitCamera->GetProjectionViewMatrix();
        sceneConstants->m_cameraTranslation = m_orbitCamera->GetViewTranslation();

        const Math::Quaternion& rotation = m_orbitCamera->GetViewRotation();
        sceneConstants->m_cameraQuaternion = float4(rotation.x, rotation.y, rotation.z, rotation.w);
        sceneConstants->m_depthLinearizationConstants = m_orbitCamera->GetDepthLinearizeConstants();

        m_sceneConstantsBuffer.Unmap(_graphicsContext);
    }

    void SceneManager::ExecuteTransfers(GraphicsContext* _graphicsContext, CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("SceneManager::ExecuteTransfers");

        m_sceneConstantsBuffer.PrepareBuffers(
            _graphicsContext,
            _commandList,
            BarrierAccessFlags::ConstantBuffer,
            _graphicsContext->GetCurrentFrameContextIndex());

        m_torusKnot->ProcessTransfers(_graphicsContext, _commandList);
    }

    void SceneManager::RenderGBuffer(GraphicsContext* _graphicsContext, CommandListHandle _commandList)
    {
        _graphicsContext->DeclarePassBufferViewUsage(
            _commandList,
            { &m_sceneCbvs[_graphicsContext->GetCurrentFrameContextIndex()], 1 },
            BufferViewAccessType::Constant);
        _graphicsContext->SetViewport(
            _commandList,
            Viewport { .m_width = static_cast<s32>(m_windowSize.x), .m_height = static_cast<s32>(m_windowSize.y) });
        _graphicsContext->SetScissorsRect(
            _commandList,
            Rect { .m_left = 0, .m_top = 0, .m_right = m_windowSize.x, .m_bottom = m_windowSize.y });
        m_torusKnot->RenderGBuffer(
            _graphicsContext,
            _commandList,
            m_sceneDescriptorSets[_graphicsContext->GetCurrentFrameContextIndex()]);
    }
}
