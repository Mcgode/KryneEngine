/**
 * @file
 * @author Max Godefroy
 * @date 13/08/2024.
 */

#pragma once

#include "Graphics/Vulkan/VkHeaders.hpp"
#include "KryneEngine/Core/Common/Utils/MultiFrameTracking.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"

namespace KryneEngine
{
    struct DescriptorSetDesc;
    struct VkResources;

    class VkDescriptorSetManager
    {
    public:
        friend class VkGraphicsContext;

        explicit VkDescriptorSetManager(AllocatorInstance _allocator);
        ~VkDescriptorSetManager();

        void Init(u8 _frameCount, u8 _frameIndex, bool _partiallyBoundDescriptors);

        [[nodiscard]] DescriptorSetLayoutHandle CreateDescriptorSetLayout(
            const DescriptorSetDesc& _desc,
            u32* _bindingIndices,
            VkDevice _device);
        bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout, VkDevice _device);

        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout(DescriptorSetLayoutHandle _layout);

        [[nodiscard]] DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout, VkDevice _device);
        bool DestroyDescriptorSet(DescriptorSetHandle _descriptorSet, VkDevice _device);

        void UpdateDescriptorSet(
            DescriptorSetHandle _descriptorSet,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame,
            VkDevice _device,
            const VkResources& _resources,
            u8 _frameIndex);

        void NextFrame(VkDevice _device, const VkResources& _resources, u8 _frameIndex);

    private:
        u64 m_frameCount = 0;
        bool m_partiallyBoundDescriptors = false;

        struct LayoutData
        {
            VkDescriptorSetLayout m_layout;
            eastl::vector<VkDescriptorPoolSize> m_poolSizes;
        };
        GenerationalPool<LayoutData> m_descriptorSetLayouts;

        GenerationalPool<VkDescriptorPool> m_descriptorSetPools;
        eastl::vector<VkDescriptorSet> m_descriptorSets;

        struct WriteOp
        {
            eastl::vector<DescriptorSetWriteInfo::DescriptorData> m_descriptorData;
            DescriptorSetHandle m_descriptorSet;
            u32 m_index;
            u16 m_arrayOffset;
        };

        MultiFrameDataTracker<WriteOp> m_multiFrameTracker;

        eastl::vector<WriteOp> m_tmpWriteOps;
        eastl::vector<VkWriteDescriptorSet> m_tmpWrites;

        union DescriptorData
        {
            static_assert(sizeof(VkDescriptorImageInfo) == sizeof(VkDescriptorBufferInfo), "Types must take full size");

            VkDescriptorImageInfo m_imageInfo;
            VkDescriptorBufferInfo m_bufferImageInfo;
        };
        eastl::vector<DescriptorData> m_tmpDescriptorData;

        [[nodiscard]] AllocatorInstance GetAllocator() const;

        void _ProcessUpdates(
            const eastl::vector<WriteOp>& _writes,
            VkDevice _device,
            const VkResources& _resources,
            u8 _frameIndex);
    };
} // namespace KryneEngine