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

namespace KryneEngine
{
    class Dx12Resources;

    struct DescriptorSetDesc;
    struct DescriptorSetWriteInfo;

    class Dx12DescriptorSetManager
    {
    public:
        Dx12DescriptorSetManager(AllocatorInstance _allocator);

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
            u8 _frameIndex);

        void SetGraphicsDescriptorSets(
            CommandList _commandList,
            const eastl::span<const DescriptorSetHandle>& _sets,
            const u16* _tableSetOffsets,
            u32 _offset,
            u8 _currentFrame);

        void SetComputeDescriptorSets(
            CommandList _commandList,
            const eastl::span<const DescriptorSetHandle>& _sets,
            const u16* _tableSetOffsets,
            u32 _offset,
            u8 _currentFrame);

        void OnBeginGraphicsCommandList(CommandList _commandList, u8 _frameIndex);

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

        const LayoutData* GetDescriptorSetLayoutData(DescriptorSetLayoutHandle _layout);

    private:
        static constexpr u32 kCbvSrvUavHeapSize = 1024;
        DynamicArray<ComPtr<ID3D12DescriptorHeap>> m_cbvSrvUavGpuDescriptorHeaps;
        u32 m_cbvSrvUavDescriptorSize = 0;
        std::atomic<u32> m_cbvSrvUavLinearAllocIndex { 0 };

        static constexpr u32 kSamplerHeapSize = 64;
        DynamicArray<ComPtr<ID3D12DescriptorHeap>> m_samplerGpuDescriptorHeaps;
        u32 m_samplerDescriptorSize = 0;
        std::atomic<u32> m_samplerLinearAllocIndex { 0 };

        GenerationalPool<LayoutData> m_descriptorSetLayout;

        struct DescriptorSetRanges
        {
            eastl::array<u16, static_cast<u32>(RangeType::COUNT)> m_sizes;
            eastl::array<u32, static_cast<u32>(RangeType::COUNT)> m_offsets;
        };
        GenerationalPool<DescriptorSetRanges> m_descriptorSets;

        struct TrackedData
        {
            DescriptorSetHandle m_descriptorSet;
            GenPool::Handle m_object;
            u32 m_packedIndex;
        };
        MultiFrameDataTracker<TrackedData> m_multiFrameUpdateTracker;

        void _ProcessUpdate(
            ID3D12Device* _device,
            const Dx12Resources& _resources,
            const Dx12DescriptorSetManager::TrackedData& _data,
            u8 _currentFrame);
    };
} // namespace KryneEngine