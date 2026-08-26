/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2024.
 */

#include "Graphics/Metal/MetalFrameContext.hpp"

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    MetalFrameContext::MetalFrameContext(
        MTL::Device* _device,
        const AllocatorInstance _allocator,
        const u32 _timestampCount,
        MTL4::CommandQueue* _graphicsQueue,
        MTL4::CommandQueue* _computeQueue,
        MTL4::CommandQueue* _ioQueue,
        const bool _validationLayers)
            : m_graphicsAllocationSet(_allocator, _device, _graphicsQueue)
            , m_computeAllocationSet(_allocator, _device, _computeQueue)
            , m_ioAllocationSet(_allocator, _device, _ioQueue)
            , m_enhancedCommandBufferErrors(_validationLayers)
    {
        if (_timestampCount > 0)
        {
            {
                KE_AUTO_RELEASE_POOL;
                MTL4::CounterHeapDescriptor* descriptor = MTL4::CounterHeapDescriptor::alloc()->init();
                descriptor->setType(MTL4::CounterHeapTypeTimestamp);
                descriptor->setCount(_timestampCount);

                NS::Error* error = nullptr;
                m_sampleCounterHeap = _device->newCounterHeap(descriptor, &error);
                KE_ASSERT_FATAL_MSG(m_sampleCounterHeap != nullptr, error->localizedDescription()->cString(NS::UTF8StringEncoding));
#if !defined(KE_FINAL)
                m_sampleCounterHeap->setLabel(NS::String::string("TimestampBuffer", NS::UTF8StringEncoding));
#endif
            }

            m_resolvedTimestamps.SetAllocator(_allocator);
            m_resolvedTimestamps.Resize(_timestampCount);
        }
    }

    CommandList MetalFrameContext::BeginGraphicsCommandList(MTL::Device* _device)
    {
        KE_ASSERT(m_graphicsAllocationSet.m_queue != nullptr);

        return m_graphicsAllocationSet.UseNextCommandList(_device);
    }

    void MetalFrameContext::PrepareForNextFrame(const u64 _frameId)
    {
        m_frameId = _frameId;
    }

    void MetalFrameContext::WaitForFrame(const u64 _frameId)
    {
        KE_ZoneScopedFunction("MetalFrameContext::WaitForFrame");
        if (m_frameId > _frameId)
        {
            return;
        }
        m_graphicsAllocationSet.Wait(m_frameId);
        m_computeAllocationSet.Wait(m_frameId);
        m_ioAllocationSet.Wait(m_frameId);
    }

    void MetalFrameContext::ResolveCounters(const TimestampConversion& _conversion)
    {
        if (m_sampleCounterHeap == nullptr)
            return;

        if (m_lastResolvedFrame == m_frameId)
            return;
        m_lastResolvedFrame = m_frameId;

        const u32 sampleCount = m_timestampIndex.load(std::memory_order_acquire);
        if (sampleCount == 0)
            return;

        m_timestampIndex.store(0, std::memory_order_release);

        KE_AUTO_RELEASE_POOL;
        const NS::Data* resolvedNsData = m_sampleCounterHeap->resolveCounterRange(NS::Range(0, sampleCount));
        KE_ASSERT(resolvedNsData != nullptr);
        KE_ASSERT(resolvedNsData->length() == sampleCount * sizeof(MTL::CounterResultTimestamp));

        const auto* resolvedTimestamps = static_cast<const MTL::CounterResultTimestamp*>(resolvedNsData->bytes());
        for (u32 i = 0; i < sampleCount; ++i)
        {
            m_resolvedTimestamps[i] = _conversion.ConvertGpuTimestamp(resolvedTimestamps[i].timestamp);
        }
    }

    u32 MetalFrameContext::AllocateTimestamp()
    {
        return m_timestampIndex.fetch_add(1, std::memory_order_acquire);
    }

    MetalFrameContext::AllocationSet::AllocationSet(
        const AllocatorInstance _allocator,
        MTL::Device* _device,
        MTL4::CommandQueue* _queue)
            : m_usedCommandBuffers(_allocator)
            , m_commandAllocators(_allocator)
            , m_commandBuffers(_allocator)
            , m_queue(_queue)
    {
        if (_queue)
        {
            m_synchronizationEvent = _device->newSharedEvent();
        }
    }

    void MetalFrameContext::AllocationSet::Commit(const u64 _frameId, const bool _enhancedErrors)
    {
        if (m_queue == nullptr)
        {
            return;
        }

        MTL4::CommitOptions* options = nullptr;
        if (_enhancedErrors)
        {
            options = MTL4::CommitOptions::alloc()->init();
            options->addFeedbackHandler(^(MTL4::CommitFeedback* _feedback) {
                if (_feedback->error() != nullptr)
                {
                    KE_ERROR("Metal error: %s", _feedback->error()->localizedDescription()->cString(NS::UTF8StringEncoding));
                }
            });
        }

        if (m_currentCommandAllocatorIndex > 0)
        {
            m_queue->commit(m_commandBuffers.data(), m_currentCommandAllocatorIndex, options);
            m_queue->signalEvent(m_synchronizationEvent, _frameId);
        }
        else
        {
            m_synchronizationEvent->setSignaledValue(_frameId);
        }

        for (CommandListData* commandList: m_usedCommandBuffers)
        {
            m_usedCommandBuffers.get_allocator().Delete(commandList);
        }

        m_usedCommandBuffers.clear();
        m_currentCommandAllocatorIndex = 0;
    }

    void MetalFrameContext::AllocationSet::Wait(const u64 _frameId) const
    {
        if (m_queue == nullptr)
        {
            return;
        }

        m_synchronizationEvent->waitUntilSignaledValue(_frameId, DISPATCH_TIME_FOREVER);
    }

    CommandListData* MetalFrameContext::AllocationSet::UseNextCommandList(MTL::Device* _device)
    {
        KE_AUTO_RELEASE_POOL;

        const u16 index = m_currentCommandAllocatorIndex++;
        if (index >= m_commandAllocators.size())
        {
            m_commandAllocators.push_back(_device->newCommandAllocator());
            m_commandBuffers.push_back(_device->newCommandBuffer());
        }

        MTL4::CommandBuffer* commandBuffer = m_commandBuffers[index];
        commandBuffer->beginCommandBuffer(m_commandAllocators[index]);

        m_usedCommandBuffers.push_back(m_usedCommandBuffers.get_allocator().New<CommandListData>(commandBuffer));

        return m_usedCommandBuffers[index];
    }
} // namespace KryneEngine