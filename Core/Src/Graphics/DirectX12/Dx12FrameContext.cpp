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
    Dx12FrameContext::Dx12FrameContext(ID3D12Device *_device, const AllocatorInstance _allocator)
        : m_directCommandAllocationSet(_allocator)
        , m_computeCommandAllocationSet(_allocator)
        , m_copyCommandAllocationSet(_allocator)
        , m_resolvedTimestampBuffer(nullptr)
        , m_timestamps(_allocator)
    {
        m_device = _device;
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

    u32 Dx12FrameContext::PutTimestamp(const CommandListSet* _commandListSet, ID3D12QueryHeap* _heap)
    {
        const u32 localIndex = m_timestampIndex.fetch_add(1, std::memory_order_acquire);
        KE_ASSERT_MSG(localIndex < m_timestamps.capacity(), "GPU timestamp buffer capacity exceeded");
        _commandListSet->m_commandList->EndQuery(_heap, D3D12_QUERY_TYPE_TIMESTAMP, localIndex + m_timestampOffset);
        // Returned index is relative to this frame context, to index into m_timestamps.
        return localIndex;
    }

    void Dx12FrameContext::RecordTimestampsResolve(ID3D12QueryHeap* _heap)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::RecordTimestampsResolve");

        VERIFY_OR_RETURN_VOID(m_timestampBufferAllocation != nullptr);

        m_pendingTimestampCount = m_timestampIndex.exchange(0u, std::memory_order_acq_rel);

        if (m_pendingTimestampCount == 0)
        {
            return;
        }

        const CommandListSet* commandListSet = m_directCommandAllocationSet.BeginCommandList(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        commandListSet->m_commandList->ResolveQueryData(
            _heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            m_timestampOffset,
            m_pendingTimestampCount,
            m_resolvedTimestampBuffer,
            0);
        m_directCommandAllocationSet.EndCommandList(commandListSet);
    }

    void Dx12FrameContext::ReadbackTimestamps(const double _timestampPeriod, const u64 _timestampSyncOffset)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::ReadbackTimestamps");

        VERIFY_OR_RETURN_VOID(m_timestampBufferAllocation != nullptr);

        const u32 count = m_pendingTimestampCount;
        m_pendingTimestampCount = 0;

        if (count == 0)
        {
            return;
        }

        const D3D12_RANGE readRange { 0, sizeof(u64) * count };
        u64* buffer;
        Dx12Assert(m_resolvedTimestampBuffer->Map(0, &readRange, reinterpret_cast<void**>(&buffer)));

        m_timestamps.resize(count);
        for (u32 i = 0; i < count; i++)
        {
            m_timestamps[i] = static_cast<u64>(static_cast<double>(buffer[i]) * _timestampPeriod) + _timestampSyncOffset;
        }

        constexpr D3D12_RANGE writtenRange { 0, 0 };
        m_resolvedTimestampBuffer->Unmap(0, &writtenRange);
    }

    Dx12FrameContext::CommandAllocationSet::CommandAllocationSet(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
          , m_availableCommandLists(_allocator)
          , m_usedCommandLists(_allocator)
    {}

    CommandListSet* Dx12FrameContext::CommandAllocationSet::BeginCommandList(
        ID3D12Device *_device,
        const D3D12_COMMAND_LIST_TYPE _commandType)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::BeginCommandList");

        const auto lock = m_mutex.AutoLock();

        if (m_availableCommandLists.empty())
        {
            KE_ZoneScoped("Allocate new command list");

            CommandListSet& newSet = *m_usedCommandLists.emplace_back(m_allocator.Allocate<CommandListSet>());

            Dx12Assert(_device->CreateCommandAllocator(
                _commandType,
                IID_PPV_ARGS(&newSet.m_commandAllocator)));

            Dx12Assert(_device->CreateCommandList(
                0,
                _commandType,
                newSet.m_commandAllocator,
                nullptr,
                IID_PPV_ARGS(&newSet.m_commandList)));

#if !defined(KE_FINAL)
            const wchar_t* queueName;
            switch (_commandType)
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
            const CommandListSet* set = m_usedCommandLists.back();
            Dx12Assert(set->m_commandList->Reset(set->m_commandAllocator, nullptr));
        }

        return m_usedCommandLists.back();
    }

    void Dx12FrameContext::CommandAllocationSet::EndCommandList(const CommandListSet* _commandList)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::EndCommandList");

        const auto lock = m_mutex.AutoLock();

        for (const auto& set: m_usedCommandLists)
        {
            if (set == _commandList)
            {
                Dx12Assert(_commandList->m_commandList->Close());
                return;
            }
        }
        KE_ERROR("Command list not found in used command lists");
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
            for (auto* commandListSet: _vector)
            {
                SafeRelease(commandListSet->m_commandList);
                SafeRelease(commandListSet->m_commandAllocator);
            }
            _vector.clear();
        };
        freeCommandVector(m_usedCommandLists);
        freeCommandVector(m_availableCommandLists);
    }
} // KryneEngine