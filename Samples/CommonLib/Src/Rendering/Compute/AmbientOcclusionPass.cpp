/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#include "Rendering/Compute/AmbientOcclusionPass.hpp"

#include "Samples/CommonLib/AmbientOcclusionConstants.h"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Graphics/Enums.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
#include <fstream>
#include <imgui.h>

namespace KryneEngine::Samples
{
    AmbientOcclusionPass::AmbientOcclusionPass(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_constantsBuffer(_allocator)
    {}

    AmbientOcclusionPass::~AmbientOcclusionPass()
    {
        if (m_constantsBufferViews != nullptr)
            m_allocator.deallocate(m_constantsBufferViews);
    }

    void AmbientOcclusionPass::Initialize(
        GraphicsContext* _graphicsContext,
        const TextureViewHandle _gBufferDepth,
        const TextureViewHandle _gBufferNormal,
        const TextureViewHandle _aoTermA,
        const TextureViewHandle _aoTermB,
        const TextureViewHandle _aoEdges,
        const TextureHandle _aoTextures)
    {
        m_aoTermA = _aoTermA;
        m_aoTermB = _aoTermB;
        m_aoTextures = _aoTextures;

        const u8 frameContextCount = _graphicsContext->GetFrameContextCount();
        m_constantsBuffer.Init(
            _graphicsContext,
            {
                .m_desc = {
                    .m_size = sizeof(AmbientOcclusionConstants),
#if !defined(KE_FINAL)
                    .m_debugName = "AO constants",
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer
                    | MemoryUsage::ConstantBuffer,
            },
            frameContextCount);

        m_constantsBufferViews = m_allocator.Allocate<BufferViewHandle>(frameContextCount);
        for (u32 i = 0; i < frameContextCount; i++)
        {
            char name[64];
            snprintf(name, sizeof(name), "AO constants view %u", i);
            m_constantsBufferViews[i] = _graphicsContext->CreateBufferView({
                .m_buffer = m_constantsBuffer.GetBuffer(i),
                .m_size = sizeof(AmbientOcclusionConstants),
                .m_stride = sizeof(AmbientOcclusionConstants),
                .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                .m_debugName = name,
#endif
            });
        }

        // Main descriptor set layout + bindings
        {
            constexpr DescriptorBindingDesc bindings[] {
                // Fullscreen pass constants (camera reconstruction)
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // AO tunables
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // GBuffer depth
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // GBuffer normal
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // Output raw AO term
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadWriteTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                // Output packed depth edges
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadWriteTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
            };
            m_mainDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                m_mainIndices.Get());
        }

        m_mainDescriptorSet = _graphicsContext->CreateDescriptorSet(m_mainDescriptorSetLayout);

        {
            const DescriptorSetWriteInfo::DescriptorData depthData[] {
                { .m_textureLayout = TextureLayout::ShaderResource, .m_handle = _gBufferDepth.m_handle }
            };
            const DescriptorSetWriteInfo::DescriptorData normalData[] {
                { .m_textureLayout = TextureLayout::ShaderResource, .m_handle = _gBufferNormal.m_handle }
            };
            const DescriptorSetWriteInfo::DescriptorData aoTermOutData[] {
                { .m_textureLayout = TextureLayout::UnorderedAccess, .m_handle = _aoTermA.m_handle }
            };
            const DescriptorSetWriteInfo::DescriptorData edgesOutData[] {
                { .m_textureLayout = TextureLayout::UnorderedAccess, .m_handle = _aoEdges.m_handle }
            };
            const DescriptorSetWriteInfo writes[] {
                { .m_index = m_mainIndices.m_gBufferDepth, .m_descriptorData = depthData },
                { .m_index = m_mainIndices.m_gBufferNormal, .m_descriptorData = normalData },
                { .m_index = m_mainIndices.m_aoTermOut, .m_descriptorData = aoTermOutData },
                { .m_index = m_mainIndices.m_edgesOut, .m_descriptorData = edgesOutData },
            };
            _graphicsContext->UpdateDescriptorSet(m_mainDescriptorSet, writes, false);
        }

        {
            const DescriptorSetLayoutHandle sets[] { m_mainDescriptorSetLayout };
            m_mainPipelineLayout = _graphicsContext->CreatePipelineLayout({ .m_descriptorSets = sets });
        }

        // Denoise descriptor sets (ping-pong: index 0 writes aoTermB, index 1 writes aoTermA)
        {
            constexpr DescriptorBindingDesc bindings[] {
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Compute,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadWriteTexture,
                    .m_visibility = ShaderVisibility::Compute,
                },
            };
            m_denoiseDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                m_denoiseIndices.Get());
        }

