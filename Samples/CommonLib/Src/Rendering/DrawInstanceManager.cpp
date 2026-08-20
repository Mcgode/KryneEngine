/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#include "Rendering/DrawInstanceManager.hpp"


namespace KryneEngine::Samples
{
    DrawInstanceManager::DrawInstanceManager(const AllocatorInstance _allocator, u32 _maxInstances)
        : m_allocator(_allocator)
        , m_models(_allocator)
        , m_validModels(_allocator)
        , m_instances(_allocator, _maxInstances)
        , m_instanceData(_allocator)
        , m_instanceDataBuffer(_allocator)
    {
        m_instanceData.reserve(_maxInstances);
    }

    void DrawInstanceManager::UpdateGpuData(GraphicsContext& _graphicsContext, CommandListHandle _commandList)
    {
        if (m_instanceDataBuffer.NeedsInit())
        {
            const BufferCreateDesc desc {
                .m_desc = {
                    .m_size = m_instanceData.capacity() * sizeof(InstanceData),
#if !defined(KE_FINAL)
                    .m_debugName { "Instance data buffer", m_allocator },
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::ReadBuffer | MemoryUsage::TransferDstBuffer,
            };
            m_instanceDataBuffer.Init(&_graphicsContext, desc, _graphicsContext.GetFrameContextCount());
        }

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
            _commandList,
            BarrierAccessFlags::ShaderResource,
            _graphicsContext.GetCurrentFrameContextIndex());
    }
} // namespace KryneEngine::Samples