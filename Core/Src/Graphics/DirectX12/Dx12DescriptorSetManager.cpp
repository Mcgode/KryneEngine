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
    namespace
    {
        union PackedIndex
        {
            struct {
                u32 m_type: Dx12DescriptorSetManager::kDescriptorTypeBits;
                u32 m_binding: 32 - Dx12DescriptorSetManager::kDescriptorTypeBits;
            };
            u32 m_packed;
        };

        constexpr size_t kCbvSrvUavBits = 20;
        constexpr size_t kSamplerBits = 11;
        constexpr size_t kIndicesBits = kCbvSrvUavBits + kSamplerBits;
        constexpr size_t frameBits = 64 - kIndicesBits;

        constexpr u64 kIndicesMask = BitUtils::BitMask<u64>(kIndicesBits) << frameBits;
        constexpr u64 kFrameMask = BitUtils::BitMask<u64>(frameBits);

        constexpr u64 kCbvSrvUavMask = BitUtils::BitMask<u64>(kCbvSrvUavBits) << (frameBits + kSamplerBits);
        constexpr u64 kSamplerMask = BitUtils::BitMask<u64>(kSamplerBits) << frameBits;

        constexpr u64 kSentinel = kCbvSrvUavMask | kFrameMask;
    }

    Dx12DescriptorSetManager::Dx12DescriptorSetManager(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_cbvSrvUavCacheDescriptorHeaps(_allocator)
        , m_cbvSrvUavCacheAllocator(_allocator, kCbvSrvUavCacheHeapSize)
        , m_samplerCacheDescriptorHeaps(_allocator)
        , m_samplerCacheAllocator(_allocator, kSamplerCacheHeapSize)
        , m_descriptorSetLayout(_allocator)
        , m_descriptorSets(_allocator)
    {}

    Dx12DescriptorSetManager::~Dx12DescriptorSetManager()
    {
        for (u8 i = 0; i < m_frameCount; i++)
        {
            m_allocator.Delete(m_cbvSrvUavCacheDescriptorHeaps[i]);
            m_allocator.Delete(m_samplerCacheDescriptorHeaps[i]);
        }

        for (auto* vector: m_releaseList)
        {
            m_allocator.Delete(vector);
        }
    }

    void Dx12DescriptorSetManager::Init(ID3D12Device* _device, const u8 _frameContextCount, const u8 _currentFrame)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::Init");

        m_cbvSrvUavCacheDescriptorHeaps.Resize(_frameContextCount);
        m_cbvSrvUavCacheDescriptorHeaps.InitAll(nullptr);
        m_cbvSrvUavGpuDescriptorHeaps.Resize(_frameContextCount);
        m_cbvSrvUavGpuDescriptorHeaps.InitAll(nullptr);
        m_samplerCacheDescriptorHeaps.Resize(_frameContextCount);
        m_samplerCacheDescriptorHeaps.InitAll(nullptr);
        m_samplerGpuDescriptorHeaps.Resize(_frameContextCount);
        m_samplerGpuDescriptorHeaps.InitAll(nullptr);

        m_frameCount = _frameContextCount;

        m_cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_samplerDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        for (auto i = 0u; i < _frameContextCount; i++)
        {
            {
                m_cbvSrvUavGpuDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
                const D3D12_DESCRIPTOR_HEAP_DESC heapDesc {
                    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    .NumDescriptors = m_cbvSrvUavGpuDescriptorHeapSize,
                    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                };
                Dx12Assert(_device->CreateDescriptorHeap(
                    &heapDesc,
                    IID_PPV_ARGS(&m_cbvSrvUavGpuDescriptorHeaps[i])));
#if !defined(KE_FINAL)
                Dx12SetName(m_cbvSrvUavGpuDescriptorHeaps[i].Get(), L"CBV/SRV/UAV descriptor GPU heap [%d]", i);
#endif
            }

            {
                m_samplerGpuDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
                const D3D12_DESCRIPTOR_HEAP_DESC heapDesc {
                    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                    .NumDescriptors = m_samplerGpuDescriptorHeapSize,
                    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                };
                Dx12Assert(_device->CreateDescriptorHeap(
                    &heapDesc,
                    IID_PPV_ARGS(&m_samplerGpuDescriptorHeaps[i])));
#if !defined(KE_FINAL)
                Dx12SetName(m_samplerGpuDescriptorHeaps[i].Get(), L"Sampler descriptor GPU heap [%d]", i);
#endif
            }
        }

        KE_ASSERT((1 << kCbvSrvUavBits) >= m_cbvSrvUavGpuDescriptorHeapSize);
        KE_ASSERT((1 << kSamplerBits) >= m_samplerGpuDescriptorHeapSize);

        // If not true, the sentinel value can represent a valid index
        KE_ASSERT((1 << kCbvSrvUavBits) - 1 >= m_cbvSrvUavGpuDescriptorHeapSize);
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
            const DescriptorBindingDesc binding = _desc.m_bindings[i];

            RangeType rangeType {};
            DescriptorType descriptorType {};
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
            if (binding.m_bindingIndex == DescriptorBindingDesc::kImplicitBindingIndex)
            {
                // Implicit index: append after the running per-range-type total.
                _bindingIndices[i] = PackedIndex { .m_type = static_cast<u32>(descriptorType), .m_binding = total }.m_packed;
                total += binding.m_count;
            }
            else
            {
                // Explicit index: honour the requested binding slot.
                KE_ASSERT(total <= binding.m_bindingIndex);
                _bindingIndices[i] = PackedIndex { .m_type = static_cast<u32>(descriptorType), .m_binding = binding.m_bindingIndex }.m_packed;
                total = binding.m_bindingIndex + binding.m_count;
            }

            const u8 rangeTypeIndex = static_cast<u8>(rangeType);
            visibilities[rangeTypeIndex] |= binding.m_visibility;
        }

        const GenPool::Handle handle = m_descriptorSetLayout.Allocate();
        *m_descriptorSetLayout.Get(handle) = { .m_visibilities = visibilities, .m_totals = totals };
        return { handle };
    }

    bool Dx12DescriptorSetManager::DestroyDescriptorSetLayout(const DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetLayout.Free(_layout.m_handle);
    }

    DescriptorSetHandle Dx12DescriptorSetManager::CreateDescriptorSet(const DescriptorSetLayoutHandle _layout)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::CreateDescriptorSet");

        VERIFY_OR_RETURN(_layout != GenPool::kInvalidHandle, { GenPool::kInvalidHandle });
        LayoutData* pData = m_descriptorSetLayout.Get(_layout.m_handle);
        VERIFY_OR_RETURN(pData != nullptr, { GenPool::kInvalidHandle });
        eastl::array<u32, kRangeTypesCount>& totals = pData->m_totals;

        const GenPool::Handle handle = m_descriptorSets.Allocate();
        DescriptorSet* set = m_descriptorSets.Get(handle);

        constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

        u32 total = 0;
        for (auto i = 0u; i < samplerIndex; i++)
        {
            set->m_sizes[i] = totals[i];
            total += totals[i];
        }
        KE_ASSERT(total < UINT16_MAX);

        {
            u32 offset = total > 0 ? m_cbvSrvUavCacheAllocator.Allocate(total) : 0;
            for (auto i = 0u; i < samplerIndex; i++)
            {
                set->m_offsets[i] = offset;
                offset += totals[i];
            }
        }

        set->m_sizes[samplerIndex] = totals[samplerIndex];
        set->m_offsets[samplerIndex] = totals[samplerIndex] > 0 ? m_samplerCacheAllocator.Allocate(totals[samplerIndex]) : 0;

        set->m_gpuHeapIndex = kSentinel;

        return { handle };
    }

    bool Dx12DescriptorSetManager::DestroyDescriptorSet(const DescriptorSetHandle _set)
    {
        DescriptorSet set {};
        if (m_descriptorSets.Free(_set.m_handle, &set))
        {
            constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

            u32 total = 0;
            for (auto i = 0u; i < samplerIndex; i++)
            {
                total += set.m_sizes[i];
            }
            if (total > 0)
                m_cbvSrvUavCacheAllocator.Deallocate(set.m_offsets[0], total);

            if (set.m_sizes[samplerIndex] > 0)
                m_samplerCacheAllocator.Deallocate(set.m_offsets[samplerIndex], set.m_sizes[samplerIndex]);

            return true;
        }
        return false;
    }

    void Dx12DescriptorSetManager::UpdateDescriptorSet(
        const DescriptorSetHandle _descriptorSet,
        const Dx12Resources& _resources,
        const eastl::span<const DescriptorSetWriteInfo>& _writes,
        const bool _singleFrame,
        ID3D12Device* _device,
        const u64 _frameId)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::UpdateDescriptorSet");

        DescriptorSet* pSet = m_descriptorSets.Get(_descriptorSet.m_handle);
        if (pSet == nullptr)
        {
            return;
        }