        const TextureViewHandle denoiseSource[2] { _aoTermA, _aoTermB };
        const TextureViewHandle denoiseDest[2] { _aoTermB, _aoTermA };
        for (u32 i = 0; i < 2; i++)
        {
            m_denoiseDescriptorSets[i] = _graphicsContext->CreateDescriptorSet(m_denoiseDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData sourceAoTermData[] {
                { .m_textureLayout = TextureLayout::ShaderResource, .m_handle = denoiseSource[i].m_handle }
            };
            const DescriptorSetWriteInfo::DescriptorData sourceEdgesData[] {
                { .m_textureLayout = TextureLayout::ShaderResource, .m_handle = _aoEdges.m_handle }
            };
            const DescriptorSetWriteInfo::DescriptorData destAoTermData[] {
                { .m_textureLayout = TextureLayout::UnorderedAccess, .m_handle = denoiseDest[i].m_handle }
            };
            const DescriptorSetWriteInfo writes[] {
                { .m_index = m_denoiseIndices.m_sourceAoTerm, .m_descriptorData = sourceAoTermData },
                { .m_index = m_denoiseIndices.m_sourceEdges, .m_descriptorData = sourceEdgesData },
                { .m_index = m_denoiseIndices.m_aoTermOut, .m_descriptorData = destAoTermData },
            };
            _graphicsContext->UpdateDescriptorSet(m_denoiseDescriptorSets[i], writes, false);
        }

        {
            const DescriptorSetLayoutHandle sets[] { m_denoiseDescriptorSetLayout };
            m_denoisePipelineLayout = _graphicsContext->CreatePipelineLayout({ .m_descriptorSets = sets });
        }
    }

    void AmbientOcclusionPass::UpdateSceneConstants(
        GraphicsContext* _graphicsContext,
        const BufferViewHandle _fullscreenConstantsBufferView) const
    {
        const DescriptorSetWriteInfo::DescriptorData data[] { { .m_handle = _fullscreenConstantsBufferView.m_handle } };
        const DescriptorSetWriteInfo writes[] { { .m_index = m_mainIndices.m_fullscreenConstants, .m_descriptorData = data } };
        _graphicsContext->UpdateDescriptorSet(m_mainDescriptorSet, writes, true);
    }

    void AmbientOcclusionPass::UpdateConstants(
        GraphicsContext* _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        const u8 frameIndex = _graphicsContext->GetCurrentFrameContextIndex();

        auto* constants = static_cast<AmbientOcclusionConstants*>(m_constantsBuffer.Map(_graphicsContext, frameIndex));
        constants->m_effectRadius = m_effectRadius;
        constants->m_falloffRangeFraction = m_falloffRangeFraction;
        constants->m_sampleDistributionPower = m_sampleDistributionPower;
        constants->m_thinOccluderCompensation = m_thinOccluderCompensation;
        constants->m_finalValuePower = m_finalValuePower;
        constants->m_denoiseBlurBeta = m_denoiseBlurBeta;
        constants->m_intensity = m_intensity;
        constants->m_sliceCount = static_cast<u32>(m_sliceCount);
        constants->m_stepsPerSlice = static_cast<u32>(m_stepsPerSlice);
        m_constantsBuffer.Unmap(_graphicsContext);

        m_constantsBuffer.PrepareBuffers(_graphicsContext, _transferEncoder, BarrierAccessFlags::ConstantBuffer, frameIndex);

        const BufferViewHandle constantsView = m_constantsBufferViews[frameIndex];

        const DescriptorSetWriteInfo::DescriptorData data[] { { .m_handle = constantsView.m_handle } };
        {
            const DescriptorSetWriteInfo writes[] { { .m_index = m_mainIndices.m_aoConstants, .m_descriptorData = data } };
            _graphicsContext->UpdateDescriptorSet(m_mainDescriptorSet, writes, true);
        }
        for (u32 i = 0; i < 2; i++)
        {
            const DescriptorSetWriteInfo writes[] { { .m_index = m_denoiseIndices.m_aoConstants, .m_descriptorData = data } };
            _graphicsContext->UpdateDescriptorSet(m_denoiseDescriptorSets[i], writes, true);
        }
    }

