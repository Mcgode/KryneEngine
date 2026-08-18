/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#include "TorusKnot.hpp"

#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include <KryneEngine/Core/Math/RotationConversion.hpp>
#include <KryneEngine/Core/Math/Transform.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <fstream>
#include <imgui.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#include "TorusKnotMeshGenerator.hpp"

namespace KryneEngine::Samples::RenderGraphDemo
{
    TorusKnot::TorusKnot(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    void TorusKnot::BuildPso(
        GraphicsContext* _graphicsContext,
        RenderPassHandle _renderPass,
        DescriptorSetLayoutHandle _descriptorSetLayout)
    {
        const auto createShaderModule = [&](const auto& _path) -> ShaderModuleHandle
        {
            auto path = eastl::string(_path) + "." + _graphicsContext->GetShaderFileExtension();;

            std::ifstream file(path.c_str(), std::ios::binary);
            VERIFY_OR_RETURN(file, { GenPool::kInvalidHandle });

            file.seekg(0, std::ios::end);
            const size_t size = file.tellg();
            void* data = m_allocator.allocate(size);
            file.seekg(0, std::ios::beg);

            KE_VERIFY(file.read(reinterpret_cast<char*>(data), size));

            return _graphicsContext->RegisterShaderModule(data, size);
        };

        ShaderModuleHandle vsModule = createShaderModule("Shaders/Samples/RenderGraphDemo/Torus_MainVs");
        ShaderModuleHandle fsModule = createShaderModule("Shaders/Samples/RenderGraphDemo/Torus_MainFs");

        m_pipelineLayout = _graphicsContext->CreatePipelineLayout({
            .m_descriptorSets = { &_descriptorSetLayout, 1 },
        });

        const ShaderStage stages[] {
            {
                .m_shaderModule = vsModule,
                .m_stage = ShaderStage::Stage::Vertex,
                .m_entryPoint = "MainVs",
            },
            {
                .m_shaderModule = fsModule,
                .m_stage = ShaderStage::Stage::Fragment,
                .m_entryPoint = "MainFs",
            }
        };
        const VertexLayoutElement vertexLayoutElements[] {
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Position,
                .m_format = TextureFormat::RGB32_Float,
                .m_offset = 0,
                .m_location = 0,
            },
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Normal,
                .m_format = TextureFormat::RGB32_Float,
                .m_offset = sizeof(float3),
                .m_location = 1,
            }
        };
        const VertexBindingDesc vertexBindings[] {{ .m_stride = sizeof(float3) + sizeof(float3) }};
        const GraphicsPipelineDesc psoDesc = {
            .m_stages = stages,
            .m_vertexInput = {
                .m_elements = vertexLayoutElements,
                .m_bindings = vertexBindings,
            },
            .m_rasterState = {},
            .m_colorBlending = {
                .m_attachments = {
                    ColorAttachmentBlendDesc {},
                    ColorAttachmentBlendDesc {}
                }
            },
            .m_depthStencil = {
                .m_depthCompare = DepthStencilStateDesc::CompareOp::Greater // Use reverse depth
            },
            .m_renderPass = _renderPass,
            .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
            .m_debugName = "TorusKnotPSO",
#endif
        };
        m_pso = _graphicsContext->CreateGraphicsPipeline(psoDesc);

