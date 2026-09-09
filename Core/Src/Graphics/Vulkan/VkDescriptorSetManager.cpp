/**
 * @file
 * @author Max Godefroy
 * @date 13/08/2024.
 */

#include "Graphics/Vulkan/VkDescriptorSetManager.hpp"

#include <EASTL/vector_map.h>

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkResources.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"

namespace KryneEngine
{
    union PackedIndex
    {
        static constexpr u8 kTypeBits = 10;

        struct {
            u32 m_type : kTypeBits;
            u32 m_binding : 32 - kTypeBits;
        };
        u32 m_packed;
    };

    VkDescriptorSetManager::VkDescriptorSetManager(AllocatorInstance _allocator)
        : m_descriptorSetLayouts(_allocator)
        , m_descriptorSetPools(_allocator)
        , m_descriptorSets(_allocator)
        , m_tmpWriteOps(_allocator)
        , m_tmpWrites(_allocator)
        , m_tmpDescriptorData(_allocator)
    {}

    VkDescriptorSetManager::~VkDescriptorSetManager() = default;

    void VkDescriptorSetManager::Init(u8 _frameCount, u8 _frameIndex, bool _partiallyBoundDescriptors)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::Init");
        m_frameCount = _frameCount;
        m_partiallyBoundDescriptors = _partiallyBoundDescriptors;
        m_multiFrameTracker.Init(GetAllocator(), _frameCount, _frameIndex);
    }

    DescriptorSetLayoutHandle VkDescriptorSetManager::CreateDescriptorSetLayout(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices,
        VkDevice _device)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::CreateDescriptorSetLayout");

        eastl::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(_desc.m_bindings.size());

        eastl::vector_map<VkDescriptorType, u32> countPerType;

        for (auto i = 0u; i < _desc.m_bindings.size(); i++)
        {
            const DescriptorBindingDesc& binding = _desc.m_bindings[i];
            const VkDescriptorType type = VkHelperFunctions::ToVkDescriptorType(binding.m_type);
            const u32 bindingIndex = binding.m_bindingIndex == DescriptorBindingDesc::kImplicitBindingIndex ? i : binding.m_bindingIndex;
            bindings.push_back(VkDescriptorSetLayoutBinding {
                .binding = bindingIndex,
                .descriptorType = type,
                .descriptorCount = binding.m_count,
                .stageFlags = VkHelperFunctions::ToVkShaderStageFlags(binding.m_visibility),
                .pImmutableSamplers = nullptr,
            });
            KE_ASSERT(type < (1 << PackedIndex::kTypeBits));

            countPerType[type] += binding.m_count;

            const PackedIndex packedIndex {
                .m_type = static_cast<u32>(type),
                .m_binding = bindingIndex,
            };
            _bindingIndices[i] = packedIndex.m_packed;
        }

        // Flag arrayed bindings as partially bound so the shader may leave unused slots unwritten.
        eastl::vector<VkDescriptorBindingFlags> bindingFlags { this->m_descriptorSets.get_allocator() };
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo {};
        const void* createInfoNext = nullptr;
        if (m_partiallyBoundDescriptors)
        {
            bindingFlags.reserve(bindings.size());
            for (const VkDescriptorSetLayoutBinding& binding : bindings)
            {
                bindingFlags.push_back(binding.descriptorCount > 1 ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0);
            }
            bindingFlagsInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = static_cast<u32>(bindingFlags.size()),
                .pBindingFlags = bindingFlags.data(),
            };
            createInfoNext = &bindingFlagsInfo;
        }

        const VkDescriptorSetLayoutCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = createInfoNext,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout layout;
        VkAssert(vkCreateDescriptorSetLayout(
            _device,
            &createInfo,
            nullptr,
            &layout));

        const GenPool::Handle handle = m_descriptorSetLayouts.Allocate();
        auto* data = m_descriptorSetLayouts.Get(handle);
        data->m_layout = layout;
        data->m_poolSizes.clear();
        data->m_poolSizes.set_allocator(GetAllocator());
        for (auto [type, count]: countPerType)
        {
            data->m_poolSizes.push_back(VkDescriptorPoolSize { type, static_cast<u32>(count * m_frameCount) });
        }

        return { handle };
    }

    bool VkDescriptorSetManager::DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::DestroyDescriptorSetLayout");

        LayoutData data;
        if (m_descriptorSetLayouts.Free(_layout.m_handle, &data))
        {
            vkDestroyDescriptorSetLayout(_device, data.m_layout, nullptr);
            return true;
        }
        return false;
    }

    VkDescriptorSetLayout VkDescriptorSetManager::GetDescriptorSetLayout(DescriptorSetLayoutHandle _layout)
    {
        const LayoutData* pData = m_descriptorSetLayouts.Get(_layout.m_handle);
        VERIFY_OR_RETURN(pData != nullptr, VK_NULL_HANDLE);
        return pData->m_layout;
    }

    DescriptorSetHandle VkDescriptorSetManager::CreateDescriptorSet(DescriptorSetLayoutHandle _layout, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::CreateDescriptorSet");

        VERIFY_OR_RETURN(_layout != GenPool::kInvalidHandle, {GenPool::kInvalidHandle });
        LayoutData* pLayoutData = m_descriptorSetLayouts.Get(_layout.m_handle);
        VERIFY_OR_RETURN(pLayoutData != nullptr, {GenPool::kInvalidHandle });

        // Create descriptor pool
        VkDescriptorPool pool;
        {
            VkDescriptorPoolCreateInfo createInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = 0,
                .maxSets = static_cast<u32>(m_frameCount),
                .poolSizeCount = static_cast<u32>(pLayoutData->m_poolSizes.size()),
                .pPoolSizes = pLayoutData->m_poolSizes.data(),
            };

            VkAssert(vkCreateDescriptorPool(
                _device,
                &createInfo,
                nullptr,
                &pool));
        }

        const GenPool::Handle handle = m_descriptorSetPools.Allocate();
        *m_descriptorSetPools.Get(handle) = pool;

        // Allocate descriptor sets
        {
            const u64 offset = static_cast<u64>(handle.m_index) * m_frameCount;
            if (m_descriptorSets.size() < offset + m_frameCount)
            {
                m_descriptorSets.resize(offset + m_frameCount, VK_NULL_HANDLE);
            }

            DynamicArray<VkDescriptorSetLayout> layouts(m_frameCount);
            layouts.InitAll(pLayoutData->m_layout);

            const VkDescriptorSetAllocateInfo allocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = pool,
                .descriptorSetCount = static_cast<u32>(m_frameCount),
                .pSetLayouts = layouts.Data(),
            };
            VkAssert(vkAllocateDescriptorSets(
                _device,
                &allocateInfo,
                m_descriptorSets.begin() + offset));
        }

        return { handle };
    }

    bool VkDescriptorSetManager::DestroyDescriptorSet(DescriptorSetHandle _descriptorSet, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::DestroyDescriptorSet");

        VkDescriptorPool pool;
        if (m_descriptorSetPools.Free(_descriptorSet.m_handle, &pool))
        {
            // Descriptor sets in the vector can be left as loose memory, their memory location will be reallocated
            vkDestroyDescriptorPool(_device, pool, nullptr);
            return true;
        }
        return false;
    }

    void VkDescriptorSetManager::UpdateDescriptorSet(
        DescriptorSetHandle _descriptorSet,
        const eastl::span<const DescriptorSetWriteInfo>& _writes,
        bool _singleFrame,
        VkDevice _device,
        const VkResources& _resources,
        u8 _frameIndex)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::UpdateDescriptorSet");

        m_tmpWriteOps.clear();

        for (const auto& write: _writes)
        {
            WriteOp& writeOp = m_tmpWriteOps.emplace_back();
            writeOp.m_descriptorSet = _descriptorSet;
            writeOp.m_index = write.m_index;
            writeOp.m_arrayOffset = write.m_arrayOffset;

            writeOp.m_descriptorData.set_allocator(GetAllocator());
            writeOp.m_descriptorData.reserve(write.m_descriptorData.size());
            writeOp.m_descriptorData.insert(writeOp.m_descriptorData.end(), write.m_descriptorData.begin(), write.m_descriptorData.end());;

            if (!_singleFrame)
            {
                m_multiFrameTracker.TrackForOtherFrames(writeOp);
            }
        }

        _ProcessUpdates(m_tmpWriteOps, _device, _resources, _frameIndex);
    }

    void VkDescriptorSetManager::NextFrame(VkDevice _device, const VkResources& _resources, u8 _frameIndex)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::NextFrame");

        m_multiFrameTracker.AdvanceToNextFrame();

        _ProcessUpdates(m_multiFrameTracker.GetData(), _device, _resources, _frameIndex);

        m_multiFrameTracker.ClearData();

        m_descriptorSetPools.FlushDeferredFrees();
        m_descriptorSetLayouts.FlushDeferredFrees();
    }

    AllocatorInstance VkDescriptorSetManager::GetAllocator() const
    {
        return m_descriptorSetLayouts.GetAllocator();
    }

    void VkDescriptorSetManager::_ProcessUpdates(
        const eastl::vector<WriteOp>& _writes,
        VkDevice _device,
        const VkResources& _resources,
        u8 _frameIndex)
    {
        KE_ZoneScopedFunction("VkDescriptorSetManager::_ProcessUpdates");

        m_tmpWrites.clear();
        m_tmpWrites.reserve(_writes.size());

        m_tmpDescriptorData.clear();

        VkDescriptorSet set = VK_NULL_HANDLE;
        GenPool::Handle lastSet = GenPool::kInvalidHandle;
        for (const auto writeOp: _writes)
        {
            if (lastSet != writeOp.m_descriptorSet.m_handle)
            {
                lastSet = writeOp.m_descriptorSet.m_handle;
                if (m_descriptorSetPools.Get(lastSet) != nullptr)
                {
                    const u64 index = m_frameCount * lastSet.m_index + _frameIndex;
                    set = m_descriptorSets[index];
                }
                else
                {
                    set = VK_NULL_HANDLE;
                }
            }

            if (set == VK_NULL_HANDLE)
            {
                continue;
            }

            const PackedIndex packedIndex = { .m_packed = writeOp.m_index };
            const bool isImageInfo = packedIndex.m_type <= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

            m_tmpWrites.push_back(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                // We save the vector offset as it may grow during the operation
                .pNext = reinterpret_cast<void*>(m_tmpDescriptorData.size()),
                .dstSet = set,
                .dstBinding = packedIndex.m_binding,
                .dstArrayElement = writeOp.m_arrayOffset,
                .descriptorCount = static_cast<u32>(writeOp.m_descriptorData.size()),
                .descriptorType = static_cast<VkDescriptorType>(packedIndex.m_type),
            });

            for (const auto& descriptor : writeOp.m_descriptorData)
            {
                auto& data = m_tmpDescriptorData.emplace_back();

                if (packedIndex.m_type == VK_DESCRIPTOR_TYPE_SAMPLER)
                {
                    const VkSampler* pSampler = _resources.m_samplers.Get(descriptor.m_handle);
                    VERIFY_OR_RETURN_VOID(pSampler != nullptr);
                    data.m_imageInfo.sampler = *pSampler;
                }
                else if (isImageInfo)
                {
                    const VkImageView* pImageView = _resources.m_imageViews.Get(descriptor.m_handle);
                    VERIFY_OR_RETURN_VOID(pImageView != nullptr);
                    data.m_imageInfo.imageView = *pImageView;
                    data.m_imageInfo.imageLayout = VkHelperFunctions::ToVkLayout(descriptor.m_textureLayout);
                }
                else if (packedIndex.m_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || packedIndex.m_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                {
                    const VkResources::BufferSpan* bufferView = _resources.m_bufferViews.Get(descriptor.m_handle);
                    data.m_bufferImageInfo.buffer = bufferView->m_buffer;
                    data.m_bufferImageInfo.offset = bufferView->m_offset;
                    data.m_bufferImageInfo.range = bufferView->m_size;
                }
            }

        }

        // Set the definitive pointers
        for (auto& write: m_tmpWrites)
        {
            const bool isImageInfo = write.descriptorType <= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            const auto offset = reinterpret_cast<size_t>(write.pNext);
            write.pNext = nullptr;
            if (isImageInfo)
            {
                write.pImageInfo = &m_tmpDescriptorData[offset].m_imageInfo;
            }
            else
            {
                write.pBufferInfo = &m_tmpDescriptorData[offset].m_bufferImageInfo;
            }
        }

        vkUpdateDescriptorSets(
            _device,
            m_tmpWrites.size(),
            m_tmpWrites.data(),
            0,
            nullptr);
    }
} // namespace KryneEngine