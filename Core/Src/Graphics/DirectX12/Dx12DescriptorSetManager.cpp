/**
 * @file
 * @author Max Godefroy
 * @date 11/08/2024.
 */

#include "Graphics/DirectX12/Dx12DescriptorSetManager.hpp"
#include "Graphics/DirectX12/Dx12Resources.h"
#include "Graphics/DirectX12/HelperFunctions.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"

namespace KryneEngine
{
    union PackedIndex
    {
        struct {
            u32 m_type: Dx12DescriptorSetManager::kDescriptorTypeBits;
            u32 m_binding: 32 - Dx12DescriptorSetManager::kDescriptorTypeBits;
        };
        u32 m_packed;
    };

    Dx12DescriptorSetManager::Dx12DescriptorSetManager(AllocatorInstance _allocator)
        : m_cbvSrvUavGpuDescriptorHeaps(_allocator)
        , m_samplerGpuDescriptorHeaps(_allocator)
        , m_descriptorSetLayout(_allocator)
        , m_descriptorSets(_allocator)
    {}

    void Dx12DescriptorSetManager::Init(ID3D12Device* _device, u8 _frameContextCount, u8 _currentFrame)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::Init");

        m_cbvSrvUavGpuDescriptorHeaps.Resize(_frameContextCount);
        m_cbvSrvUavGpuDescriptorHeaps.InitAll(nullptr);
        m_samplerGpuDescriptorHeaps.Resize(_frameContextCount);
        m_samplerGpuDescriptorHeaps.InitAll(nullptr);

        for (auto i = 0u; i < _frameContextCount; i++)
        {
            {
                const D3D12_DESCRIPTOR_HEAP_DESC heapDesc {
                    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    .NumDescriptors = kCbvSrvUavHeapSize,
                    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                };
                Dx12Assert(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvUavGpuDescriptorHeaps[i])));
#if !defined(KE_FINAL)
                Dx12SetName(m_cbvSrvUavGpuDescriptorHeaps[i].Get(), L"CBV/SRV/UAV descriptor GPU heap [%d]", i);
#endif
                m_cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }

            {
                const D3D12_DESCRIPTOR_HEAP_DESC heapDesc {
                    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                    .NumDescriptors = kSamplerHeapSize,
                    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                };
                Dx12Assert(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_samplerGpuDescriptorHeaps[i])));
#if !defined(KE_FINAL)
                Dx12SetName(m_samplerGpuDescriptorHeaps[i].Get(), L"Sampler descriptor GPU heap [%d]", i);
