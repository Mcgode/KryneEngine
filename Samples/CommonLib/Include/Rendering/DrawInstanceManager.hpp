/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#pragma once


#include <KryneEngine/Core/Graphics/Buffer.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/SimplePool.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>

#include "Rendering/MaterialManager.hpp"
#include "Rendering/PassDispatcher.hpp"


namespace KryneEngine
{
    class GraphicsContext;
}

namespace KryneEngine::Samples
{
    class DrawInstanceManager
    {
        friend PassDispatcher;

    public:
        explicit DrawInstanceManager(
            AllocatorInstance _allocator,
            GraphicsContext& _graphicsContext,
            u32 _maxInstances = 131'072);
        ~DrawInstanceManager();

        void UpdateGpuData(GraphicsContext& _graphicsContext, TransferCommandEncoderHandle _transferEncoder);

        [[nodiscard]] DescriptorSetLayoutHandle GetPassDescriptorSetLayout(GraphicsContext& _graphicsContext);

    private:
        AllocatorInstance m_allocator;

        struct Model
        {
            BufferSpan m_vertexBuffer {};
            BufferSpan m_indexBuffer {};
            MaterialHandle m_material = kInvalidMaterialHandle;
            u32 m_instanceCount = 0;
            u32 m_vertexCount = 0;
            u32 m_indexOffset = 0;
            u32 m_vertexOffset = 0;
        };

        SimplePool<Model> m_models;
        eastl::vector<u64> m_validModels;

        struct Instance
        {
            SimplePoolHandle m_model {};
            bool m_valid = false;
            bool m_dynamic = false;
            u8 m_uploadFrames = 0;
        };

        struct alignas(sizeof(float4)) InstanceData
        {
            float3 m_position {};
            u32 m_packedRotation[2] {};
            float3 m_scale {};
        };

        SimplePool<Instance> m_instances;
        eastl::vector<InstanceData> m_instanceData;

        DescriptorSetLayoutHandle m_passDescriptorSetLayout;
        u32 m_passCbBindingIndex = 0;
        u32 m_instanceDataBindingIndex = 0;

        Modules::GraphicsUtils::DynamicBuffer m_instanceDataBuffer;
        BufferViewHandle* m_instanceDataBufferViews = nullptr;
    };
}