        _graphicsContext->FreeShaderModule(fsModule);
        _graphicsContext->FreeShaderModule(vsModule);
    }

    void TorusKnot::Process(GraphicsContext* _graphicsContext)
    {
        if (m_previousIndexBuffer != GenPool::kInvalidHandle && _graphicsContext->IsFrameExecuted(m_transferFrameId - 1))
        {
            _graphicsContext->DestroyBuffer(m_previousIndexBuffer);
            _graphicsContext->DestroyBuffer(m_previousVertexBuffer);
            m_previousIndexBuffer = GenPool::kInvalidHandle;
            m_previousVertexBuffer = GenPool::kInvalidHandle;
        }

        if (m_transferBuffer != GenPool::kInvalidHandle && _graphicsContext->IsFrameExecuted(m_transferFrameId))
        {
            _graphicsContext->DestroyBuffer(m_transferBuffer);
            m_transferBuffer = GenPool::kInvalidHandle;
        }

        if (m_windowOpen)
        {
            RenderWindow();
        }

        if (!m_meshDirty || m_transferBuffer != GenPool::kInvalidHandle)
        {
            return;
        }

        const TorusKnotMeshGenerator::TorusKnotMesh meshData = TorusKnotMeshGenerator::GenerateMesh(
            m_tubularSegments,
            m_radialSegments,
            m_knotRadius,
            m_tubeRadius,
            m_pValue,
            m_qValue,
            m_allocator);

        m_indexBufferSize = meshData.m_indexCount * sizeof(u32);
        m_vertexBufferSize = meshData.m_vertexCount * TorusKnotMeshGenerator::kVertexSize;
        m_transferBuffer = _graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = m_indexBufferSize + m_vertexBufferSize,
#if !defined(KE_FINAL)
                .m_debugName = "TorusKnotTransferBuffer"
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        });
        m_transferFrameId = _graphicsContext->GetFrameId();

        m_previousIndexBuffer = m_indexBuffer;
        m_previousVertexBuffer = m_vertexBuffer;

        m_indexBuffer = _graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = m_indexBufferSize,
#if !defined(KE_FINAL)
                .m_debugName = "TorusKnotIndexBuffer"
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::IndexBuffer | MemoryUsage::TransferDstBuffer,
        });
        m_vertexBuffer = _graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = m_vertexBufferSize,