#endif
                m_samplerDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }
        }

        m_multiFrameUpdateTracker.Init(m_cbvSrvUavGpuDescriptorHeaps.GetAllocator(), _frameContextCount, _currentFrame);
    }

    DescriptorSetLayoutHandle Dx12DescriptorSetManager::CreateDescriptorSetLayout(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::CreateDescriptorSetLayout");

        VERIFY_OR_RETURN(_bindingIndices != nullptr, { GenPool::kInvalidHandle });

        eastl::array<ShaderVisibility, kRangeTypesCount> visibilities {
            ShaderVisibility::None,
            ShaderVisibility::None,
            ShaderVisibility::None,
            ShaderVisibility::None, };
        eastl::array<u32, kRangeTypesCount> totals = { 0, 0, 0, 0 };

        for (auto i = 0; i < _desc.m_bindings.size(); i++)
        {
            DescriptorBindingDesc binding = _desc.m_bindings[i];

            RangeType rangeType;
            DescriptorType descriptorType;
            switch (binding.m_type)
            {
            case DescriptorBindingDesc::Type::ConstantBuffer:
                rangeType = RangeType::Cbv;
                descriptorType = DescriptorType::BufferCbv;
                break;
            case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
                rangeType = RangeType::Srv;
                descriptorType = DescriptorType::BufferSrv;
                break;
            case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
            case DescriptorBindingDesc::Type::SampledTexture:
                rangeType = RangeType::Srv;
                descriptorType = DescriptorType::TextureSrv;
                break;
            case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
                rangeType = RangeType::Uav;
                descriptorType = DescriptorType::BufferUav;
                break;
            case DescriptorBindingDesc::Type::StorageReadWriteTexture:
                rangeType = RangeType::Uav;
                descriptorType = DescriptorType::TextureUav;
                break;
            case DescriptorBindingDesc::Type::Sampler:
                rangeType = RangeType::Sampler;
                descriptorType = DescriptorType::Sampler;
                break;
            }

            u32& total = totals[static_cast<u32>(rangeType)];

            // Pack index data into a single u32
            if (binding.m_bindingIndex != DescriptorBindingDesc::kImplicitBindingIndex)
            {
                _bindingIndices[i] = PackedIndex { .m_type = static_cast<u32>(descriptorType), .m_binding = total }.m_packed;
                total += binding.m_count;
            }
            else
            {
                KE_ASSERT(total <= binding.m_bindingIndex);
                _bindingIndices[i] = PackedIndex { .m_type = static_cast<u32>(descriptorType), .m_binding = binding.m_bindingIndex }.m_packed;
                total = binding.m_bindingIndex + binding.m_count;
            }

            const u8 rangeTypeIndex = static_cast<u8>(rangeType);
            visibilities[rangeTypeIndex] |= binding.m_visibility;
        }

        const GenPool::Handle handle = m_descriptorSetLayout.Allocate();
        *m_descriptorSetLayout.Get(handle) = { visibilities, totals };
        return { handle };
    }

    bool Dx12DescriptorSetManager::DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetLayout.Free(_layout.m_handle);
    }

    DescriptorSetHandle Dx12DescriptorSetManager::CreateDescriptorSet(DescriptorSetLayoutHandle _layout)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::CreateDescriptorSet");

        VERIFY_OR_RETURN(_layout != GenPool::kInvalidHandle, { GenPool::kInvalidHandle });
        LayoutData* pData = m_descriptorSetLayout.Get(_layout.m_handle);
        VERIFY_OR_RETURN(pData != nullptr, { GenPool::kInvalidHandle });
        eastl::array<u32, kRangeTypesCount>& totals = pData->m_totals;

        const GenPool::Handle handle = m_descriptorSets.Allocate();
        DescriptorSetRanges* ranges = m_descriptorSets.Get(handle);

        constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

        u32 total = 0;
        for (auto i = 0u; i < samplerIndex; i++)
        {
            if (totals[i] > 0)
            {
                ranges->m_sizes[i] = totals[i];
                total += totals[i];
            }
        }

        if (total > 0)
        {
            u32 offset = m_cbvSrvUavLinearAllocIndex.fetch_add(total);
            for (auto i = 0u; i < samplerIndex; i++)
            {
                ranges->m_offsets[i] = offset;
                offset += totals[i];
            }
        }

        if (totals[samplerIndex] > 0)
        {
            ranges->m_sizes[samplerIndex] = totals[samplerIndex];
            ranges->m_offsets[samplerIndex] = m_samplerLinearAllocIndex.fetch_add(totals[samplerIndex]);
        }

        return { handle };
    }

    bool Dx12DescriptorSetManager::DestroyDescriptorSet(DescriptorSetHandle _set)
    {
        return m_descriptorSets.Free(_set.m_handle);
    }

    void Dx12DescriptorSetManager::UpdateDescriptorSet(
        DescriptorSetHandle _descriptorSet,
        const Dx12Resources& _resources,
        const eastl::span<const DescriptorSetWriteInfo>& _writes,
        bool _singleFrame,
        ID3D12Device* _device,
        u8 _frameIndex)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::UpdateDescriptorSet");

        for (const auto& writeDesc: _writes)
        {
            const PackedIndex baseIndex { .m_packed = writeDesc.m_index };
            for (auto i = 0u; i < writeDesc.m_descriptorData.size(); i++)
            {
                TrackedData data {
                    .m_descriptorSet = _descriptorSet,
                    .m_object = writeDesc.m_descriptorData[i].m_handle,
                    .m_packedIndex = PackedIndex {
                        .m_type = baseIndex.m_type,
                        .m_binding = baseIndex.m_binding + writeDesc.m_arrayOffset + i
                    }.m_packed,
                };
                _ProcessUpdate(_device, _resources, data, _frameIndex);
                if (!_singleFrame)
                {
                    m_multiFrameUpdateTracker.TrackForOtherFrames(data);
                }
            }
        }
    }

    void Dx12DescriptorSetManager::SetGraphicsDescriptorSets(
        CommandList _commandList,
        const eastl::span<const DescriptorSetHandle>& _sets,
        u16* _tableSetOffsets,
        u32 _offset,
        u8 _currentFrame)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::SetGraphicsDescriptorSets");

        constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

        u32 tableIndex = _tableSetOffsets[_offset];
        for (auto setIndex = 0u; setIndex < _sets.size(); setIndex++)
        {
            DescriptorSetHandle set = _sets[setIndex];
            DescriptorSetRanges* pRanges = m_descriptorSets.Get(set.m_handle);
            VERIFY_OR_RETURN_VOID(pRanges != nullptr);

            u32 cbvSrvUavTotal = 0;
            for (auto i = 0u; i < samplerIndex; i++) { cbvSrvUavTotal += pRanges->m_sizes[i]; }

            if (cbvSrvUavTotal > 0)
            {
                CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    m_cbvSrvUavGpuDescriptorHeaps[_currentFrame]->GetGPUDescriptorHandleForHeapStart(),
                    pRanges->m_offsets[0],
                    m_cbvSrvUavDescriptorSize);
                _commandList->SetGraphicsRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }

            if (pRanges->m_sizes[samplerIndex] > 0)
            {
                CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    m_samplerGpuDescriptorHeaps[_currentFrame]->GetGPUDescriptorHandleForHeapStart(),
                    pRanges->m_offsets[samplerIndex],
                    m_samplerDescriptorSize);
                _commandList->SetGraphicsRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }
        }
    }

    void Dx12DescriptorSetManager::OnBeginGraphicsCommandList(CommandList _commandList, u8 _frameIndex)
    {
        ID3D12DescriptorHeap* heaps[2] = {
            m_cbvSrvUavGpuDescriptorHeaps[_frameIndex].Get(),
            m_samplerGpuDescriptorHeaps[_frameIndex].Get(),
        };
        _commandList->SetDescriptorHeaps(2, heaps);
    }

    void Dx12DescriptorSetManager::NextFrame(ID3D12Device* _device, const Dx12Resources& _resources, u8 _frameIndex)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::NextFrame");

        m_multiFrameUpdateTracker.AdvanceToNextFrame();

        for (const auto& data : m_multiFrameUpdateTracker.GetData())
        {
            _ProcessUpdate(_device, _resources, data, _frameIndex);
        }

        m_multiFrameUpdateTracker.ClearData();

        m_descriptorSets.FlushDeferredFrees();
        m_descriptorSetLayout.FlushDeferredFrees();
    }

    const Dx12DescriptorSetManager::LayoutData* Dx12DescriptorSetManager::GetDescriptorSetLayoutData(
        DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetLayout.Get(_layout.m_handle);
    }

    void Dx12DescriptorSetManager::_ProcessUpdate(
        ID3D12Device* _device,
        const Dx12Resources& _resources,
        const Dx12DescriptorSetManager::TrackedData& _data,
        u8 _currentFrame)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::_ProcessUpdate");

        const PackedIndex packedIndex { .m_packed = _data.m_packedIndex };
        const auto descriptorType = static_cast<DescriptorType>(packedIndex.m_type);
        const bool isSampler = descriptorType == DescriptorType::Sampler;

        auto* dstHeap = (isSampler ? m_samplerGpuDescriptorHeaps : m_cbvSrvUavGpuDescriptorHeaps)[_currentFrame].Get();

        CD3DX12_CPU_DESCRIPTOR_HANDLE srcCpuHandle {};
        RangeType rangeType;
        switch (descriptorType)
        {
        case DescriptorType::BufferCbv:
            srcCpuHandle = _resources.m_bufferViews.Get(_data.m_object)->m_cbvHandle;
            rangeType = RangeType::Cbv;
            break;
        case DescriptorType::BufferSrv:
            srcCpuHandle = _resources.m_bufferViews.Get(_data.m_object)->m_srvHandle;
            rangeType = RangeType::Srv;
            break;
        case DescriptorType::BufferUav:
            srcCpuHandle = _resources.m_bufferViews.Get(_data.m_object)->m_uavHandle;
            rangeType = RangeType::Uav;
            break;
        case DescriptorType::TextureSrv:
            srcCpuHandle = _resources.m_textureViews.Get(_data.m_object)->m_srvHandle;
            rangeType = RangeType::Srv;
            break;
        case DescriptorType::TextureUav:
            srcCpuHandle = _resources.m_textureViews.Get(_data.m_object)->m_uavHandle;
            rangeType = RangeType::Uav;
            break;
        case DescriptorType::Sampler:
            srcCpuHandle = *_resources.m_samplers.Get(_data.m_object);
            rangeType = RangeType::Sampler;
            break;
        default:
            KE_ERROR("Invalid descriptor type");
            break;
        }
        if (srcCpuHandle.ptr == 0)
        {
            return;
        }

        const u32 relativeIndex = packedIndex.m_binding;
        DescriptorSetRanges* pRanges = m_descriptorSets.Get(_data.m_descriptorSet.m_handle);
        if (pRanges == nullptr)
        {
            return;
        }

        const u32 index = relativeIndex + pRanges->m_offsets[static_cast<u32>(rangeType)];

        CD3DX12_CPU_DESCRIPTOR_HANDLE dstCpuHandle(
            dstHeap->GetCPUDescriptorHandleForHeapStart(),
            index,
            isSampler ? m_samplerDescriptorSize : m_cbvSrvUavDescriptorSize);

        _device->CopyDescriptorsSimple(
            1,
            dstCpuHandle,
            srcCpuHandle,
            isSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
} // namespace KryneEngine