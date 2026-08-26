/**
 * @file
 * @author Max Godefroy
 * @date 23/08/2026.
 */

#include "Rendering/PassDispatcher.hpp"

#include "Rendering/DrawInstanceManager.hpp"
#include <EASTL/sort.h>
#include <KryneEngine/Core/Graphics/Drawing.hpp>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>


namespace KryneEngine::Samples
{
    void PassDispatcher::PrepareDispatch(
        const float4x4& _viewMatrix,
        const float4x4& _projectionMatrix,
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        m_dispatchData.emplace();
        m_dispatchData->m_models.set_allocator(m_drawInstanceManager->m_allocator);
        m_dispatchData->m_modelInstanceOffsets.set_allocator(m_drawInstanceManager->m_allocator);

        size_t totalInstances = 0;

        for (size_t i = 0; i < m_drawInstanceManager->m_validModels.size(); ++i)
        {
            u64 bits = m_drawInstanceManager->m_validModels[i];
            while (bits != 0)
            {
                const size_t index = std::countr_zero(bits);
                bits &= bits - 1;

                const SimplePoolHandle handle = i * 64 + index;
                const DrawInstanceManager::Model& model = m_drawInstanceManager->m_models.Get(handle);
                const MaterialHandle material = model.m_material;
                if (m_materialManager->GetGraphicsPipeline(material, m_passType) != GenPool::kInvalidHandle)
                {
                    m_dispatchData->m_models.push_back_unsorted(handle);
                    m_dispatchData->m_modelInstanceOffsets.push_back(totalInstances);
                    totalInstances += model.m_instanceCount;
                }
            }
        }

        if (totalInstances > m_instanceBuffer.GetSize(_graphicsContext.GetCurrentFrameContextIndex()))
        {
            m_instanceBuffer.RequestResize(Alignment::AlignUp(totalInstances, 128uz));
        }

        // Update and transfer instances buffer
        {
            const DynamicArray<u64> instanceCounts {
                m_drawInstanceManager->m_allocator,
                m_dispatchData->m_models.size(),
                0
            };

            void* instanceBuffer = m_instanceBuffer.Map(&_graphicsContext, _graphicsContext.GetCurrentFrameContextIndex());
            auto* instanceIds = static_cast<u32*>(instanceBuffer);
            for (size_t i = 0; i < m_drawInstanceManager->m_instanceData.size(); ++i)
            {
                const DrawInstanceManager::Instance& instance = m_drawInstanceManager->m_instances.Get(i);
                if (!instance.m_valid)
                    continue;

                const auto it = m_dispatchData->m_models.find(instance.m_model);
                if (it == m_dispatchData->m_models.end())
                    continue;

                const size_t index = eastl::distance(m_dispatchData->m_models.begin(), it);
                const u64 offset = m_dispatchData->m_modelInstanceOffsets[index];
                u64& count = instanceCounts[index];
                instanceIds[offset + count++] = i;
            }
            m_instanceBuffer.Unmap(&_graphicsContext);

            m_instanceBuffer.PrepareBuffers(
                &_graphicsContext,
                _transferEncoder,
                BarrierAccessFlags::VertexBuffer,
                _graphicsContext.GetCurrentFrameContextIndex());
        }

        // Update and transfer constant buffer
        {
            auto* cbData = static_cast<PassConstantBuffer*>(m_constantBuffer.Map(
                &_graphicsContext, _graphicsContext.GetCurrentFrameContextIndex()));

            cbData->m_viewMatrix = _viewMatrix;
            cbData->m_projectionMatrix = _projectionMatrix;
            cbData->m_viewProjectionMatrix = _projectionMatrix * _viewMatrix;

            m_constantBuffer.Unmap(&_graphicsContext);
            m_constantBuffer.PrepareBuffers(
                &_graphicsContext,
                _transferEncoder,
                BarrierAccessFlags::ConstantBuffer,
                _graphicsContext.GetCurrentFrameContextIndex());
        }

        // Update descriptor set
        {
            const DescriptorSetWriteInfo::DescriptorData cbData[1] = {
                { .m_handle = m_constantBufferViews[_graphicsContext.GetCurrentFrameContextIndex()].m_handle },
            };

            const DescriptorSetWriteInfo::DescriptorData ibData[1] = {
                { .m_handle = m_drawInstanceManager->m_instanceDataBufferViews[_graphicsContext.GetCurrentFrameContextIndex()].m_handle },
            };

            const DescriptorSetWriteInfo writes[2] = {
                {
                    .m_index = m_drawInstanceManager->m_passCbBindingIndex,
                    .m_descriptorData = cbData,
                },
                {
                    .m_index = m_drawInstanceManager->m_instanceDataBindingIndex,
                    .m_descriptorData = ibData,
                }
            };

            _graphicsContext.UpdateDescriptorSet(m_passDescriptorSet, writes, true);
        }
    }

