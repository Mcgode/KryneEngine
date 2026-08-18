/**
 * @file
 * @author Max Godefroy
 * @date 10/03/2025.
 */

#include "KryneEngine/Modules/RenderGraph/Utils/ResourceStateTracker.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include "KryneEngine/Modules/RenderGraph/Builder.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    void ResourceStateTracker::Process(Builder& _builder, Registry& _registry)
    {
        KE_ZoneScoped("Track resource states");

        m_bufferMemoryBarriers.clear();
        m_textureMemoryBarriers.clear();
        m_passBarriers.resize(_builder.m_declaredPasses.size());
        m_trackedStates.clear();

        for (size_t i = 0; i < _builder.m_declaredPasses.size(); i++)
        {
            if (!_builder.m_passAlive[i])
            {
                continue;
            }

            PassDeclaration& pass = _builder.m_declaredPasses[i];
            constexpr ResourceState defaultState {};

            size_t bufferSpanBegin = m_bufferMemoryBarriers.size();
            size_t textureSpanBegin = m_textureMemoryBarriers.size();

            const auto parseDependencies = [&](const eastl::span<Dependency>& _dependencies)
            {
                for (const auto& dependency : _dependencies)
                {
                    const SimplePoolHandle underlyingResource = _registry.GetUnderlyingResource(dependency.m_resource);
                    const auto it = m_trackedStates.find(underlyingResource);

                    const ResourceState& previousState = it != m_trackedStates.end() ? it->second : defaultState;

                    const Resource& resource = _registry.GetResource(dependency.m_resource);

                    if (previousState.m_attachment != nullptr)
                    {
                        previousState.m_attachment->m_layoutAfter = dependency.m_targetLayout;
                    }
                    else if (resource.IsTexture())
                    {
                        m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                            .m_stagesSrc = previousState.m_syncStage,
                            .m_stagesDst = dependency.m_targetSyncStage,
                            .m_accessSrc = previousState.m_accessFlags,
                            .m_accessDst = dependency.m_targetAccessFlags,
                            .m_texture = _registry.GetResource(underlyingResource).m_rawTextureData.m_texture,
                            .m_layoutSrc = previousState.m_layout,
                            .m_layoutDst = dependency.m_targetLayout,
                            .m_planes = dependency.m_planes,
                        });
                    }
                    else if (resource.IsBuffer())
                    {
                        m_bufferMemoryBarriers.emplace_back(BufferMemoryBarrier {
                            .m_stagesSrc = previousState.m_syncStage,
                            .m_stagesDst = dependency.m_targetSyncStage,
                            .m_accessSrc = previousState.m_accessFlags,
                            .m_accessDst = dependency.m_targetAccessFlags,
                            .m_buffer = _registry.GetResource(underlyingResource)
                                            .m_bufferData.m_buffer,
                        });
                    }

                    m_trackedStates[underlyingResource] = {
                        .m_syncStage = dependency.m_targetSyncStage,
                        .m_accessFlags = dependency.m_targetAccessFlags,
                        .m_layout = dependency.m_targetLayout,
                    };
                }
            };

            parseDependencies(pass.m_readDependencies);
            parseDependencies(pass.m_writeDependencies);

            const auto parseAttachment = [&](PassAttachmentDeclaration& _attachment, const bool _depth)
            {
                const SimplePoolHandle underlyingResource = _registry.GetUnderlyingResource(_attachment.m_rtv);

                const auto it = m_trackedStates.find(underlyingResource);
                const ResourceState newState {
                    .m_depthPass = _depth,
                    .m_attachment = &_attachment,
                };

                // Set default layoutAfter based on attachment type
                _attachment.m_layoutAfter =
                    _depth
                        ? _attachment.m_readOnly
                              ? TextureLayout::DepthStencilReadOnly
                              : TextureLayout::DepthStencilAttachment
                        : _attachment.m_storeOperation == RenderPassDesc::Attachment::StoreOperation::Store
                            ? TextureLayout::Present // If last render pass stores color, it is likely presenting. This
                                                     // approach is not the best, and will likely encounter edge cases.
                            : TextureLayout::ColorAttachment;

                // Update layoutBefore based on previous state
                if (it == m_trackedStates.end())
                {
                    _attachment.m_layoutBefore = TextureLayout::Unknown;
                    m_trackedStates.emplace(underlyingResource, newState);
                }
                else
                {
                    if (it->second.m_attachment != nullptr)
                    {
                        it->second.m_attachment->m_layoutAfter = _depth
                            ? _attachment.m_readOnly
                                ? TextureLayout::DepthStencilReadOnly
                                : TextureLayout::DepthStencilAttachment
                            : TextureLayout::ColorAttachment;
                        _attachment.m_layoutBefore = it->second.m_attachment->m_layoutAfter;
                    }
                    else
                    {
                        _attachment.m_layoutBefore = it->second.m_layout;
                    }
                    it->second = newState;
                }
            };

            for (PassAttachmentDeclaration& attachment : pass.m_colorAttachments)
            {
                parseAttachment(attachment, false);
            }
            if (pass.m_depthAttachment.has_value())
            {
                parseAttachment(*pass.m_depthAttachment, true);
            }

            m_passBarriers[i] = {
                .m_bufferMemoryBarriersStart = bufferSpanBegin,
                .m_bufferMemoryBarriersCount = m_bufferMemoryBarriers.size() - bufferSpanBegin,
                .m_textureMemoryBarriersStart = textureSpanBegin,
                .m_textureMemoryBarriersCount = m_textureMemoryBarriers.size() - textureSpanBegin,
            };
        }
    }

    ResourceStateTracker::PassBarriers ResourceStateTracker::GetPassBarriers(u32 _passIndex)
    {
        const PassBarriersRaw& ranges = m_passBarriers[_passIndex];
        return PassBarriers {
            .m_bufferMemoryBarriers = {
                m_bufferMemoryBarriers.begin() + ranges.m_bufferMemoryBarriersStart,
                ranges.m_bufferMemoryBarriersCount,
            },
            .m_textureMemoryBarriers = {
                m_textureMemoryBarriers.begin() + ranges.m_textureMemoryBarriersStart,
                ranges.m_textureMemoryBarriersCount,
            },
        };
    }
} // namespace KryneEngine