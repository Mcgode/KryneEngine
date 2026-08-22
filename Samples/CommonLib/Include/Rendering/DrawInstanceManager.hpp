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

#include "Rendering/ModelManager.hpp"


namespace KryneEngine
{
    class GraphicsContext;
}

namespace KryneEngine::Samples
{
    class DrawInstanceManager
    {
    public:
        explicit DrawInstanceManager(AllocatorInstance _allocator, u32 _maxInstances = 131'072);
        ~DrawInstanceManager();

        void UpdateGpuData(GraphicsContext& _graphicsContext, CommandListHandle _commandList);

    private:
        AllocatorInstance m_allocator;

        struct Model
        {
            BufferSpan m_vertexBuffer {};
            BufferSpan m_indexBuffer {};
            ModelHandle m_model = kInvalidModelHandle;
            u32 m_instanceCount = 0;
        };

        SimplePool<Model> m_models;
        eastl::vector<u64> m_validModels;

        struct Instance
        {
            GenPool::Handle m_model {};
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

        Modules::GraphicsUtils::DynamicBuffer m_instanceDataBuffer;
    };
}
