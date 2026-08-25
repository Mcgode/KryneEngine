/**
 * @file
 * @author Max Godefroy
 * @date 29/10/2024.
 */

#pragma once

#include <atomic>
#include <EASTL/vector.h>

#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "Graphics/Metal/MetalTypes.hpp"

namespace KryneEngine
{
    class MetalFrameContext
    {
        friend class MetalGraphicsContext;

    public:
        MetalFrameContext(
            MTL::Device* _device,
            AllocatorInstance _allocator,
            u32 _timestampCount,
            MTL4::CommandQueue* _graphicsQueue,
            MTL4::CommandQueue* _computeQueue,
            MTL4::CommandQueue* _ioQueue,
            bool _validationLayers);

        CommandList BeginGraphicsCommandList(MTL::Device* _device);

        void PrepareForNextFrame(u64 _frameId);

        void WaitForFrame(u64 _frameId);

        void ResolveCounters(const TimestampConversion& _conversion);

        u32 AllocateTimestamp();

    private:
        struct AllocationSet
        {
            eastl::vector<CommandListData*> m_usedCommandBuffers;
            eastl::vector<MTL4::CommandAllocator*> m_commandAllocators;
            eastl::vector<MTL4::CommandBuffer*> m_commandBuffers;
            MTL::SharedEvent* m_synchronizationEvent = nullptr;
            MTL4::CommandQueue* m_queue;
            u16 m_currentCommandAllocatorIndex = 0;

            AllocationSet(AllocatorInstance _allocator, MTL::Device* _device, MTL4::CommandQueue* _queue);
            void Commit(u64 _frameId, bool _enhancedErrors);
            void Wait(u64 _frameId) const;

            CommandListData* UseNextCommandList(MTL::Device* _device);
        };

        AllocationSet m_graphicsAllocationSet;
        AllocationSet m_computeAllocationSet;
        AllocationSet m_ioAllocationSet;
        u64 m_frameId;
        bool m_enhancedCommandBufferErrors;

        NsPtr<MTL4::CounterHeap> m_sampleCounterHeap;
        DynamicArray<u64> m_resolvedTimestamps;
        std::atomic<u32> m_timestampIndex;
        u64 m_lastResolvedFrame = ~0ull;
    };
} // namespace KryneEngine
