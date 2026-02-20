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
        Dx12FrameContext(ID3D12Device* _device, bool _directAllocator, bool _computeAllocator, bool _copyAllocator);

        virtual ~Dx12FrameContext();

        ID3D12GraphicsCommandList7* BeginDirectCommandList()
        {
            return m_directCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        void EndDirectCommandList(CommandList _commandList)
        {
            m_directCommandAllocationSet.EndCommandList(_commandList);
        }

        ID3D12GraphicsCommandList7* BeginComputeCommandList()
        {
            return m_computeCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
        }

        void EndComputeCommandList(CommandList _commandList)
        {
            m_computeCommandAllocationSet.EndCommandList(_commandList);
        }

        ID3D12GraphicsCommandList7* BeginTransferCommandList(CommandList _commandList)
        {
            return m_copyCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
        }

        void EndTransferCommandList(CommandList _commandList)
        {
            m_copyCommandAllocationSet.EndCommandList(_commandList);
        }

        u32 PutTimestamp(CommandList _commandList, ID3D12QueryHeap* _heap);

        void ResolveTimestamps(
            ID3D12QueryHeap* _heap,
            double _timestampPeriod,
            u64 _timestampSyncOffset);

    private:
        ComPtr<ID3D12Device> m_device;

        struct CommandAllocationSet
        {
            ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;

            eastl::vector<ID3D12GraphicsCommandList7*> m_availableCommandLists;
            eastl::vector<ID3D12GraphicsCommandList7*> m_usedCommandLists;

            LightweightMutex m_mutex {};

            ID3D12GraphicsCommandList7 * BeginCommandList(ID3D12Device *_device, D3D12_COMMAND_LIST_TYPE _commandType);
            void EndCommandList(CommandList _commandList);

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