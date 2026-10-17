/**
 * @file
 * @author Max Godefroy
 * @date 11/08/2024.
 */

#pragma once

#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "Graphics/DirectX12/Dx12Types.hpp"
#include "KryneEngine/Core/Common/Utils/MultiFrameTracking.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Memory/Allocators/ExternalRangeAllocator.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

namespace KryneEngine
{
    class Dx12Resources;

    struct DescriptorSetDesc;
    struct DescriptorSetWriteInfo;

    class Dx12DescriptorSetManager
    {
    public:
        explicit Dx12DescriptorSetManager(AllocatorInstance _allocator);

        ~Dx12DescriptorSetManager();

        void Init(ID3D12Device* _device, u8 _frameContextCount, u8 _currentFrame);

        [[nodiscard]] DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetDesc& _desc, u32* _bindingIndices);
        bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout);

        [[nodiscard]] DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout);
        bool DestroyDescriptorSet(DescriptorSetHandle _set);

        void UpdateDescriptorSet(
            DescriptorSetHandle _descriptorSet,
            const Dx12Resources& _resources,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame,
            ID3D12Device* _device,
            u64 _frameId);

        void SetGraphicsDescriptorSets(
            ID3D12Device* _device,
            const CommandListSet* _commandListSet,
            const eastl::span<const DescriptorSetHandle>& _sets,
            const u16* _tableSetOffsets,
            u32 _offset,
            u64 _frameId);

        void SetComputeDescriptorSets(
            ID3D12Device* _device,
            const CommandListSet* _commandListSet,
            const eastl::span<const DescriptorSetHandle>& _sets,
            const u16* _tableSetOffsets,
            u32 _offset,
            u64 _frameId);

        void OnBeginGraphicsCommandList(const CommandListSet* _commandListSet, u8 _frameIndex) const;

        void NextFrame(ID3D12Device* _device, const Dx12Resources& _resources, u8 _frameIndex);

        enum class RangeType: u32
        {
            Cbv = 0,
            Srv,
            Uav,
            Sampler,
            COUNT,
        };
        static constexpr u32 kRangeTypesCount = static_cast<u32>(RangeType::COUNT);

        enum class DescriptorType: u8
        {
            BufferCbv,
            BufferSrv,
            TextureSrv,
            BufferUav,
            TextureUav,
            Sampler,
            COUNT,
        };
        static constexpr u32 kDescriptorTypesCount = static_cast<u32>(DescriptorType::COUNT);
        static constexpr u32 kDescriptorTypeBits = 3;
        static_assert(kDescriptorTypesCount <= (1 << kDescriptorTypeBits), "Not enough bits for DescriptorType");;

        struct LayoutData
        {
            eastl::array<ShaderVisibility, kRangeTypesCount> m_visibilities;
            eastl::array<u32, kRangeTypesCount> m_totals;
        };

        const LayoutData* GetDescriptorSetLayoutData(DescriptorSetLayoutHandle _layout) const;

    private:
        AllocatorInstance m_allocator;
        u8 m_frameCount = 0;

        static constexpr u32 kCbvSrvUavCacheHeapSize = 8192;
        DynamicArray<eastl::vector<ComPtr<ID3D12DescriptorHeap>>*> m_cbvSrvUavCacheDescriptorHeaps;
        DynamicArray<ComPtr<ID3D12DescriptorHeap>> m_cbvSrvUavGpuDescriptorHeaps;
        u32 m_cbvSrvUavDescriptorSize = 0;
        u32 m_cbvSrvUavGpuDescriptorHeapSize = 0;
        ExternalRangeAllocator m_cbvSrvUavCacheAllocator;
        std::atomic<u32> m_cbvSrvUavLinearAllocIndex { 0 };

        static constexpr u32 kSamplerCacheHeapSize = 1024;
        DynamicArray<eastl::vector<ComPtr<ID3D12DescriptorHeap>>*> m_samplerCacheDescriptorHeaps;
        DynamicArray<ComPtr<ID3D12DescriptorHeap>> m_samplerGpuDescriptorHeaps;
        u32 m_samplerDescriptorSize = 0;
        u32 m_samplerGpuDescriptorHeapSize = 0;
        ExternalRangeAllocator m_samplerCacheAllocator;
        std::atomic<u32> m_samplerLinearAllocIndex { 0 };

        GenerationalPool<LayoutData> m_descriptorSetLayout;

        struct DescriptorSet
        {
            eastl::array<u16, static_cast<u32>(RangeType::COUNT)> m_sizes;
            eastl::array<u32, static_cast<u32>(RangeType::COUNT)> m_offsets;
            u64 m_gpuHeapIndex;
        };
        GenerationalPool<DescriptorSet> m_descriptorSets;

        LightweightMutex m_cacheLock;
        eastl::vector<eastl::vector<ComPtr<ID3D12DescriptorHeap>>*> m_releaseList;

        ID3D12DescriptorHeap* GetCacheDescriptorHeap(ID3D12Device* _device, u32 _index, u8 _frameIndex, bool _sampler);

        struct SetIndices
        {
            u32 m_cbvSrvUavIndex;
            u32 m_samplerIndex;
        };
        SetIndices GetShadowedDescriptorSetIndex(ID3D12Device* _device, DescriptorSet* _descriptorSet, u64 _frameId);
    };
} // namespace KryneEngine