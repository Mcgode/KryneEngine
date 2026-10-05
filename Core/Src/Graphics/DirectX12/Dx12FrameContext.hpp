/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#pragma once

#include "Dx12Types.hpp"
#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

namespace D3D12MA
{
    class Allocation;
}

namespace KryneEngine
{
    class Dx12FrameContext
    {
        friend class Dx12GraphicsContext;

    public:
        Dx12FrameContext(ID3D12Device* _device, AllocatorInstance _allocator);

        virtual ~Dx12FrameContext();

        CommandListSet* BeginDirectCommandList()
        {
            return m_directCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        void EndDirectCommandList(const CommandListSet* _commandList)
        {
            m_directCommandAllocationSet.EndCommandList(_commandList);
        }

        CommandListSet* BeginComputeCommandList()
        {
            return m_computeCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
        }

        void EndComputeCommandList(const CommandListSet* _commandList)
        {
            m_computeCommandAllocationSet.EndCommandList(_commandList);
        }

        CommandListSet* BeginTransferCommandList()
        {
            return m_copyCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
        }

        void EndTransferCommandList(const CommandListSet* _commandListSet)
        {
            m_copyCommandAllocationSet.EndCommandList(_commandListSet);
        }

        u32 PutTimestamp(const CommandListSet* _commandListSet, ID3D12QueryHeap* _heap);

        void ResolveTimestamps(
            ID3D12QueryHeap* _heap,
            double _timestampPeriod,
            u64 _timestampSyncOffset);

    private:
        ComPtr<ID3D12Device> m_device;

        struct CommandAllocationSet
        {
            explicit CommandAllocationSet(AllocatorInstance _allocator);

            AllocatorInstance m_allocator;
            eastl::vector<CommandListSet*> m_availableCommandLists;
            eastl::vector<CommandListSet*> m_usedCommandLists;

            LightweightMutex m_mutex {};

            CommandListSet* BeginCommandList(ID3D12Device *_device, D3D12_COMMAND_LIST_TYPE _commandType);
            void EndCommandList(const CommandListSet* _commandList);

            void Reset();

            void Destroy();
        };

        CommandAllocationSet m_directCommandAllocationSet;
        CommandAllocationSet m_computeCommandAllocationSet;
        CommandAllocationSet m_copyCommandAllocationSet;
        u64 m_frameId = 0;
        u32 m_timestampOffset = 0;
        std::atomic<u32> m_timestampIndex = 0;
        D3D12MA::Allocation* m_timestampBufferAllocation = nullptr;
        ID3D12Resource* m_resolvedTimestampBuffer;
        eastl::vector<u64> m_timestamps {};
    };
} // KryneEngine