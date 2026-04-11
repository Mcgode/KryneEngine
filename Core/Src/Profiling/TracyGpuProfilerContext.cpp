/**
 * @file
 * @author Max Godefroy
 * @date 21/09/2025.
 */

#include "KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp"

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <tracy/Tracy.hpp>

namespace KryneEngine
{
    TracyGpuProfilerContext::TracyGpuProfilerContext(AllocatorInstance _allocator, u32 _frameContextCount)
        : m_allocator(_allocator)
        , m_queryRingBuffer(m_allocator.Allocate<u32>(kQueryRingBufferCapacity))
        , m_frameContextQueryRanges(m_allocator, _frameContextCount)
    {
        using namespace tracy;

        m_tracyContextId = GetGpuCtxCounter().fetch_add(1);

        m_frameContextQueryRanges.InitAll(0, 0);

        const s64 timestamp = Profiler::GetTime();

        QueueItem* item = Profiler::QueueSerial();
        MemWrite(&item->hdr.type, QueueType::GpuNewContext);
        MemWrite(&item->gpuNewContext.cpuTime, timestamp);
        MemWrite(&item->gpuNewContext.gpuTime, timestamp);
        MemWrite(&item->gpuNewContext.thread, uint32_t(0)); // TODO: why not GetThreadHandle()?
        MemWrite(&item->gpuNewContext.period, float(1));
        MemWrite(&item->gpuNewContext.context, m_tracyContextId);
        MemWrite(&item->gpuNewContext.flags, GpuContextFlags(0));

#if defined(KE_GRAPHICS_API_VK)
        MemWrite(&item->gpuNewContext.type, GpuContextType::Vulkan);
#elif defined(KE_GRAPHICS_API_DX12)
        MemWrite(&item->gpuNewContext.type, GpuContextType::Direct3D12);
#elif defined(KE_GRAPHICS_API_MTL)
        MemWrite(&item->gpuNewContext.type, GpuContextType::Invalid);
#else
#   error Unsupported API
#endif

#ifdef TRACY_ON_DEMAND
        GetProfiler().DeferItem(*item);
#endif
        Profiler::QueueSerialFinish();
    }

    TracyGpuProfilerContext::~TracyGpuProfilerContext()
    {
        m_allocator.deallocate(m_queryRingBuffer, sizeof(u32) * kQueryRingBufferCapacity);
    }

    u16 TracyGpuProfilerContext::ReserveQuery()
    {
        const auto lock = m_queryRingBufferLock.AutoLock();

        u32 queryId = m_queryRingBufferTail;
        m_queryRingBufferTail += 2;
        queryId &= 0xFFFF; // Go back to 16 bits;
        KE_ASSERT_MSG(m_queryRingBufferTail - m_queryRingBufferHead < kQueryRingBufferCapacity, "Query ring buffer overflow");

        return queryId;
    }

    void TracyGpuProfilerContext::SetQueryTimestampIndex(u16 _queryIndex, u32 _timestampIndex)
    {
        m_queryRingBuffer[_queryIndex] = _timestampIndex;
    }

    void TracyGpuProfilerContext::EndFrame(u64 _frameId)
    {
        const auto lock = m_queryRingBufferLock.AutoLock();

        const size_t frameContextCount = m_frameContextQueryRanges.Size();
        const u8 frameContextId = _frameId % frameContextCount;

        const u8 previousFrameContextId = (frameContextId + frameContextCount - 1) % frameContextCount;

        const u32 start = m_frameContextQueryRanges[previousFrameContextId].second % kQueryRingBufferCapacity;
        const u32 end = m_queryRingBufferTail % kQueryRingBufferCapacity;

        m_queryRingBufferHead = m_frameContextQueryRanges[frameContextId].first % kQueryRingBufferCapacity;
        m_queryRingBufferTail %= kQueryRingBufferCapacity;
        if (m_queryRingBufferTail < m_queryRingBufferHead)
        {
            m_queryRingBufferTail += kQueryRingBufferCapacity;
        }
        m_frameContextQueryRanges[frameContextId] = { start, end };
    }

    void TracyGpuProfilerContext::ResolveQueries(const GraphicsContext* _graphicsContext, u64 _frameId)
    {
        const auto [start, end] = m_frameContextQueryRanges[_frameId % m_frameContextQueryRanges.Size()];
        if (start == end)
            return;

        const eastl::span<const u64>& resolvedTimestamps = _graphicsContext->GetResolvedTimestamps(_frameId);
        KE_ASSERT(!resolvedTimestamps.empty());

        for (u32 i = start; i != end; i = (i + 1) % kQueryRingBufferCapacity)
        {
            const u32 timestampIdx = m_queryRingBuffer[i];
            const u64 timestamp = resolvedTimestamps[timestampIdx];

            using namespace tracy;

            QueueItem* item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuTime);
            MemWrite(&item->gpuTime.gpuTime, timestamp);
            MemWrite(&item->gpuTime.queryId, static_cast<u16>(i));
            MemWrite(&item->gpuTime.context, m_tracyContextId);
#ifdef TRACY_ON_DEMAND
            GetProfiler().DeferItem(*item);
#endif
            Profiler::QueueSerialFinish();
        }
    }
}