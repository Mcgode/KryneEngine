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

        const auto initAllocator = [this] (bool _alloc,CommandAllocationSet& _set, D3D12_COMMAND_LIST_TYPE _type, const wchar_t* _name)
        {
            if (_alloc)
            {
                Dx12Assert(m_device->CreateCommandAllocator(_type, IID_PPV_ARGS(&_set.m_commandAllocator)));

#if !defined(KE_FINAL)
                Dx12SetName(_set.m_commandAllocator.Get(), L"%s Command Allocator", _name);
#endif
            }
        };

        initAllocator(_directAllocator, m_directCommandAllocationSet, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct");
        initAllocator(_computeAllocator, m_computeCommandAllocationSet, D3D12_COMMAND_LIST_TYPE_COMPUTE, L"Compute");
        initAllocator(_copyAllocator, m_copyCommandAllocationSet, D3D12_COMMAND_LIST_TYPE_COPY, L"Copy");
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
        CommandList _commandList,
        ID3D12QueryHeap* _heap,
        const double _timestampPeriod,
        const u64 _timestampSyncOffset)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::ResolveTimestamps");

        VERIFY_OR_RETURN_VOID(m_timestampBufferAllocation != nullptr);

        const u32 count = m_timestampIndex.load(std::memory_order_acquire);
        _commandList->ResolveQueryData(
            _heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            m_timestampOffset,
            count,
            m_resolvedTimestampBuffer,
            0);

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

        VERIFY_OR_RETURN(m_commandAllocator != nullptr, nullptr);

        const auto lock = m_mutex.AutoLock();

        if (m_availableCommandLists.empty())
        {
            KE_ZoneScoped("Allocate new command list");

            Dx12Assert(_device->CreateCommandList(0,
                                                  _commandType,
                                                  m_commandAllocator.Get(),
                                                  nullptr,
                                                  IID_PPV_ARGS(&m_usedCommandLists.push_back())));
        }
        else
        {
            m_usedCommandLists.push_back(m_availableCommandLists.back());
            m_availableCommandLists.pop_back();
            Dx12Assert(m_usedCommandLists.back()->Reset(m_commandAllocator.Get(), nullptr));
        }

        return m_usedCommandLists.back();
    }

    void Dx12FrameContext::CommandAllocationSet::EndCommandList(CommandList _commandList)
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::EndCommandList");

        VERIFY_OR_RETURN_VOID(m_commandAllocator != nullptr);

        const auto lock = m_mutex.AutoLock();

        const auto it = eastl::find(m_usedCommandLists.begin(), m_usedCommandLists.end(), _commandList);
        if (KE_VERIFY(it != m_usedCommandLists.end()))
        {
            Dx12Assert(_commandList->Close());
        }
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

        const auto freeCommandListVector = [](auto& _vector)
        {
            for (auto commandList: _vector)
            {
                SafeRelease(commandList);
            }
            _vector.clear();
        };
        freeCommandListVector(m_usedCommandLists);
        freeCommandListVector(m_availableCommandLists);

        SafeRelease(m_commandAllocator);
    }

    void Dx12FrameContext::CommandAllocationSet::Reset()
    {
        KE_ZoneScopedFunction("Dx12FrameContext::CommandAllocationSet::Reset");

        const auto lock = m_mutex.AutoLock();

        m_availableCommandLists.insert(
            m_availableCommandLists.end(),
            m_usedCommandLists.begin(),
            m_usedCommandLists.end());
        m_usedCommandLists.clear();
    }
} // KryneEngine