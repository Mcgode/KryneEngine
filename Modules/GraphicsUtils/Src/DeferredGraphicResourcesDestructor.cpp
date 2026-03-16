/**
 * @file
 * @author Max Godefroy
 * @date 16/03/2026.
 */

#include "KryneEngine/Modules/GraphicsUtils/DeferredGraphicResourcesDestructor.hpp"

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>

namespace KryneEngine::Modules::GraphicsUtils
{
    DeferredGraphicResourcesDestructor::DeferredGraphicResourcesDestructor(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_deferredDestructions(_allocator)
    {}

    DeferredGraphicResourcesDestructor::~DeferredGraphicResourcesDestructor() = default;

    void DeferredGraphicResourcesDestructor::Flush(GraphicsContext* _graphicsContext)
    {
        const u64 currentFrame = _graphicsContext->GetFrameId();

        const auto lock = m_mutex.AutoLock();

        for (auto it = m_deferredDestructions.begin(); it != m_deferredDestructions.end();)
        {
            if (it->m_frameId <= currentFrame)
            {
                switch (it->m_type)
                {
                case Type::Buffer:
                    _graphicsContext->DestroyBuffer({ it->m_handle });
                    break;
                case Type::Texture:
                    _graphicsContext->DestroyTexture({ it->m_handle });
                    break;
                case Type::Sampler:
                    _graphicsContext->DestroySampler({ it->m_handle });
                    break;
                case Type::TextureView:
                    _graphicsContext->DestroyTextureView({ it->m_handle });
                    break;
                case Type::BufferView:
                    _graphicsContext->DestroyBufferView({ it->m_handle });
                    break;
                case Type::RenderTargetView:
                    _graphicsContext->DestroyRenderTargetView({ it->m_handle });
                    break;
                case Type::RenderPass:
                    _graphicsContext->DestroyRenderPass({ it->m_handle });
                    break;
                case Type::ShaderModule:
                    _graphicsContext->FreeShaderModule({ it->m_handle });
                    break;
                case Type::DescriptorSetLayout:
                    _graphicsContext->DestroyDescriptorSetLayout({ it->m_handle });
                    break;
                case Type::DescriptorSet:
                    _graphicsContext->DestroyDescriptorSet({ it->m_handle });
                    break;
                case Type::PipelineLayout:
                    _graphicsContext->DestroyPipelineLayout({ it->m_handle });
                    break;
                case Type::GraphicsPipeline:
                    _graphicsContext->DestroyGraphicsPipeline({ it->m_handle });
                    break;
                case Type::ComputePipeline:
                    _graphicsContext->DestroyComputePipeline({ it->m_handle });
                    break;
                }

                m_deferredDestructions.erase_unsorted(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void DeferredGraphicResourcesDestructor::DeferDestruction(Type _type, GenPool::Handle _handle, u64 _frameId)
    {
        const auto lock = m_mutex.AutoLock();

        // Insert without ordering, we'll simply iterate over all entries and check frame id later.
        m_deferredDestructions.emplace_back(DeferredDestruction{ _type, _handle, _frameId });
    }
}