    void AmbientOcclusionPass::CreatePso(GraphicsContext* _graphicsContext)
    {
        if (m_mainPso == GenPool::kInvalidHandle)
        {
            char path[256];
            snprintf(
                path,
                sizeof(path),
                "Shaders/Samples/CommonLib/AmbientOcclusion_AmbientOcclusionMain.%s",
                GraphicsContext::GetShaderFileExtension());

            std::ifstream file(path, std::ios::binary);
            KE_ASSERT_MSG(file, "Could not open ambient occlusion main shader");

            file.seekg(0, std::ios::end);
            const size_t size = file.tellg();
            void* bytecode = m_allocator.allocate(size);
            file.seekg(0, std::ios::beg);
            KE_VERIFY(file.read(static_cast<char*>(bytecode), size));

            const ShaderModuleHandle module = _graphicsContext->RegisterShaderModule(bytecode, size);

            m_mainPso = _graphicsContext->CreateComputePipeline({
                .m_computeStage = {
                    .m_shaderModule = module,
                    .m_stage = ShaderStage::Stage::Compute,
                    .m_entryPoint = "AmbientOcclusionMain",
                },
                .m_pipelineLayout = m_mainPipelineLayout,
#if !defined(KE_FINAL)
                .m_debugName = "AmbientOcclusionMainPSO",
#endif
            });

            _graphicsContext->FreeShaderModule(module);
            m_allocator.deallocate(bytecode);
        }

        if (m_denoisePso == GenPool::kInvalidHandle)
        {
            char path[256];
            snprintf(
                path,
                sizeof(path),
                "Shaders/Samples/CommonLib/AmbientOcclusionDenoise_AmbientOcclusionDenoiseMain.%s",
                GraphicsContext::GetShaderFileExtension());

            std::ifstream file(path, std::ios::binary);
            KE_ASSERT_MSG(file, "Could not open ambient occlusion denoise shader");

            file.seekg(0, std::ios::end);
            const size_t size = file.tellg();
            void* bytecode = m_allocator.allocate(size);
            file.seekg(0, std::ios::beg);
            KE_VERIFY(file.read(static_cast<char*>(bytecode), size));

            const ShaderModuleHandle module = _graphicsContext->RegisterShaderModule(bytecode, size);

            m_denoisePso = _graphicsContext->CreateComputePipeline({
                .m_computeStage = {
                    .m_shaderModule = module,
                    .m_stage = ShaderStage::Stage::Compute,
                    .m_entryPoint = "AmbientOcclusionDenoiseMain",
                },
                .m_pipelineLayout = m_denoisePipelineLayout,
#if !defined(KE_FINAL)
                .m_debugName = "AmbientOcclusionDenoisePSO",
#endif
            });

            _graphicsContext->FreeShaderModule(module);
            m_allocator.deallocate(bytecode);
        }
    }

