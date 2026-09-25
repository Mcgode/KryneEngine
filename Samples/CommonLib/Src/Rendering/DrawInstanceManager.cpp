/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#include "Rendering/DrawInstanceManager.hpp"


#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <KryneEngine/Core/Memory/SimplePool.inl>


namespace KryneEngine::Samples
{
    DrawInstanceManager::DrawInstanceManager(
        const AllocatorInstance _allocator,
        GraphicsContext& _graphicsContext,
        const u32 _maxInstances)
            : m_allocator(_allocator)
            , m_models(_allocator)
            , m_validModels(_allocator)
            , m_instances(_allocator, _maxInstances)
            , m_instanceData(_allocator)
            , m_instanceDataBuffer(_allocator)
    {
        m_instanceData.reserve(_maxInstances);

        // Instance data buffers
        const size_t instanceBufferSize = m_instanceData.capacity() * sizeof(InstanceData);
        {
            const BufferCreateDesc desc {
                .m_desc = {
                    .m_size = instanceBufferSize,
#if !defined(KE_FINAL)
                    .m_debugName { "Instance data buffer", m_allocator },
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::ReadBuffer | MemoryUsage::TransferDstBuffer,
            };
            m_instanceDataBuffer.Init(&_graphicsContext, desc, _graphicsContext.GetFrameContextCount());
        }

        // Instance data buffer views
        {
            m_instanceDataBufferViews = m_allocator.Allocate<BufferViewHandle>(_graphicsContext.GetFrameContextCount());

            for (size_t i = 0; i < _graphicsContext.GetFrameContextCount(); ++i)
            {
                char name[128];
                snprintf(name, sizeof(name), "Instance data buffer view #%zu", i);

                m_instanceDataBufferViews[i] = _graphicsContext.CreateBufferView({
                    .m_buffer = m_instanceDataBuffer.GetBuffer(i),
                    .m_size = instanceBufferSize,
                    .m_offset = 0,
                    .m_stride = sizeof(InstanceData),
                    .m_accessType = BufferViewAccessType::Read,
#if !defined(KE_FINAL)
                    .m_debugName = name,
#endif
                });
            }
        }
    }

    DrawInstanceManager::~DrawInstanceManager() = default;

    void DrawInstanceManager::UpdateGpuData(
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        void* dstBuffer = m_instanceDataBuffer.Map(&_graphicsContext, _graphicsContext.GetCurrentFrameContextIndex());

        for (size_t i = 0; i < m_instanceData.size(); ++i)
        {
            Instance& instance = m_instances.Get(i);
            if (!instance.m_valid)
                continue;

            if (instance.m_dynamic || instance.m_uploadFrames > 0)
            {
                if (instance.m_uploadFrames > 0)
                    instance.m_uploadFrames--;

                memcpy(static_cast<std::byte*>(dstBuffer) + i * sizeof(InstanceData), &m_instanceData[i], sizeof(InstanceData));
            }
        }

        m_instanceDataBuffer.Unmap(&_graphicsContext);
        m_instanceDataBuffer.PrepareBuffers(
            &_graphicsContext,
            _transferEncoder,
            BarrierAccessFlags::ShaderResource,
            _graphicsContext.GetCurrentFrameContextIndex());
    }

    SimplePoolHandle DrawInstanceManager::RegisterModel(
        const BufferSpan _vertexBuffer,
        const BufferSpan _indexBuffer,
        const MaterialHandle _material,
        const u32 _elementCount,
        const u32 _indexOffset,
        const u32 _vertexOffset)
    {
        const SimplePoolHandle handle = m_models.AllocateAndInit(Model {
            .m_vertexBuffer = _vertexBuffer,
            .m_indexBuffer = _indexBuffer,
            .m_material = _material,
            .m_instanceCount = 0,
            .m_elementCount = _elementCount,
            .m_indexOffset = _indexOffset,
            .m_vertexOffset = _vertexOffset,
        });

        const size_t word = handle / 64;
        if (word >= m_validModels.size())
        {
            m_validModels.resize(word + 1, 0);
        }
        m_validModels[word] |= (1ull << (handle % 64));

        return handle;
    }

    SimplePoolHandle DrawInstanceManager::RegisterInstance(
        const SimplePoolHandle _model,
        const float3 _position,
        const Math::Quaternion _rotation,
        const float3 _scale)
    {
        const SimplePoolHandle handle = m_instances.AllocateAndInit(Instance {
            .m_model = _model,
            .m_valid = true,
            .m_dynamic = true,
            .m_uploadFrames = 0,
        });

        if (handle >= m_instanceData.size())
        {
            m_instanceData.resize(handle + 1);
        }
        m_instanceData[handle] = PackInstanceData(_position, _rotation, _scale);

        m_models.Get(_model).m_instanceCount++;

        return handle;
    }

    void DrawInstanceManager::UnregisterInstance(const SimplePoolHandle _instance)
    {
        Instance& instance = m_instances.Get(_instance);
        VERIFY_OR_RETURN_VOID(instance.m_valid);

        m_models.Get(instance.m_model).m_instanceCount--;
        instance.m_valid = false;
        m_instances.Free(_instance);
    }

    void DrawInstanceManager::SetInstanceTransform(
        const SimplePoolHandle _instance,
        const float3 _position,
        const Math::Quaternion _rotation,
        const float3 _scale)
    {
        KE_ASSERT(m_instances.Get(_instance).m_valid);
        m_instanceData[_instance] = PackInstanceData(_position, _rotation, _scale);
    }

    DrawInstanceManager::InstanceData DrawInstanceManager::PackInstanceData(
        const float3 _position,
        const Math::Quaternion& _rotation,
        const float3 _scale)
    {
        const u64 packedRotation = _rotation.Pack64();
        return InstanceData {
            .m_position = _position,
            .m_packedRotation0 = static_cast<u32>(packedRotation),
            .m_scale = _scale,
            .m_packedRotation1 = static_cast<u32>(packedRotation >> 32),
        };
    }

    DescriptorSetLayoutHandle DrawInstanceManager::GetPassDescriptorSetLayout(GraphicsContext& _graphicsContext)
    {
        if (m_passDescriptorSetLayout == GenPool::kInvalidHandle)
        {
            constexpr DescriptorBindingDesc bindings[2]
            {
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Vertex,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::StorageReadOnlyBuffer,
                    .m_visibility = ShaderVisibility::Vertex
                }
            };

            m_passDescriptorSetLayout = _graphicsContext.CreateDescriptorSetLayout(
                { .m_bindings = bindings },
                &m_passCbBindingIndex);
        }

        return m_passDescriptorSetLayout;
    }

    PassDispatcher* DrawInstanceManager::CreatePassDispatcher(
        GraphicsContext& _graphicsContext,
        const MaterialManager* _materialManager,
        const u8 _passType,
        const eastl::string_view _debugName)
    {
        auto* ptr = m_allocator.Allocate<PassDispatcher>();
        new (ptr) PassDispatcher(this, _materialManager, _graphicsContext, _passType, _debugName);
        return ptr;
    }

    void DrawInstanceManager::DestroyPassDispatcher(PassDispatcher* _passDispatcher) const
    {
        m_allocator.Delete(_passDispatcher);
    }
} // namespace KryneEngine::Samples