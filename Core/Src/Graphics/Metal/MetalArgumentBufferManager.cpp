/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2024.
 */

#include "Graphics/Metal/MetalArgumentBufferManager.hpp"

#include "Graphics/Metal/Helpers/EnumConverters.hpp"
#include "Graphics/Metal/MetalConstants.hpp"
#include "Graphics/Metal/MetalHeaders.hpp"
#include "Graphics/Metal/MetalResources.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"

namespace KryneEngine
{
    union PackedIndex
    {
        static constexpr size_t kTypeBits = 8;

        struct {
            u32 m_type: kTypeBits;
            u32 m_index: 32 - kTypeBits;
        };
        u32 m_packedIndex;
    };

    MetalArgumentBufferManager::MetalArgumentBufferManager(AllocatorInstance _allocator)
        : m_argumentDescriptors(_allocator)
        , m_argumentBufferSets(_allocator)
        , m_pipelineLayouts(_allocator)
    {}

    MetalArgumentBufferManager::~MetalArgumentBufferManager() = default;

    void MetalArgumentBufferManager::Init(u8 _inFlightFrameCount, u8 _frameIndex)
    {
        m_inFlightFrameCount = _inFlightFrameCount;
        m_multiFrameTracker.Init(GetAllocator(), _inFlightFrameCount, _frameIndex);
    }

    void MetalArgumentBufferManager::FlushPools()
    {
        m_argumentDescriptors.FlushDeferredFrees();
        m_argumentBufferSets.FlushDeferredFrees();
        m_pipelineLayouts.FlushDeferredFrees();
    }

    AllocatorInstance MetalArgumentBufferManager::GetAllocator() const
    {
        return m_argumentDescriptors.GetAllocator();
    }

    DescriptorSetLayoutHandle MetalArgumentBufferManager::CreateArgumentDescriptor(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices)
    {
        const GenPool::Handle handle = m_argumentDescriptors.Allocate();
        auto [hot, cold] = m_argumentDescriptors.GetAll(handle);

        hot->m_argDescriptors.SetAllocator(GetAllocator());
        hot->m_argDescriptors.Resize(_desc.m_bindings.size());
        hot->m_argDescriptors.InitAll(nullptr);

        cold->m_shaderVisibility = ShaderVisibility::None;

        u32 flattenedSize = 0;

        for (size_t i = 0; i < hot->m_argDescriptors.Size(); i++)
        {
            NsPtr<MTL::ArgumentDescriptor>& desc = hot->m_argDescriptors[i];
            const DescriptorBindingDesc& binding = _desc.m_bindings[i];
            const u32 bindingIndex = binding.m_bindingIndex == DescriptorBindingDesc::kImplicitBindingIndex ? flattenedSize : binding.m_bindingIndex;

            desc = MTL::ArgumentDescriptor::alloc()->init();
            desc->setDataType(MetalConverters::GetDataType(binding.m_type));
            desc->setAccess(MetalConverters::GetBindingAccess(binding.m_type));
            desc->setArrayLength(binding.m_count);
            desc->setIndex(bindingIndex);
            desc->setTextureType(MetalConverters::GetTextureType(binding.m_textureType));
            _bindingIndices[i] = PackedIndex {
                .m_type = static_cast<u32>(binding.m_type),
                .m_index = static_cast<u32>(bindingIndex),
            }.m_packedIndex;
            flattenedSize = bindingIndex + binding.m_count;

            cold->m_shaderVisibility |= binding.m_visibility;
        }

        return { handle };
    }

    bool MetalArgumentBufferManager::DeleteArgumentDescriptor(DescriptorSetLayoutHandle _argDescriptor)
    {
        ArgumentDescriptorHotData hot;
        if (m_argumentDescriptors.Free(_argDescriptor.m_handle, &hot))
        {
            hot.m_argDescriptors.Clear();
            return true;
        }
        return false;
    }