    void AmbientOcclusionPass::DeclareRenderGraphPass(
        Modules::RenderGraph::Builder& _builder,
        const SimplePoolHandle _gBufferDepthView,
        const SimplePoolHandle _gBuffer1View,
        const SimplePoolHandle _aoTermAView,
        const SimplePoolHandle _aoTermBView,
        const SimplePoolHandle _aoEdgesView,
        const uint2 _renderSize)
    {
        _builder.DeclarePass(Modules::RenderGraph::PassType::Compute)
            .SetName("Ambient occlusion pass")
            .ReadDependency({
                .m_resource = _gBufferDepthView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
                .m_planes = TexturePlane::Depth,
            })
            .ReadDependency({
                .m_resource = _gBuffer1View,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
            })
            // Entry/exit state only; inter-dispatch transitions are placed by hand in #Dispatch.
            .WriteDependency({
                .m_resource = _aoTermAView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                .m_targetLayout = TextureLayout::UnorderedAccess,
            })
            .WriteDependency({
                .m_resource = _aoTermBView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                .m_targetLayout = TextureLayout::UnorderedAccess,
            })
            .WriteDependency({
                .m_resource = _aoEdgesView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                .m_targetLayout = TextureLayout::UnorderedAccess,
            })
            .SetPrePassTransferFunction([this](GraphicsContext* _graphicsContext, const TransferCommandEncoderHandle _transferEncoder)
            {
                UpdateConstants(_graphicsContext, _transferEncoder);
            })
            .SetExecuteFunction([this, _renderSize](const auto& _renderGraph, const auto& _executionData)
            {
                Dispatch(_renderGraph, _executionData, _renderSize);
            })
            .Done();
    }

    void AmbientOcclusionPass::Dispatch(
        const Modules::RenderGraph::RenderGraph& _renderGraph,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData,
        const uint2 _renderSize) const
    {
        if (m_mainPso == GenPool::kInvalidHandle || m_denoisePso == GenPool::kInvalidHandle)
            return;

        GraphicsContext* graphicsContext = _passExecutionData.m_graphicsContext;
        const ComputeCommandEncoderHandle computeEncoder = _passExecutionData.m_computeEncoder;

        DispatchMain(_renderGraph, _passExecutionData, _renderSize);

        // aoTermA + aoEdges: UAV (written by the main dispatch) -> SRV (read by the first denoise
        // dispatch).
        {
            const TextureMemoryBarrier barriers[] = {
                {
                    .m_stagesSrc = BarrierSyncStageFlags::ComputeShading,
                    .m_stagesDst = BarrierSyncStageFlags::ComputeShading,
                    .m_accessSrc = BarrierAccessFlags::UnorderedAccess,
                    .m_accessDst = BarrierAccessFlags::ShaderResource,
                    .m_texture = m_aoTextures,
                    .m_arrayStart = kAoTermASlice,
                    .m_arrayCount = 1,
                    .m_layoutSrc = TextureLayout::UnorderedAccess,
                    .m_layoutDst = TextureLayout::ShaderResource,
                },
                {
                    .m_stagesSrc = BarrierSyncStageFlags::ComputeShading,
                    .m_stagesDst = BarrierSyncStageFlags::ComputeShading,
                    .m_accessSrc = BarrierAccessFlags::UnorderedAccess,
                    .m_accessDst = BarrierAccessFlags::ShaderResource,
                    .m_texture = m_aoTextures,
                    .m_arrayStart = kAoEdgesSlice,
                    .m_arrayCount = 1,
                    .m_layoutSrc = TextureLayout::UnorderedAccess,
                    .m_layoutDst = TextureLayout::ShaderResource,
                },
            };
            graphicsContext->PlaceMemoryBarriers(
                computeEncoder,
                { .m_placementType = BarrierPlacementType::IntraEncoder, .m_textureBarriers = barriers });
        }

        DispatchDenoise(_renderGraph, _passExecutionData, _renderSize, 0);

        // aoTermB: UAV (written by the first denoise dispatch) -> SRV (read by the second).
        // aoTermA: SRV (read by the first denoise dispatch) -> UAV (written by the second), so it
        // ends the pass holding the final result (see #GetFinalOutputView).
        {
            const TextureMemoryBarrier barriers[] = {
                {
                    .m_stagesSrc = BarrierSyncStageFlags::ComputeShading,
                    .m_stagesDst = BarrierSyncStageFlags::ComputeShading,
                    .m_accessSrc = BarrierAccessFlags::UnorderedAccess,
                    .m_accessDst = BarrierAccessFlags::ShaderResource,
                    .m_texture = m_aoTextures,
                    .m_arrayStart = kAoTermBSlice,
                    .m_arrayCount = 1,
                    .m_layoutSrc = TextureLayout::UnorderedAccess,
                    .m_layoutDst = TextureLayout::ShaderResource,
                },
                {
                    .m_stagesSrc = BarrierSyncStageFlags::ComputeShading,
                    .m_stagesDst = BarrierSyncStageFlags::ComputeShading,
                    .m_accessSrc = BarrierAccessFlags::ShaderResource,
                    .m_accessDst = BarrierAccessFlags::UnorderedAccess,
                    .m_texture = m_aoTextures,
                    .m_arrayStart = kAoTermASlice,
                    .m_arrayCount = 1,
                    .m_layoutSrc = TextureLayout::ShaderResource,
                    .m_layoutDst = TextureLayout::UnorderedAccess,
                },
            };
            graphicsContext->PlaceMemoryBarriers(
                computeEncoder,
                { .m_placementType = BarrierPlacementType::IntraEncoder, .m_textureBarriers = barriers });
        }

        DispatchDenoise(_renderGraph, _passExecutionData, _renderSize, 1);
    }

