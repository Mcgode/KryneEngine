/**
 * @file
 * @author Max Godefroy
 * @date 04/08/2024.
 */

#include "KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp"

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"

namespace KryneEngine::Modules::GraphicsUtils
{
    DynamicBuffer::DynamicBuffer(const AllocatorInstance _allocator)
        : m_mappableBuffers(_allocator)
        , m_sizes(_allocator)
        , m_gpuBuffersToFree(_allocator)
    {}

    void DynamicBuffer::Init(
        GraphicsContext* _graphicsContext,
        const BufferCreateDesc& _bufferDesc,
        const u8 _frameCount,
        const bool _makeGpuReadOptimal)
    {
        const MemoryUsage usageType = _bufferDesc.m_usage & MemoryUsage::USAGE_TYPE_MASK;
        KE_ASSERT_MSG(
            usageType == MemoryUsage::StageEveryFrame_UsageType || usageType == MemoryUsage::CpuReadWrite_UsageType,
            "Buffer usage type should be `StageEveryFrame_UsageType` or `CpuReadWrite_UsageType`");
        KE_ASSERT_MSG(
            !BitUtils::EnumHasAny(_bufferDesc.m_usage, MemoryUsage::WriteBuffer),
            "Dynamic buffers are written by the CPU only, `WriteBuffer` is not supported");

        m_mappableBuffers.Resize(_frameCount);

        if (_makeGpuReadOptimal && !_graphicsContext->IsGpuReadOptimal(usageType))
        {
            // The GPU would read CPU-visible memory across the bus: write to mappable buffers, and copy them into
            // a device-local buffer that the GPU reads from.

            m_gpuRecreateDesc = _bufferDesc;
            m_gpuRecreateDesc.m_usage = (_bufferDesc.m_usage & ~MemoryUsage::USAGE_TYPE_MASK)
                | MemoryUsage::GpuOnly_UsageType
                | MemoryUsage::TransferDstBuffer;
            m_gpuBuffer = _graphicsContext->CreateBuffer(m_gpuRecreateDesc);

            m_mappableRecreateDesc = {
                .m_desc = _bufferDesc.m_desc,
                .m_usage = usageType | MemoryUsage::TransferSrcBuffer,
            };
            for (u8 i = 0; i < _frameCount; i++)
            {
                m_mappableBuffers[i] = _graphicsContext->CreateBuffer(m_mappableRecreateDesc);
            }
        }
        else
        {
            // The GPU can read the mappable buffers directly

#if !defined(KE_FINAL)
            m_mappableRecreateDesc.m_desc.m_debugName.set_allocator(m_mappableBuffers.GetAllocator());
#endif
            m_mappableRecreateDesc = _bufferDesc;
            for (u8 i = 0; i < _frameCount; i++)
            {
                m_mappableBuffers[i] = _graphicsContext->CreateBuffer(m_mappableRecreateDesc);
            }
        }

        m_sizes.Resize(_frameCount);
        m_sizes.InitAll(m_mappableRecreateDesc.m_desc.m_size);
    }

    void DynamicBuffer::RequestResize(u64 _size)
    {
        m_mappableRecreateDesc.m_desc.m_size = _size;
        if (m_gpuBuffer != GenPool::kInvalidHandle)
        {
            m_gpuRecreateDesc.m_desc.m_size = _size;
        }
    }

