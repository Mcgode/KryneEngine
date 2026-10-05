/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#include "Graphics/DirectX12/Dx12FrameContext.hpp"

#include <D3D12MemAlloc.h>

#include "Graphics/DirectX12/HelperFunctions.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"

namespace KryneEngine
{
    Dx12FrameContext::Dx12FrameContext(ID3D12Device *_device, bool _directAllocator, bool _computeAllocator, bool _copyAllocator)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::Dx12FrameContext");

        m_device = _device;

        m_directCommandAllocationSet.m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_computeCommandAllocationSet.m_type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        m_copyCommandAllocationSet.m_type = D3D12_COMMAND_LIST_TYPE_COPY;
    }

    Dx12FrameContext::~Dx12FrameContext()
    {
        if (m_timestampBufferAllocation != nullptr)
        {
            m_timestampBufferAllocation->Release();
        }

        m_directCommandAllocationSet.Destroy();
        m_computeCommandAllocationSet.Destroy();
        m_copyCommandAllocationSet.Destroy();
    }

    u32 Dx12FrameContext::PutTimestamp(CommandList _commandList, ID3D12QueryHeap* _heap)
    {
        const u32 index = m_timestampIndex.fetch_add(1, std::memory_order_acquire) + m_timestampOffset;
        _commandList->EndQuery(_heap, D3D12_QUERY_TYPE_TIMESTAMP, index);
        return index;
    }

    void Dx12FrameContext::ResolveTimestamps(
        ID3D12QueryHeap* _heap,
        const double _timestampPeriod,
        const u64 _timestampSyncOffset)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::ResolveTimestamps");

        VERIFY_OR_RETURN_VOID(m_timestampBufferAllocation != nullptr);

        const u32 count = m_timestampIndex.load(std::memory_order_acquire);

        if (count == 0)
        {
            m_timestamps.clear();
            return;
        }

        CommandList commandList = m_directCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        commandList->ResolveQueryData(
            _heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            m_timestampOffset,
            count,
            m_resolvedTimestampBuffer,
            0);
        m_directCommandAllocationSet.EndCommandList(commandList);

        const D3D12_RANGE readRange { 0, sizeof(u64) * count };
        u64* buffer;
        Dx12Assert(m_resolvedTimestampBuffer->Map(0, &readRange, reinterpret_cast<void**>(&buffer)));

        m_timestamps.resize(count);
        for (u32 i = 0; i < count; i++)
        {
            m_timestamps[i] = static_cast<u64>(static_cast<double>(buffer[i]) * _timestampPeriod) + _timestampSyncOffset;
        }

        m_resolvedTimestampBuffer->Unmap(0, nullptr);

        m_timestampIndex.store(0u, std::memory_order::release);
    }

    ID3D12GraphicsCommandList7* Dx12FrameContext::CommandAllocationSet::BeginCommandList(
        ID3D12Device *_device,
        const D3D12_COMMAND_LIST_TYPE _commandType)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::BeginCommandList");

        const auto lock = m_mutex.AutoLock();

        if (m_availableCommandLists.empty())
        {
            KE_ZoneScoped("Allocate new command list");

            CommandListAndAllocator& newSet = m_usedCommandLists.emplace_back();

            Dx12Assert(_device->CreateCommandAllocator(
                m_type,
                IID_PPV_ARGS(&newSet.m_commandAllocator)));

            Dx12Assert(_device->CreateCommandList(
                0,
                _commandType,
                newSet.m_commandAllocator,
                nullptr,
                IID_PPV_ARGS(&newSet.m_commandList)));

#if !defined(KE_FINAL)
            const wchar_t* queueName;
            switch (m_type)
            {
                case D3D12_COMMAND_LIST_TYPE_DIRECT:
                    queueName = L"Direct";
                    break;
                case D3D12_COMMAND_LIST_TYPE_COMPUTE:
                    queueName = L"Compute";
                    break;
                case D3D12_COMMAND_LIST_TYPE_COPY:
                    queueName = L"Copy";
                    break;
                default:
                    queueName = L"";
                    break;
            }
            Dx12SetName(
                newSet.m_commandAllocator,
                L"%s Command Allocator %lld",
                queueName,
                m_usedCommandLists.size());
            Dx12SetName(
                newSet.m_commandList,
                L"%s Command List %lld",
                queueName,
                m_usedCommandLists.size());
#endif
        }
        else
        {
            m_usedCommandLists.push_back(m_availableCommandLists.back());
            m_availableCommandLists.pop_back();
            const CommandListAndAllocator& set = m_usedCommandLists.back();
            Dx12Assert(set.m_commandList->Reset(set.m_commandAllocator, nullptr));
        }

        return m_usedCommandLists.back().m_commandList;
    }

    void Dx12FrameContext::CommandAllocationSet::EndCommandList(CommandList _commandList)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::EndCommandList");

        const auto lock = m_mutex.AutoLock();

        for (const auto& set: m_usedCommandLists)
        {
            if (set.m_commandList == _commandList)
            {
                Dx12Assert(_commandList->Close());
                return;
            }
        }
        KE_ERROR("Command list not found in used command lists");
    }

    void Dx12FrameContext::CommandAllocationSet::Destroy()
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::Destroy");

        if (!m_usedCommandLists.empty())
        {
            Reset();
        }

        const auto lock = m_mutex.AutoLock();
        KE_ASSERT_MSG(m_usedCommandLists.empty(), "Allocation set should have been reset");

        const auto freeCommandVector = [](auto& _vector)
        {
            for (auto& commandListAndAllocator: _vector)
            {
                SafeRelease(commandListAndAllocator.m_commandList);
                SafeRelease(commandListAndAllocator.m_commandAllocator);
            }
            _vector.clear();
        };
        freeCommandVector(m_usedCommandLists);
        freeCommandVector(m_availableCommandLists);
    }

    void Dx12FrameContext::CommandAllocationSet::Reset()
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::Reset");

        const auto lock = m_mutex.AutoLock();

        // Use swap to keep order of command lists and allocators

        eastl::swap(m_availableCommandLists, m_usedCommandLists);
        m_availableCommandLists.insert(
            m_availableCommandLists.end(),
            m_usedCommandLists.begin(),
            m_usedCommandLists.end());
        m_usedCommandLists.clear();
    }
} // KryneEngine