    void PassDispatcher::Dispatch(GraphicsContext& _graphicsContext, const RenderCommandEncoderHandle _renderEncoder)
    {
        const DynamicArray<u64> sortedModels(m_drawInstanceManager->m_allocator, m_dispatchData->m_models.size());
        eastl::copy(m_dispatchData->m_models.begin(), m_dispatchData->m_models.end(), sortedModels.begin());
        eastl::sort(sortedModels.begin(), sortedModels.end(), [this](const u64 _a, const u64 _b)
        {
            const auto* a = m_materialManager->GetMaterialPipeline(
                m_drawInstanceManager->m_models.Get(m_dispatchData->m_models[_a]).m_material,
                m_passType);
            const auto* b = m_materialManager->GetMaterialPipeline(
                m_drawInstanceManager->m_models.Get(m_dispatchData->m_models[_b]).m_material,
                m_passType);
            return *a < *b;
        });

        PipelineLayoutHandle currentLayout {};

        const BufferSpan instanceVertexBuffer {
            .m_size = m_instanceBuffer.GetSize(_graphicsContext.GetCurrentFrameContextIndex()),
            .m_offset = 0,
            .m_stride = sizeof(u32),
            .m_buffer = m_instanceBuffer.GetBuffer(_graphicsContext.GetCurrentFrameContextIndex()),
        };

        for (const u64 i : sortedModels)
        {
            const DrawInstanceManager::Model& model = m_drawInstanceManager->m_models.Get(m_dispatchData->m_models[i]);

            const MaterialManager::MaterialPipeline* materialPipeline = m_materialManager->GetMaterialPipeline(
                model.m_material, m_passType);

            _graphicsContext.SetIndexBuffer(_renderEncoder, model.m_indexBuffer, false);

            const BufferSpan vertexBuffers[2] = {
                model.m_vertexBuffer,
                instanceVertexBuffer,
            };
            _graphicsContext.SetVertexBuffers(_renderEncoder, vertexBuffers);

            _graphicsContext.SetGraphicsPipeline(_renderEncoder, materialPipeline->m_pipeline);

            if (currentLayout != materialPipeline->m_pipelineLayout)
            {
                _graphicsContext.SetGraphicsDescriptorSetsWithOffset(
                    _renderEncoder,
                    materialPipeline->m_pipelineLayout,
                    { &m_passDescriptorSet, 1 },
                    0);
                currentLayout = materialPipeline->m_pipelineLayout;
            }

            if (materialPipeline->m_descriptorSets[0] != GenPool::kInvalidHandle)
            {
                _graphicsContext.SetGraphicsDescriptorSetsWithOffset(
                    _renderEncoder,
                    currentLayout,
                    {
                        &materialPipeline->m_descriptorSets[0],
                        materialPipeline->m_descriptorSets[1] != GenPool::kInvalidHandle ? 2uz : 1uz
                    },
                    1);
            }

            _graphicsContext.DrawIndexedInstanced(_renderEncoder, {
                .m_elementCount = model.m_vertexCount,
                .m_instanceCount = model.m_instanceCount,
                .m_indexOffset = model.m_indexOffset,
                .m_vertexOffset = model.m_vertexOffset,
                .m_instanceOffset = m_dispatchData->m_modelInstanceOffsets[i],
            });
        }

        m_dispatchData.reset();
    }

    PassDispatcher::PassDispatcher(
        DrawInstanceManager* _drawInstanceManager,
        const MaterialManager* _materialManager,
        GraphicsContext& _graphicsContext,
        const u8 _passType,
        const eastl::string_view _debugName)
            : m_drawInstanceManager(_drawInstanceManager)
            , m_materialManager(_materialManager)
            , m_passType(_passType)
            , m_instanceBuffer(m_drawInstanceManager->m_allocator)
            , m_constantBuffer(m_drawInstanceManager->m_allocator)
    {
        m_passDescriptorSet = _graphicsContext.CreateDescriptorSet(
            m_drawInstanceManager->GetPassDescriptorSetLayout(_graphicsContext));

        char name[256];

        snprintf(name, sizeof(name), "%s Instance Buffer", _debugName.data());
        m_instanceBuffer.Init(
            &_graphicsContext,
            {
                .m_desc = {
                    .m_size = Alignment::AlignUp(m_drawInstanceManager->m_instanceData.size() + 1, 128uz),
#if !defined(KE_FINAL)
                    .m_debugName = name,
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer | MemoryUsage::VertexBuffer,
            },
            _graphicsContext.GetFrameContextCount());

        snprintf(name, sizeof(name), "%s Constant Buffer", _debugName.data());
        m_constantBuffer.Init(
            &_graphicsContext,
            {
                .m_desc = {
                    .m_size = sizeof(PassConstantBuffer),
#if !defined(KE_FINAL)
                    .m_debugName = name,
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer | MemoryUsage::ConstantBuffer,
            },
            _graphicsContext.GetFrameContextCount());

        m_constantBufferViews = m_drawInstanceManager->m_allocator.Allocate<BufferViewHandle>(_graphicsContext.GetFrameContextCount());
        for (size_t i = 0; i < _graphicsContext.GetFrameContextCount(); i++)
        {
            snprintf(name, sizeof(name), "%s Constant Buffer View %zu", _debugName.data(), i);
            m_constantBufferViews[i] = _graphicsContext.CreateBufferView({
                .m_buffer = m_constantBuffer.GetBuffer(i),
                .m_size = m_constantBuffer.GetSize(i),
                .m_offset = 0,
                .m_stride = static_cast<u32>(m_constantBuffer.GetSize(i)),
                .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                .m_debugName = name,
#endif
            });
        }
    }
} // namespace KryneEngine::Samples