    DescriptorSetHandle MetalArgumentBufferManager::CreateArgumentBuffer(
        MTL::Device& _device,
        MetalResources& _resources,
        DescriptorSetLayoutHandle _descriptor)
    {
        KE_AUTO_RELEASE_POOL;

        const GenPool::Handle handle = m_argumentBufferSets.Allocate();

        ArgumentBufferHotData* hot = m_argumentBufferSets.Get(handle);

        {
            const ArgumentDescriptorHotData* argDescHot = m_argumentDescriptors.Get(_descriptor.m_handle);

            DynamicArray<NsPtr<MTL::ArgumentDescriptor>> array(GetAllocator(), argDescHot->m_argDescriptors.Size());
            for (auto i = 0u; i < argDescHot->m_argDescriptors.Size(); i++)
            {
                array.Init(i, argDescHot->m_argDescriptors[i]->copy());
            }
            hot->m_encoder = _device.newArgumentEncoder(NS::Array::array(reinterpret_cast<const NS::Object* const*>(array.Data()), array.Size()));
        }

#if defined(TARGET_OS_MAC)
        const MTL::ResourceOptions options = MTL::ResourceStorageModeManaged;
#else
        const MTL::ResourceOptions options = MTL::ResourceStorageModeShared;
#endif
        hot->m_argumentBuffer = _device.newBuffer(hot->m_encoder->encodedLength() * m_inFlightFrameCount, options);

        {
            const auto lock = _resources.m_residencySetLock.AutoLock();
            _resources.m_residencySet->addAllocation(hot->m_argumentBuffer.get());
        }

#if !defined(KE_FINAL)
        eastl::string debugName;
        debugName.sprintf("ArgumentBuffer#%d", handle.m_index);
        hot->m_argumentBuffer->setLabel(NS::String::string(debugName.c_str(), NS::UTF8StringEncoding));
#endif

        return { handle };
    }

    bool MetalArgumentBufferManager::DestroyArgumentBuffer(
        const DescriptorSetHandle _argumentBuffer,
        MetalResources& _resources)
    {
        ArgumentBufferHotData hot;
        if (m_argumentBufferSets.Free(_argumentBuffer.m_handle, &hot))
        {
            {
                const auto lock = _resources.m_residencySetLock.AutoLock();
                _resources.m_residencySet->removeAllocation(hot.m_argumentBuffer.get());
            }
            hot.m_encoder.reset();
            hot.m_argumentBuffer.reset();
            return true;
        }
        return false;
    }

#pragma region Pipeline layout
    PipelineLayoutHandle MetalArgumentBufferManager::CreatePipelineLayout(const PipelineLayoutDesc& _desc)
    {
        KE_ASSERT(_desc.m_descriptorSets.size() < MetalConstants::kMaxArgumentBuffers);
        KE_ASSERT(_desc.m_pushConstants.size() < MetalConstants::kMaxPushConstantBuffers);

        const GenPool::Handle handle = m_pipelineLayouts.Allocate();

        PipelineLayoutHotData* hot = m_pipelineLayouts.Get(handle);

        constexpr ShaderVisibility testedVisibilities[] = {
            ShaderVisibility::Vertex,
            ShaderVisibility::TesselationControl,
            ShaderVisibility::TesselationEvaluation,
            ShaderVisibility::Fragment,
            ShaderVisibility::Compute,
            ShaderVisibility::Mesh,
            ShaderVisibility::Task
        };

        // Reproduce SpirV cross behaviour regarding push constant buffer index determination.
        // If no descriptor set is included in shader, takes buffer index 0.
        // If there's any set, will take the last set index, and add +1.
        // Push constant buffer index can vary between stages.
        hot->m_pushConstantsData.set_overflow_allocator(GetAllocator());
        for (auto pushConstantDesc : _desc.m_pushConstants)
        {
            auto& data = hot->m_pushConstantsData.emplace_back();
            data.m_data.set_overflow_allocator(GetAllocator());
            for (const ShaderVisibility visibility: testedVisibilities)
            {
                if (BitUtils::EnumHasAny(pushConstantDesc.m_visibility, visibility))
                {
                    data.m_data.push_back({
                        .m_visibility = visibility,
                    });
                }
            }
        }

        hot->m_setVisibilities.set_overflow_allocator(GetAllocator());
        for (size_t i = 0; i < _desc.m_descriptorSets.size(); i++)
        {
            const auto& set = _desc.m_descriptorSets[i];
            const ShaderVisibility setVisibility = m_argumentDescriptors.GetCold(set.m_handle)->m_shaderVisibility;
            hot->m_setVisibilities.push_back(setVisibility);

            for (auto& pcData: hot->m_pushConstantsData)
            {
                for (auto& visibilityData: pcData.m_data)
                {
                    if (BitUtils::EnumHasAny(setVisibility, visibilityData.m_visibility))
                    {
                        visibilityData.m_bufferIndex = i + 1;
                    }
                }
            }
        }

        return { handle };
    }