#if !defined(KE_FINAL)
        {
            const u64 index = std::atomic_ref(pSet->m_gpuHeapIndex).load(std::memory_order::relaxed);
            KE_ASSERT_MSG((index & kFrameMask) != (_frameId & kFrameMask) || index == kSentinel,
                "Descriptor set updated after being bound this frame");
        }
#endif

        for (const auto& writeDesc: _writes)
        {
            const PackedIndex baseIndex { .m_packed = writeDesc.m_index };
            for (auto i = 0u; i < writeDesc.m_descriptorData.size(); i++)
            {
                const auto descriptorType = static_cast<DescriptorType>(baseIndex.m_type);
                const bool isSampler = descriptorType == DescriptorType::Sampler;

                CD3DX12_CPU_DESCRIPTOR_HANDLE srcCpuHandle {};
                RangeType rangeType {};
                switch (descriptorType)
                {
                case DescriptorType::BufferCbv:
                    srcCpuHandle = _resources.m_bufferViews.Get(writeDesc.m_descriptorData[i].m_handle)->m_cbvHandle;
                    rangeType = RangeType::Cbv;
                    break;
                case DescriptorType::BufferSrv:
                    srcCpuHandle = _resources.m_bufferViews.Get(writeDesc.m_descriptorData[i].m_handle)->m_srvHandle;
                    rangeType = RangeType::Srv;
                    break;
                case DescriptorType::BufferUav:
                    srcCpuHandle = _resources.m_bufferViews.Get(writeDesc.m_descriptorData[i].m_handle)->m_uavHandle;
                    rangeType = RangeType::Uav;
                    break;
                case DescriptorType::TextureSrv:
                    srcCpuHandle = _resources.m_textureViews.Get(writeDesc.m_descriptorData[i].m_handle)->m_srvHandle;
                    rangeType = RangeType::Srv;
                    break;
                case DescriptorType::TextureUav:
                    srcCpuHandle = _resources.m_textureViews.Get(writeDesc.m_descriptorData[i].m_handle)->m_uavHandle;
                    rangeType = RangeType::Uav;
                    break;
                case DescriptorType::Sampler:
                    srcCpuHandle = *_resources.m_samplers.Get(writeDesc.m_descriptorData[i].m_handle);
                    rangeType = RangeType::Sampler;
                    break;
                default:
                    KE_ERROR("Invalid descriptor type");
                    break;
                }
                if (srcCpuHandle.ptr == 0)
                {
                    continue;
                }

                const u32 relativeIndex = baseIndex.m_binding + writeDesc.m_arrayOffset + i;
                KE_ASSERT(pSet->m_sizes[static_cast<u32>(rangeType)] > relativeIndex);
                const u32 fullIndex = relativeIndex + pSet->m_offsets[static_cast<u32>(rangeType)];
                const u32 heapSize = isSampler ? kSamplerCacheHeapSize : kCbvSrvUavCacheHeapSize;
                const u32 heapIndex = fullIndex / heapSize;
                const u32 index = fullIndex % heapSize;

                if (_singleFrame)
                {
                    const CD3DX12_CPU_DESCRIPTOR_HANDLE dstCpuHandle(
                        Dx12CpuDescriptorHandleForHeapStart(GetCacheDescriptorHeap(_device, heapIndex, _frameId % m_frameCount, isSampler)),
                        static_cast<s32>(index),
                        isSampler ? m_samplerDescriptorSize : m_cbvSrvUavDescriptorSize);

                    _device->CopyDescriptorsSimple(
                        1,
                        dstCpuHandle,
                        srcCpuHandle,
                        isSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
                else
                {
                    for (u8 j = 0; j < m_frameCount; j++)
                    {
                        const CD3DX12_CPU_DESCRIPTOR_HANDLE dstCpuHandle(
                            Dx12CpuDescriptorHandleForHeapStart(GetCacheDescriptorHeap(_device, heapIndex, j, isSampler)),
                            static_cast<s32>(index),
                            isSampler ? m_samplerDescriptorSize : m_cbvSrvUavDescriptorSize);

                        _device->CopyDescriptorsSimple(
                            1,
                            dstCpuHandle,
                            srcCpuHandle,
                            isSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    }
                }
            }
        }
    }

    void Dx12DescriptorSetManager::SetGraphicsDescriptorSets(
        ID3D12Device* _device,
        const CommandListSet* _commandListSet,
        const eastl::span<const DescriptorSetHandle>& _sets,
        const u16* _tableSetOffsets,
        const u32 _offset,
        const u64 _frameId)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::SetGraphicsDescriptorSets");

        constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

        for (auto setIndex = 0u; setIndex < _sets.size(); setIndex++)
        {
            u32 tableIndex = _tableSetOffsets[_offset + setIndex];

            const DescriptorSetHandle set = _sets[setIndex];
            DescriptorSet* pSet = m_descriptorSets.Get(set.m_handle);
            VERIFY_OR_RETURN_VOID(pSet != nullptr);

            u32 cbvSrvUavTotal = 0;
            for (auto i = 0u; i < samplerIndex; i++) { cbvSrvUavTotal += pSet->m_sizes[i]; }

            const SetIndices indices = this->GetShadowedDescriptorSetIndex(_device, pSet, _frameId);

            if (cbvSrvUavTotal > 0)
            {
                const CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    Dx12GpuDescriptorHandleForHeapStart(m_cbvSrvUavGpuDescriptorHeaps[_frameId % m_frameCount]),
                    static_cast<s32>(indices.m_cbvSrvUavIndex),
                    m_cbvSrvUavDescriptorSize);
                _commandListSet->m_commandList->SetGraphicsRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }

            if (pSet->m_sizes[samplerIndex] > 0)
            {
                const CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    Dx12GpuDescriptorHandleForHeapStart(m_samplerGpuDescriptorHeaps[_frameId % m_frameCount]),
                    static_cast<s32>(indices.m_samplerIndex),
                    m_samplerDescriptorSize);
                _commandListSet->m_commandList->SetGraphicsRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }
        }
    }

    void Dx12DescriptorSetManager::SetComputeDescriptorSets(
        ID3D12Device* _device,
        const CommandListSet* _commandListSet,
        const eastl::span<const DescriptorSetHandle>& _sets,
        const u16* _tableSetOffsets,
        const u32 _offset,
        const u64 _frameId)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::SetComputeDescriptorSets");

        constexpr u32 samplerIndex = static_cast<u32>(RangeType::Sampler);

        for (auto setIndex = 0u; setIndex < _sets.size(); setIndex++)
        {
            u32 tableIndex = _tableSetOffsets[_offset + setIndex];
            const DescriptorSetHandle set = _sets[setIndex];
            DescriptorSet* pSet = m_descriptorSets.Get(set.m_handle);
            VERIFY_OR_RETURN_VOID(pSet != nullptr);

            u32 cbvSrvUavTotal = 0;
            for (auto i = 0u; i < samplerIndex; i++) { cbvSrvUavTotal += pSet->m_sizes[i]; }

            const SetIndices indices = this->GetShadowedDescriptorSetIndex(_device, pSet, _frameId);

            if (cbvSrvUavTotal > 0)
            {
                const CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    Dx12GpuDescriptorHandleForHeapStart(m_cbvSrvUavGpuDescriptorHeaps[_frameId % m_frameCount]),
                    static_cast<s32>(indices.m_cbvSrvUavIndex),
                    m_cbvSrvUavDescriptorSize);
                _commandListSet->m_commandList->SetComputeRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }

            if (pSet->m_sizes[samplerIndex] > 0)
            {
                const CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                    Dx12GpuDescriptorHandleForHeapStart(m_samplerGpuDescriptorHeaps[_frameId % m_frameCount]),
                    static_cast<s32>(indices.m_samplerIndex),
                    m_samplerDescriptorSize);
                _commandListSet->m_commandList->SetComputeRootDescriptorTable(tableIndex, handle);
                tableIndex++;
            }
        }
    }

    void Dx12DescriptorSetManager::OnBeginGraphicsCommandList(
        const CommandListSet* _commandListSet,
        const u8 _frameIndex) const
    {
        ID3D12DescriptorHeap* heaps[2] = {
            m_cbvSrvUavGpuDescriptorHeaps[_frameIndex].Get(),
            m_samplerGpuDescriptorHeaps[_frameIndex].Get(),
        };
        _commandListSet->m_commandList->SetDescriptorHeaps(2, heaps);
    }

    void Dx12DescriptorSetManager::NextFrame(ID3D12Device* _device, const Dx12Resources& _resources, const u8 _frameIndex)
    {
        KE_ZoneScopedFunction("Dx12DescriptorSetManager::NextFrame");

        {
            const auto lock = m_cacheLock.AutoLock();
            for (auto* vector : m_releaseList)
            {
                m_allocator.Delete(vector);
            }
            m_releaseList.clear();
        }

        m_cbvSrvUavLinearAllocIndex.store(0, std::memory_order::release);
        m_samplerLinearAllocIndex.store(0, std::memory_order::release);

        m_descriptorSets.FlushDeferredFrees();
        m_descriptorSetLayout.FlushDeferredFrees();
    }

    const Dx12DescriptorSetManager::LayoutData* Dx12DescriptorSetManager::GetDescriptorSetLayoutData(
        const DescriptorSetLayoutHandle _layout) const
    {
        return m_descriptorSetLayout.Get(_layout.m_handle);
    }

    ID3D12DescriptorHeap* Dx12DescriptorSetManager::GetCacheDescriptorHeap(
        ID3D12Device* _device,
        const u32 _index,
        const u8 _frameIndex,
        const bool _sampler)
    {
        auto& heaps = (_sampler ? m_samplerCacheDescriptorHeaps : m_cbvSrvUavCacheDescriptorHeaps)[_frameIndex];
        auto* vector = std::atomic_ref(heaps).load(std::memory_order::acquire);
        if (vector != nullptr && vector->size() > _index)
        {
            return vector->at(_index).Get();
        }

        const auto lock = m_cacheLock.AutoLock();

        vector = std::atomic_ref(heaps).load(std::memory_order::acquire);
        if (vector != nullptr && vector->size() > _index)
        {
            return vector->at(_index).Get();
        }

        auto* newVector = m_allocator.New<eastl::vector<ComPtr<ID3D12DescriptorHeap>>>(m_allocator);
        newVector->reserve(_index + 1);
        if (vector != nullptr)
        {
            for (const auto& heap : *vector)
            {
                newVector->push_back(heap);
            }
        }

        const size_t size = vector != nullptr ? vector->size() : 0;
        for (size_t i = size; i <= _index; ++i)
        {
            ComPtr<ID3D12DescriptorHeap>& heap = newVector->emplace_back();

            const D3D12_DESCRIPTOR_HEAP_DESC desc {
                .Type = _sampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = _sampler ? kSamplerCacheHeapSize : kCbvSrvUavCacheHeapSize,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            };

            Dx12Assert(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
#if !defined(KE_FINAL)
            Dx12SetName(
                heap.Get(),
                _sampler
                    ? "Sampler Cache Descriptor Heap [%d][%zu]"
                    : "CBV/SRV/UAV Cache Descriptor Heap [%d][%zu]",
                _frameIndex,
                i);
#endif
        }

        std::atomic_ref(heaps).store(newVector, std::memory_order::release);
        m_releaseList.push_back(vector);

        return newVector->at(_index).Get();
    }

    Dx12DescriptorSetManager::SetIndices Dx12DescriptorSetManager::GetShadowedDescriptorSetIndex(
        ID3D12Device* _device,
        DescriptorSet* _descriptorSet,
        const u64 _frameId)
    {
        const u64 currentFrame = _frameId & kFrameMask;
        const std::atomic_ref gpuIndex { _descriptorSet->m_gpuHeapIndex };

        u64 index = gpuIndex.load(std::memory_order::acquire);

        bool upload = false;
        if ((index & kFrameMask) != currentFrame || index == kSentinel)
            upload = gpuIndex.compare_exchange_strong(index, currentFrame | kIndicesMask, std::memory_order::acq_rel);

        if (!upload)
        {
            while ((index & kIndicesMask) == kIndicesMask)
            {
                Threads::CpuYield();
                index = gpuIndex.load(std::memory_order::acquire);
            }
            return {
                .m_cbvSrvUavIndex = static_cast<u32>((index & kCbvSrvUavMask) >> (frameBits + kSamplerBits)),
                .m_samplerIndex = static_cast<u32>((index & kSamplerMask) >> frameBits),
            };
        }

        constexpr u32 samplerTypeIndex = static_cast<u32>(RangeType::Sampler);

        u32 total = 0;
        for (auto i = 0u; i < samplerTypeIndex; i++)
        {
            total += _descriptorSet->m_sizes[i];
        }
        const u64 cbvSrvUavIndex = m_cbvSrvUavLinearAllocIndex.fetch_add(total, std::memory_order::acq_rel);
        const u64 samplerIndex = m_samplerLinearAllocIndex.fetch_add(_descriptorSet->m_sizes[samplerTypeIndex], std::memory_order::acq_rel);

        // If any of these two assets triggers, it means we need to implement a mechanism to handle multiple GPU heaps.
        KE_ASSERT(cbvSrvUavIndex + total <= m_cbvSrvUavGpuDescriptorHeapSize);
        KE_ASSERT(samplerIndex + _descriptorSet->m_sizes[samplerTypeIndex] <= m_samplerGpuDescriptorHeapSize);

        const u64 newIndex = currentFrame
            | (samplerIndex << frameBits)
            | (cbvSrvUavIndex << (frameBits + kSamplerBits));
        gpuIndex.store(newIndex, std::memory_order::release);

        const u8 frameIndex = static_cast<u8>(_frameId % m_frameCount);

        if (total > 0)
        {
            _device->CopyDescriptorsSimple(
               total,
               CD3DX12_CPU_DESCRIPTOR_HANDLE(
                   Dx12CpuDescriptorHandleForHeapStart(m_cbvSrvUavGpuDescriptorHeaps[frameIndex]),
                   static_cast<s32>(cbvSrvUavIndex),
                   m_cbvSrvUavDescriptorSize),
               CD3DX12_CPU_DESCRIPTOR_HANDLE(
                   Dx12CpuDescriptorHandleForHeapStart(GetCacheDescriptorHeap(
                       _device,
                       _descriptorSet->m_offsets[0] / kCbvSrvUavCacheHeapSize,
                       frameIndex,
                       false)),
                   static_cast<s32>(_descriptorSet->m_offsets[0] % kCbvSrvUavCacheHeapSize),
                   m_cbvSrvUavDescriptorSize),
               D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        if (_descriptorSet->m_sizes[samplerTypeIndex] > 0)
        {
            _device->CopyDescriptorsSimple(
               _descriptorSet->m_sizes[samplerTypeIndex],
               CD3DX12_CPU_DESCRIPTOR_HANDLE(
                   Dx12CpuDescriptorHandleForHeapStart(m_samplerGpuDescriptorHeaps[frameIndex]),
                   static_cast<s32>(samplerIndex),
                   m_samplerDescriptorSize),
               CD3DX12_CPU_DESCRIPTOR_HANDLE(
                   Dx12CpuDescriptorHandleForHeapStart(GetCacheDescriptorHeap(
                       _device,
                       _descriptorSet->m_offsets[samplerTypeIndex] / kSamplerCacheHeapSize,
                       frameIndex,
                       true)),
                   static_cast<s32>(_descriptorSet->m_offsets[samplerTypeIndex] % kSamplerCacheHeapSize),
                   m_samplerDescriptorSize),
               D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }

        return {
            .m_cbvSrvUavIndex = static_cast<u32>(cbvSrvUavIndex),
            .m_samplerIndex = static_cast<u32>(samplerIndex),
        };
    }
} // namespace KryneEngine