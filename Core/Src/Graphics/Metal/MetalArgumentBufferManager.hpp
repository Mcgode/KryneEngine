/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2024.
 */

#pragma once

#include "Graphics/Metal/Helpers/NsPtr.hpp"
#include "KryneEngine/Core/Common/Utils/MultiFrameTracking.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.hpp"

namespace MTL
{
    class ArgumentDescriptor;
    class ArgumentEncoder;
    class Buffer;
    class Device;
}

namespace KryneEngine
{
    class MetalResources;

    class MetalArgumentBufferManager
    {
        friend class MetalGraphicsContext;
        friend class MetalResources;

    public:
        explicit MetalArgumentBufferManager(AllocatorInstance _allocator);
        ~MetalArgumentBufferManager();

        void Init(u8 _inFlightFrameCount, u8 _frameIndex);

        void FlushPools();

    private:
        u8 m_inFlightFrameCount {};

        [[nodiscard]] AllocatorInstance GetAllocator() const;

    public:
        [[nodiscard]] DescriptorSetLayoutHandle CreateArgumentDescriptor(
            const DescriptorSetDesc& _desc,
            u32* _bindingIndices);
        bool DeleteArgumentDescriptor(DescriptorSetLayoutHandle _argDescriptor);

    private:
        struct ArgumentDescriptorHotData
        {
            DynamicArray<NsPtr<MTL::ArgumentDescriptor>> m_argDescriptors;
        };

        struct ArgumentDescriptorColdData
        {
            ShaderVisibility m_shaderVisibility;
        };

        GenerationalPool<ArgumentDescriptorHotData, ArgumentDescriptorColdData> m_argumentDescriptors;

    public:
        [[nodiscard]] DescriptorSetHandle
        CreateArgumentBuffer(MTL::Device& _device, MetalResources& _resources, DescriptorSetLayoutHandle _descriptor);
        bool DestroyArgumentBuffer(DescriptorSetHandle _argumentBuffer, MetalResources& _resources);

    private:
        struct ArgumentBufferHotData
        {
            NsPtr<MTL::ArgumentEncoder> m_encoder;
            NsPtr<MTL::Buffer> m_argumentBuffer;
        };

        GenerationalPool<ArgumentBufferHotData> m_argumentBufferSets;

#pragma region Pipeline layout
    public:
        PipelineLayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& _desc);
        bool DestroyPipelineLayout(PipelineLayoutHandle _layout);

    private:
        struct PushConstantData
        {
            struct VisibilityData
            {
                ShaderVisibility m_visibility;
                u8 m_bufferIndex;
            };
            eastl::fixed_vector<VisibilityData, 1> m_data;
        };

        struct PipelineLayoutHotData
        {
            eastl::fixed_vector<ShaderVisibility, 8> m_setVisibilities;
            eastl::fixed_vector<PushConstantData, 2> m_pushConstantsData;
        };

        GenerationalPool<PipelineLayoutHotData> m_pipelineLayouts;
#pragma endregion

#pragma region Argument buffer update
    public:
        void UpdateArgumentBuffer(
            MetalResources& _resources,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame,
            DescriptorSetHandle _descriptorSet,
            u8 _frameIndex);

        void UpdateAndFlushArgumentBuffers(MetalResources& _resources, u8 _frameIndex);

    private:
        struct ArgumentBufferWriteInfo
        {
            DescriptorSetHandle m_argumentBuffer;
            u32 m_index;
            GenPool::Handle m_object;
        };

        MultiFrameDataTracker<ArgumentBufferWriteInfo> m_multiFrameTracker;

        void _FlushUpdates(
            MetalResources& _resources,
            eastl::span<const ArgumentBufferWriteInfo> _updates,
            u8 _frameIndex);
#pragma endregion
    };
} // namespace KryneEngine
