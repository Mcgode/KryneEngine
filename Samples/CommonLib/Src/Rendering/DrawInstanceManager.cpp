/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#include "Rendering/DrawInstanceManager.hpp"


#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <KryneEngine/Core/Memory/SimplePool.inl>


namespace KryneEngine::Samples
{
    DrawInstanceManager::DrawInstanceManager(
        const AllocatorInstance _allocator,
        GraphicsContext& _graphicsContext,
        const u32 _maxInstances)
            : m_allocator(_allocator)
            , m_models(_allocator)
            , m_validModels(_allocator)
            , m_instances(_allocator, _maxInstances)
            , m_instanceData(_allocator)
            , m_instanceDataBuffer(_allocator)
    {
        m_instanceData.reserve(_maxInstances);

        // Instance data buffers
        const size_t instanceBufferSize = m_instanceData.capacity() * sizeof(InstanceData);
        {
            const BufferCreateDesc desc {
                .m_desc = {
                    .m_size = instanceBufferSize,
#if !defined(KE_FINAL)
                    .m_debugName { "Instance data buffer", m_allocator },
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::ReadBuffer | MemoryUsage::TransferDstBuffer,
            };
            m_instanceDataBuffer.Init(&_graphicsContext, desc, _graphicsContext.GetFrameContextCount());
        }

        // Instance data buffer views
        {
            m_instanceDataBufferViews = m_allocator.Allocate<BufferViewHandle>(_graphicsContext.GetFrameContextCount());

            for (size_t i = 0; i < _graphicsContext.GetFrameContextCount(); ++i)
            {
                char name[128];
                snprintf(name, sizeof(name), "Instance data buffer view #%zu", i);

                m_instanceDataBufferViews[i] = _graphicsContext.CreateBufferView({
                    .m_buffer = m_instanceDataBuffer.GetBuffer(i),
                    .m_size = instanceBufferSize,
                    .m_offset = 0,
                    .m_stride = sizeof(InstanceData),
                    .m_accessType = BufferViewAccessType::Read,
#if !defined(KE_FINAL)
                    .m_debugName = name,
#endif
                });
            }
        }
    }

    DrawInstanceManager::~DrawInstanceManager() = default;

    void DrawInstanceManager::UpdateGpuData(
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        void* dstBuffer = m_instanceDataBuffer.Map(&_graphicsContext, _graphicsContext.GetCurrentFrameContextIndex());

        for (size_t i = 0; i < m_instanceData.size(); ++i)
        {
            Instance& instance = m_instances.Get(i);
            if (!instance.m_valid)
                continue;

            if (instance.m_dynamic || instance.m_uploadFrames > 0)
            {
                if (instance.m_uploadFrames > 0)
                    instance.m_uploadFrames--;

                memcpy(static_cast<std::byte*>(dstBuffer) + i * sizeof(InstanceData), &m_instanceData[i], sizeof(InstanceData));
            }
        }

        m_instanceDataBuffer.Unmap(&_graphicsContext);
        m_instanceDataBuffer.PrepareBuffers(
            &_graphicsContext,
            _transferEncoder,
            BarrierAccessFlags::ShaderResource,
            _graphicsContext.GetCurrentFrameContextIndex());
    }

    DescriptorSetLayoutHandle DrawInstanceManager::GetPassDescriptorSetLayout(GraphicsContext& _graphicsContext)
    {
        if (m_passDescriptorSetLayout == GenPool::kInvalidHandle)
        {
            constexpr DescriptorBindingDesc bindings[2]
            {
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Vertex,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadOnlyBuffer,
                    .m_visibility = ShaderVisibility::Vertex
                }
            };

            m_passDescriptorSetLayout = _graphicsContext.CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                &m_passCbBindingIndex);
        }

        return m_passDescriptorSetLayout;
    }
} // namespace KryneEngine::Samples