    void* DynamicBuffer::Map(GraphicsContext* _graphicsContext, u8 _frameIndex)
    {
        if (!m_gpuBuffersToFree.empty() && m_gpuBuffersToFree.front().m_atIndex == _frameIndex)
        {
            _graphicsContext->DestroyBuffer(m_gpuBuffersToFree.front().m_buffer);
            m_gpuBuffersToFree.erase(m_gpuBuffersToFree.begin());
        }

        if (m_mappableRecreateDesc.m_desc.m_size != m_sizes[_frameIndex])
        {
            _graphicsContext->DestroyBuffer(m_mappableBuffers[_frameIndex]);
            m_mappableBuffers[_frameIndex] = _graphicsContext->CreateBuffer(m_mappableRecreateDesc);

            if (m_gpuBuffer != GenPool::kInvalidHandle)
            {
                const u8 frameCount = m_mappableBuffers.Size();
                m_gpuBuffersToFree.push_back(BufferToFree {
                    m_gpuBuffer,
                    static_cast<u8>((_frameIndex + frameCount - 1u) % frameCount),
                });
                m_gpuBuffer = _graphicsContext->CreateBuffer(m_gpuRecreateDesc);
            }

            m_sizes[_frameIndex] = m_mappableRecreateDesc.m_desc.m_size;
        }

        m_currentMapping.m_buffer = m_mappableBuffers[_frameIndex];
        m_currentMapping.m_size = m_sizes[_frameIndex];
        _graphicsContext->MapBuffer(m_currentMapping);
        return m_currentMapping.m_ptr;
    }

    void DynamicBuffer::Unmap(GraphicsContext* _graphicsContext) {
        _graphicsContext->UnmapBuffer(m_currentMapping);
    }

    void DynamicBuffer::PrepareBuffers(
        GraphicsContext* _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const BarrierAccessFlags _accessFlags,
        const u8 _frameIndex) const
    {
        if (m_gpuBuffer == GenPool::kInvalidHandle)
        {
            const BufferMemoryBarrier memoryBarrier[1] {
                {
                    .m_stagesSrc = BarrierSyncStageFlags::All,
                    .m_stagesDst = BarrierSyncStageFlags::All,
                    .m_accessSrc = BarrierAccessFlags::All,
                    .m_accessDst = _accessFlags,
                    .m_buffer = m_mappableBuffers[_frameIndex],
                }
            };

            _graphicsContext->PlaceMemoryBarriers(_transferEncoder, {
                .m_placementType = BarrierPlacementType::Producer,
                .m_bufferBarriers = memoryBarrier
            });
        }
        else
        {
            const BufferCopyParameters params {
                .m_copySize = m_sizes[_frameIndex],
                .m_bufferSrc = m_mappableBuffers[_frameIndex],
                .m_bufferDst = m_gpuBuffer,
            };

            {
                const BufferMemoryBarrier memoryBarriers[2] = {
                    {
                        .m_stagesSrc = BarrierSyncStageFlags::None,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::All,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = params.m_bufferSrc,
                    },
                    {
                        .m_stagesSrc = BarrierSyncStageFlags::None,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::All,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = params.m_bufferDst,
                    }
                };

                _graphicsContext->PlaceMemoryBarriers(_transferEncoder, {
                    .m_placementType = BarrierPlacementType::IntraEncoder,
                    .m_bufferBarriers = memoryBarriers,
                });
            }

            _graphicsContext->CopyBuffer(_transferEncoder, params);

            {
                const BufferMemoryBarrier memoryBarrier[1] {
                    {
                        .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                       .m_stagesDst = BarrierSyncStageFlags::All,
                       .m_accessSrc = BarrierAccessFlags::TransferDst,
                       .m_accessDst = _accessFlags,
                       .m_buffer = params.m_bufferDst,
                    }
                };

                _graphicsContext->PlaceMemoryBarriers(_transferEncoder, {
                    .m_placementType = BarrierPlacementType::Producer,
                    .m_bufferBarriers = memoryBarrier,
                });
            }
        }
    }

    BufferHandle DynamicBuffer::GetBuffer(u8 _frameIndex)
    {
        if (m_gpuBuffer != GenPool::kInvalidHandle)
        {
            return m_gpuBuffer;
        }
        else
        {
            return m_mappableBuffers[_frameIndex];
        }
    }

    void DynamicBuffer::Destroy(GraphicsContext* _graphicsContext)
    {
        for (auto buffer: m_mappableBuffers)
        {
            _graphicsContext->DestroyBuffer(buffer);
        }
        if (m_gpuBuffer != GenPool::kInvalidHandle)
        {
            _graphicsContext->DestroyBuffer(m_gpuBuffer);
        }
    }
}