    void AmbientOcclusionPass::DispatchMain(
        const Modules::RenderGraph::RenderGraph& /*_renderGraph*/,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData,
        const uint2 _renderSize) const
    {
        if (m_mainPso == GenPool::kInvalidHandle)
            return;

        GraphicsContext* graphicsContext = _passExecutionData.m_graphicsContext;
        const ComputeCommandEncoderHandle computeEncoder = _passExecutionData.m_computeEncoder;

        const DescriptorSetHandle sets[] { m_mainDescriptorSet };
        graphicsContext->SetComputePipeline(computeEncoder, m_mainPso);
        graphicsContext->SetComputeDescriptorSets(computeEncoder, m_mainPipelineLayout, sets);
        graphicsContext->Dispatch(
            computeEncoder,
            { (_renderSize.x + 7) / 8, (_renderSize.y + 7) / 8, 1 },
            { 8, 8, 1 });
    }

    void AmbientOcclusionPass::DispatchDenoise(
        const Modules::RenderGraph::RenderGraph& /*_renderGraph*/,
        const Modules::RenderGraph::PassExecutionData& _passExecutionData,
        const uint2 _renderSize,
        const u32 _passIndex) const
    {
        if (m_denoisePso == GenPool::kInvalidHandle)
            return;

        GraphicsContext* graphicsContext = _passExecutionData.m_graphicsContext;
        const ComputeCommandEncoderHandle computeEncoder = _passExecutionData.m_computeEncoder;

        const DescriptorSetHandle sets[] { m_denoiseDescriptorSets[_passIndex] };
        graphicsContext->SetComputePipeline(computeEncoder, m_denoisePso);
        graphicsContext->SetComputeDescriptorSets(computeEncoder, m_denoisePipelineLayout, sets);
        graphicsContext->Dispatch(
            computeEncoder,
            { (_renderSize.x + 7) / 8, (_renderSize.y + 7) / 8, 1 },
            { 8, 8, 1 });
    }

    void AmbientOcclusionPass::Debug(bool* _windowOpen)
    {
        if (ImGui::Begin("Ambient Occlusion", _windowOpen))
        {
            ImGui::SliderFloat("Intensity", &m_intensity, 0.f, 1.f);
            ImGui::DragFloat("Radius (world units)", &m_effectRadius, 0.01f, 0.01f, 10.f, "%.2f");
            ImGui::SliderFloat("Falloff range (fraction of radius)", &m_falloffRangeFraction, 0.05f, 1.f);
            ImGui::DragFloat("Sample distribution power", &m_sampleDistributionPower, 0.05f, 0.5f, 4.f, "%.2f");
            ImGui::SliderFloat("Thin occluder compensation", &m_thinOccluderCompensation, 0.f, 1.f);
            ImGui::DragFloat("Final value power", &m_finalValuePower, 0.05f, 0.5f, 4.f, "%.2f");
            ImGui::SliderInt("Slice count", &m_sliceCount, 1, 9);
            ImGui::SliderInt("Steps per slice", &m_stepsPerSlice, 1, 9);
            ImGui::DragFloat("Denoise blur beta", &m_denoiseBlurBeta, 0.05f, 0.f, 5.f, "%.2f");
        }
        ImGui::End();
    }
}
