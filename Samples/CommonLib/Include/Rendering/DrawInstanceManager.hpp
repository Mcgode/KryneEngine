/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#pragma once


#include <KryneEngine/Core/Graphics/Buffer.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>
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

        // Registers a model (a single draw call's worth of geometry) that instances can be registered against.
        // Models are expected to be long-lived (registered once at scene setup); there is no UnregisterModel.
        [[nodiscard]] SimplePoolHandle RegisterModel(
            BufferSpan _vertexBuffer,
            BufferSpan _indexBuffer,
            MaterialHandle _material,
            u32 _elementCount,
            u32 _indexOffset = 0,
            u32 _vertexOffset = 0);

        // Registers a new dynamic instance of a given model, with an initial transform.
        [[nodiscard]] SimplePoolHandle RegisterInstance(
            SimplePoolHandle _model,
            float3 _position,
            Math::Quaternion _rotation,
            float3 _scale);

        void UnregisterInstance(SimplePoolHandle _instance);

        SimplePoolHandle GetInstanceModel(SimplePoolHandle _instance) const;
        const float3& GetInstancePosition(SimplePoolHandle _instance) const;
        Math::Quaternion GetInstanceRotation(SimplePoolHandle _instance) const;

        void SetInstanceTransform(
            SimplePoolHandle _instance,
            float3 _position,
            Math::Quaternion _rotation,
            float3 _scale);

        [[nodiscard]] DescriptorSetLayoutHandle GetPassDescriptorSetLayout(GraphicsContext& _graphicsContext);

        [[nodiscard]] PassDispatcher* CreatePassDispatcher(
            GraphicsContext& _graphicsContext,
            const MaterialManager* _materialManager,
            u8 _passType,
            eastl::string_view _debugName);

        void DestroyPassDispatcher(PassDispatcher* _passDispatcher) const;

    private:
        AllocatorInstance m_allocator;

        struct Model
        {
            BufferSpan m_vertexBuffer {};
            BufferSpan m_indexBuffer {};
            MaterialHandle m_material = kInvalidMaterialHandle;
            u32 m_instanceCount = 0;
            u32 m_elementCount = 0;
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
            u32 m_packedRotation0 {};
            float3 m_scale {};
            u32 m_packedRotation1 {};
        };

        SimplePool<Instance> m_instances;
        eastl::vector<InstanceData> m_instanceData;

        static InstanceData PackInstanceData(float3 _position, const Math::Quaternion& _rotation, float3 _scale);

        DescriptorSetLayoutHandle m_passDescriptorSetLayout;
        u32 m_passCbBindingIndex = 0;
        u32 m_instanceDataBindingIndex = 0;

        Modules::GraphicsUtils::DynamicBuffer m_instanceDataBuffer;
        BufferViewHandle* m_instanceDataBufferViews = nullptr;
    };
}