#if !defined(KE_FINAL)
                .m_debugName = "TorusKnotVertexBuffer"
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::VertexBuffer | MemoryUsage::TransferDstBuffer,
        });

        {
            BufferMapping mapping(m_transferBuffer, m_indexBufferSize + m_vertexBufferSize);
            _graphicsContext->MapBuffer(mapping);

            memcpy(mapping.m_ptr, meshData.m_indices, m_indexBufferSize);
            memcpy(mapping.m_ptr + m_indexBufferSize, meshData.m_vertices, m_vertexBufferSize);

            _graphicsContext->UnmapBuffer(mapping);
        }

        m_allocator.Delete(meshData.m_indices);
        m_allocator.Delete(meshData.m_vertices);

        m_meshDirty = false;
    }

    void TorusKnot::ProcessTransfers(GraphicsContext* _graphicsContext, CommandListHandle _commandList)
    {
        if (_graphicsContext->GetFrameId() != m_transferFrameId)
        {
            return;
        }

        KE_ZoneScopedFunction("TorusKnot::ProcessTransfers");

        const BufferMemoryBarrier initBarriers[] = {
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferSrc,
                .m_buffer = m_transferBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferDst,
                .m_buffer = m_indexBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferDst,
                .m_buffer = m_vertexBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferSrc,
                .m_buffer = m_transferBuffer,
            }
        };
        _graphicsContext->PlaceMemoryBarriers(_commandList, {}, initBarriers, {});

        _graphicsContext->CopyBuffer(
            _commandList,
            {
                .m_copySize = m_indexBufferSize,
                .m_bufferSrc = m_transferBuffer,
                .m_bufferDst = m_indexBuffer,
                .m_offsetSrc = 0,
            });
        _graphicsContext->CopyBuffer(
            _commandList,
            {
                .m_copySize = m_vertexBufferSize,
                .m_bufferSrc = m_transferBuffer,
                .m_bufferDst = m_vertexBuffer,
                .m_offsetSrc = m_indexBufferSize,
            });

        const BufferMemoryBarrier postCopyBufferBarriers[] = {
            {
                .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                .m_stagesDst = BarrierSyncStageFlags::IndexInputAssembly,
                .m_accessSrc = BarrierAccessFlags::TransferSrc,
                .m_accessDst = BarrierAccessFlags::IndexBuffer,
                .m_buffer = m_indexBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                .m_stagesDst = BarrierSyncStageFlags::VertexInputAssembly,
                .m_accessSrc = BarrierAccessFlags::TransferSrc,
                .m_accessDst = BarrierAccessFlags::VertexBuffer,
                .m_buffer = m_vertexBuffer,
            }
        };
        _graphicsContext->PlaceMemoryBarriers(
            _commandList,
            {},
            postCopyBufferBarriers,
            {});
    }

    void TorusKnot::RenderGBuffer(
        GraphicsContext* _graphicsContext,
        CommandListHandle _commandList,
        DescriptorSetHandle _sceneConstantsSet)
    {
        const BufferSpan vertexBufferView = {
            .m_size = m_vertexBufferSize,
            .m_offset = 0,
            .m_buffer = m_vertexBuffer,
        };
        const BufferSpan indexBufferView = {
            .m_size = m_indexBufferSize,
            .m_offset = 0,
            .m_buffer = m_indexBuffer,
        };

        _graphicsContext->SetVertexBuffers(_commandList, { &vertexBufferView, 1 });
        _graphicsContext->SetIndexBuffer(_commandList, indexBufferView, false);

        _graphicsContext->SetGraphicsPipeline(_commandList, m_pso);
        _graphicsContext->SetGraphicsDescriptorSets(
            _commandList,
            m_pipelineLayout,
            { &_sceneConstantsSet, 1 });

        _graphicsContext->DrawIndexedInstanced(
            _commandList,
            {
                .m_elementCount = static_cast<u32>(m_indexBufferSize / sizeof(u32)),
            });
    }

    void TorusKnot::RenderWindow()
    {
        if (!ImGui::Begin("Torus knot", &m_windowOpen))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Transform parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool dirty = false;

            dirty |= ImGui::SliderFloat3("Position", m_translation.GetPtr(), -25, 25);

            auto euler = Math::ToEulerAngles<float3>(m_rotation) * (180. / M_PI);
            if (ImGui::SliderFloat3("Rotation", euler.GetPtr(), -180, 180))
            {
                dirty = true;
                m_rotation = Math::FromEulerAngles<float>(euler * (M_PI / 180.));
            }

            dirty |= ImGui::SliderFloat3("Scale", m_scale.GetPtr(), 0.f, 10.0f);

            if (dirty)
            {
                m_modelMatrix = Math::ComputeTransformMatrix<float4x4>(m_translation, m_rotation, m_scale);
            }
        }

        if (ImGui::CollapsingHeader("Material parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::ColorEdit3("Albedo", m_albedo.GetPtr());
            ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Metalness", &m_metalness, 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Geometry parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const u32 kMinPValue = 1;
            const u32 kMaxPValue = 10;
            m_meshDirty |= ImGui::SliderScalar(
                "P value",
                ImGuiDataType_U32,
                &m_pValue,
                &kMinPValue,
                &kMaxPValue);

            const u32 kMinQValue = 1;
            const u32 kMaxQValue = 10;
            m_meshDirty |= ImGui::SliderScalar(
                "Q value",
                ImGuiDataType_U32,
                &m_qValue,
                &kMinQValue,
                &kMaxQValue);

            const u32 kMinRadialSegments = 3;
            const u32 kMaxRadialSegments = 512;
            m_meshDirty |= ImGui::SliderScalar(
                "Radial segments",
                ImGuiDataType_U32,
                &m_radialSegments,
                &kMinRadialSegments,
                &kMaxRadialSegments);

            const u32 kMinTubularSegments = 16;
            const u32 kMaxTubularSegments = 2048;
            m_meshDirty |= ImGui::SliderScalar(
                "Tubular segments",
                ImGuiDataType_U32,
                &m_tubularSegments,
                &kMinTubularSegments,
                &kMaxTubularSegments);

            m_meshDirty |= ImGui::SliderFloat("Knot radius", &m_knotRadius, 0.0f, 10.0f);
            m_meshDirty |= ImGui::SliderFloat("Tube radius", &m_tubeRadius, 0.0f, 1.0f);
        }

        ImGui::End();
    }
}
