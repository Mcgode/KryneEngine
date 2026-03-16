/**
 * @file
 * @author Max Godefroy
 * @date 16/03/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Threads/LightweightMutex.hpp>

namespace KryneEngine
{
    class GraphicsContext;
}

namespace KryneEngine::Modules::GraphicsUtils
{
    class DeferredGraphicResourcesDestructor
    {
        enum class Type: u8
        {
            Buffer,
            Texture,
            Sampler,
            TextureView,
            BufferView,
            RenderTargetView,
            RenderPass,
            ShaderModule,
            DescriptorSetLayout,
            DescriptorSet,
            PipelineLayout,
            GraphicsPipeline,
            ComputePipeline,
        };

    public:
        explicit DeferredGraphicResourcesDestructor(AllocatorInstance _allocator);
        ~DeferredGraphicResourcesDestructor();

        void Flush(GraphicsContext* _graphicsContext);

        void DeferDestruction(const BufferHandle _buffer, const u64 _frameId)
        {
            DeferDestruction(Type::Buffer, _buffer.m_handle, _frameId);
        }

        void DeferDestruction(const TextureHandle _texture, const u64 _frameId)
        {
            DeferDestruction(Type::Texture, _texture.m_handle, _frameId);
        }

        void DeferDestruction(const SamplerHandle _sampler, const u64 _frameId)
        {
            DeferDestruction(Type::Sampler, _sampler.m_handle, _frameId);
        }

        void DeferDestruction(const TextureViewHandle _textureView, const u64 _frameId)
        {
            DeferDestruction(Type::TextureView, _textureView.m_handle, _frameId);
        }

        void DeferDestruction(const BufferViewHandle _bufferView, const u64 _frameId)
        {
            DeferDestruction(Type::BufferView, _bufferView.m_handle, _frameId);
        }

        void DeferDestruction(const RenderTargetViewHandle _renderTargetView, const u64 _frameId)
        {
            DeferDestruction(Type::RenderTargetView, _renderTargetView.m_handle, _frameId);
        }

        void DeferDestruction(const RenderPassHandle _renderPass, const u64 _frameId)
        {
            DeferDestruction(Type::RenderPass, _renderPass.m_handle, _frameId);
        }

        void DeferDestruction(const ShaderModuleHandle _shaderModule, const u64 _frameId)
        {
            DeferDestruction(Type::ShaderModule, _shaderModule.m_handle, _frameId);
        }

        void DeferDestruction(const DescriptorSetLayoutHandle _descriptorSetLayout, const u64 _frameId)
        {
            DeferDestruction(Type::DescriptorSetLayout, _descriptorSetLayout.m_handle, _frameId);
        }

        void DeferDestruction(const DescriptorSetHandle _descriptorSet, const u64 _frameId)
        {
            DeferDestruction(Type::DescriptorSet, _descriptorSet.m_handle, _frameId);
        }

        void DeferDestruction(const PipelineLayoutHandle _pipelineLayout, const u64 _frameId)
        {
            DeferDestruction(Type::PipelineLayout, _pipelineLayout.m_handle, _frameId);
        }

        void DeferDestruction(const GraphicsPipelineHandle _graphicsPipeline, const u64 _frameId)
        {
            DeferDestruction(Type::GraphicsPipeline, _graphicsPipeline.m_handle, _frameId);
        }

        void DeferDestruction(const ComputePipelineHandle _computePipeline, const u64 _frameId)
        {
            DeferDestruction(Type::ComputePipeline, _computePipeline.m_handle, _frameId);
        }

    private:
        struct DeferredDestruction
        {
            Type m_type;
            GenPool::Handle m_handle;
            u64 m_frameId;
        };

        AllocatorInstance m_allocator;
        eastl::vector<DeferredDestruction> m_deferredDestructions;
        LightweightMutex m_mutex;

        void DeferDestruction(Type _type, GenPool::Handle _handle, u64 _frameId);
    };
}