    bool MetalArgumentBufferManager::DestroyPipelineLayout(PipelineLayoutHandle _layout)
    {
        return m_pipelineLayouts.Free(_layout.m_handle);
    }
#pragma endregion

#pragma region Argument buffer update
    void MetalArgumentBufferManager::UpdateArgumentBuffer(
        MetalResources& _resources,
        const eastl::span<const DescriptorSetWriteInfo>& _writes,
        bool _singleFrame,
        DescriptorSetHandle _descriptorSet,
        u8 _frameIndex)
    {
        eastl::fixed_vector<ArgumentBufferWriteInfo, 128> updates;
        updates.set_overflow_allocator(GetAllocator());

        for (const DescriptorSetWriteInfo& writeInfo: _writes)
        {
            PackedIndex packedIndex = { .m_packedIndex = writeInfo.m_index };
            packedIndex.m_index += writeInfo.m_arrayOffset;
            KE_ASSERT(packedIndex.m_index != 0xFFFF);

            for (auto& data: writeInfo.m_descriptorData)
            {
                ArgumentBufferWriteInfo& info = updates.emplace_back();
                info.m_index = packedIndex.m_packedIndex;
                info.m_argumentBuffer = _descriptorSet;
                info.m_object = data.m_handle;

                if (!_singleFrame)
                {
                    m_multiFrameTracker.TrackForOtherFrames(info);
                }

                packedIndex.m_index++;
            }
        }

        _FlushUpdates(_resources, updates, _frameIndex);
    }

    void MetalArgumentBufferManager::UpdateAndFlushArgumentBuffers(MetalResources& _resources, u8 _frameIndex)
    {
        m_multiFrameTracker.AdvanceToNextFrame();

        _FlushUpdates(_resources, m_multiFrameTracker.GetData(), _frameIndex);
        m_multiFrameTracker.ClearData();
    }

    void MetalArgumentBufferManager::_FlushUpdates(
        MetalResources& _resources,
        eastl::span<const ArgumentBufferWriteInfo> _updates,
        u8 _frameIndex)
    {
        GenPool::Handle currentBuffer = GenPool::kInvalidHandle;
        MTL::ArgumentEncoder* encoder = nullptr;
        MTL::Buffer* buffer = nullptr;

        const auto flush = [&]
        {
#if defined(TARGET_OS_MAC)
            // Could be optimized by not flushing entire buffer
            if (buffer != nullptr)
            {
                buffer->didModifyRange({encoder->encodedLength() * _frameIndex, encoder->encodedLength()});
            }
#endif
        };

        for (const ArgumentBufferWriteInfo& update: _updates)
        {
            if (update.m_argumentBuffer != currentBuffer)
            {
                flush();
                currentBuffer = update.m_argumentBuffer.m_handle;
                const ArgumentBufferHotData* hot = m_argumentBufferSets.Get(currentBuffer);
                encoder = hot->m_encoder.get();
                buffer = hot->m_argumentBuffer.get();
                encoder->setArgumentBuffer(buffer, encoder->encodedLength() * _frameIndex);
            }

            PackedIndex index { .m_packedIndex = update.m_index };
            const auto type = static_cast<DescriptorBindingDesc::Type>(index.m_type);

            switch (type)
            {
            case DescriptorBindingDesc::Type::Sampler:
                encoder->setSamplerState(_resources.m_samplers.Get(update.m_object)->m_sampler, index.m_index);
                break;
            case DescriptorBindingDesc::Type::SampledTexture:
            case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
            case DescriptorBindingDesc::Type::StorageReadWriteTexture:
                encoder->setTexture(_resources.m_textureViews.Get(update.m_object)->m_texture, index.m_index);
                break;
            case DescriptorBindingDesc::Type::ConstantBuffer:
            case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
            case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
            {
                MetalResources::BufferViewHotData* bufferView = _resources.m_bufferViews.Get(update.m_object);
                encoder->setBuffer(bufferView->m_buffer, bufferView->m_offset, index.m_index);
                break;
            }
            }
        }

        flush();
    }
#pragma endregion
} // namespace KryneEngine