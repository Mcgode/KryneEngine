/**
 * @file
 * @author Max Godefroy
 * @date 10/03/2025.
 */

#include "KryneEngine/Modules/RenderGraph/Utils/ResourceStateTracker.hpp"

#include <EASTL/algorithm.h>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include "KryneEngine/Modules/RenderGraph/Builder.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    void ResourceStateTracker::Process(Builder& _builder, const Registry& _registry)
    {
        KE_ZoneScoped("Track resource states");

        m_bufferMemoryBarriers.clear();
        m_textureMemoryBarriers.clear();
        m_passBarriers.resize(_builder.m_declaredPasses.size());
        m_trackedBufferStates.clear();
        m_trackedTextureStates.clear();

        // Walks _range one mip level at a time, merging contiguous array layers that share an
        // identical previous barrier-source state, and invokes _emit once per merged run with
        // (arrayStart, arrayCount, mip, previousState). _range must already be resolved (see
        // ResolveRange) - its counts are used directly as loop bounds.
        const auto forEachTransitioningSubResourceRange = [](TextureStates& _states, const TextureSubResourceRange& _range, auto&& _emit)
        {
            if (_states.m_isUniform)
            {
                const bool partial = !_states.CoversWholeExtent(_range);
                if (partial)
                    _states.ExplodeIfNeeded();
                _emit(_range, _states.m_uniformState, partial);
            }
            else
            {
                KE_ASSERT_MSG(
                    _states.m_arraySize != RawTextureData::kNoArrayPartialIndexing
                    || (_range.m_arrayStart == 0 && _range.m_arrayCount == TextureSubResourceRange::kAllArrayLayers),
                    "Resource prohibits partial array slice indexing, array range should always be [0, kAllArrayLayers)");
                KE_ASSERT_MSG(
                    _states.m_mipCount != RawTextureData::kNoMipPartialIndexing
                    || (_range.m_mipStart == 0 && _range.m_mipCount == TextureSubResourceRange::kAllMipLevels),
                    "Resource prohibits partial mip level indexing, mip level range should always be [0, kAllMipLevels)");

                const u32 mipStart = _states.m_mipCount == RawTextureData::kNoMipPartialIndexing ? 0u : _range.m_mipStart;
                const u32 mipEnd = _states.m_mipCount == RawTextureData::kNoMipPartialIndexing
                    ? 1u
                    : eastl::min<u32>(mipStart + _range.m_mipCount, _states.m_mipCount);
                const u32 arrayStart = _states.m_arraySize == RawTextureData::kNoArrayPartialIndexing ? 0u : _range.m_arrayStart;
                const u32 arrayEnd = _states.m_arraySize == RawTextureData::kNoArrayPartialIndexing
                    ? 1u
                    : eastl::min<u32>(arrayStart + _range.m_arrayCount, _states.m_arraySize);

                u32 firstMip = mipStart;
                const TextureState* current = nullptr;
                for (u32 mip = mipStart; mip < mipEnd; ++mip)
                {
                    const u8 mipCount = _states.m_mipCount == RawTextureData::kNoMipPartialIndexing ? 0xff : 1;

                    u32 firstSlice = arrayStart;
                    for (u32 slice = arrayStart; slice < arrayEnd; ++slice)
                    {
                        const u32 index = _states.Index(slice, mip);
                        if (current == nullptr)
                        {
                            current = &_states.m_perSubResourceStates[index];
                        }
                        else if (!SameBarrierSource(_states.m_perSubResourceStates[index], *current))
                        {
                            if (firstMip != mip)
                            {
                                _emit(
                                    {
                                        .m_arrayStart = _range.m_arrayStart,
                                        .m_arrayCount = _range.m_arrayCount,
                                        .m_mipStart = static_cast<u8>(firstMip),
                                        .m_mipCount = static_cast<u8>(mip - firstMip),
                                    },
                                    *current,
                                    _range.IsPartial());
                                firstMip = mip;
                            }
                            if (firstSlice != slice)
                            {
                                _emit(
                                    {
                                        .m_arrayStart = static_cast<u16>(firstSlice),
                                        .m_arrayCount = static_cast<u16>(slice - firstSlice),
                                        .m_mipStart = static_cast<u8>(mip),
                                        .m_mipCount = mipCount,
                                    },
                                    *current,
                                    _range.IsPartial());
                                firstSlice = slice;
                            }

                            current = &_states.m_perSubResourceStates[index];
                        }
                    }
                    if (firstSlice != arrayStart)
                    {
                        _emit(
                            {
                                .m_arrayStart = static_cast<u16>(firstSlice),
                                .m_arrayCount = static_cast<u16>(arrayEnd - firstSlice),
                                .m_mipStart = static_cast<u8>(mip),
                                .m_mipCount = mipCount,
                            },
                            *current,
                            _range.IsPartial());
                        firstMip = mip + 1;
                    }
                }
                if (firstMip != mipEnd)
                {
                    _emit(
                       {
                           .m_arrayStart = _range.m_arrayStart,
                           .m_arrayCount = _range.m_arrayCount,
                           .m_mipStart = static_cast<u8>(firstMip),
                           .m_mipCount = static_cast<u8>(mipEnd),
                       },
                       *current,
                       _range.IsPartial());
                }
            }
        };

        for (size_t i = 0; i < _builder.m_declaredPasses.size(); i++)
        {
            if (!_builder.m_passAlive[i])
            {
                continue;
            }

            PassDeclaration& pass = _builder.m_declaredPasses[i];
            constexpr ResourceState defaultState {};

            KE_ZoneScopedF("Parsing pass '%s'", pass.m_name.m_string.c_str());

            const size_t bufferSpanBegin = m_bufferMemoryBarriers.size();
            const size_t textureSpanBegin = m_textureMemoryBarriers.size();

            const auto parseDependencies = [&](const eastl::span<Dependency>& _dependencies)
            {
                for (const auto& dependency : _dependencies)
                {
                    m_attachmentsToPurge.clear();

                    const SimplePoolHandle underlyingResourceHandle = _registry.GetUnderlyingResource(dependency.m_resource);
                    const Resource& resource = _registry.GetResource(dependency.m_resource);

                    KE_ZoneScopedF("Parsing dependency '%s'", resource.m_name.c_str());

                    const Resource& underlyingResource = _registry.GetResource(underlyingResourceHandle);

                    if (resource.IsBuffer())
                    {
                        const auto it = m_trackedBufferStates.find(underlyingResourceHandle);
                        const ResourceState& previousState = it != m_trackedBufferStates.end() ? it->second : defaultState;

                        m_bufferMemoryBarriers.emplace_back(BufferMemoryBarrier {
                            .m_stagesSrc = previousState.m_syncStage,
                            .m_stagesDst = dependency.m_targetSyncStage,
                            .m_accessSrc = previousState.m_accessFlags,
                            .m_accessDst = dependency.m_targetAccessFlags,
                            .m_buffer = underlyingResource.m_bufferData.m_buffer,
                        });

                        m_trackedBufferStates[underlyingResourceHandle] = {
                            ResourceState {
                                .m_syncStage = dependency.m_targetSyncStage,
                                .m_accessFlags = dependency.m_targetAccessFlags,
                            }
                        };
                        continue;
                    }

                    if (!resource.IsTexture())
                    {
                        continue;
                    }

                    TextureStates& states = GetOrCreateTextureStates(underlyingResourceHandle, underlyingResource);
                    const TextureSubResourceRange range = ResolveRange(resource.GetTextureSubResourceRange(), states);

                    const TextureState newState {
                        {
                            .m_syncStage = dependency.m_targetSyncStage,
                            .m_accessFlags = dependency.m_targetAccessFlags,
                            .m_layout = dependency.m_targetLayout,
                        },
                    };

                    forEachTransitioningSubResourceRange(
                        states,
                        range,
                        [&](const TextureSubResourceRange _range, const TextureState& _previous, const bool _partial)
                    {
                        if (_previous.m_attachment != nullptr)
                        {
                            const BarrierSyncStageFlags stageSrc = _previous.m_depthPass
                                        ? BarrierSyncStageFlags::DepthStencilTesting
                                        : BarrierSyncStageFlags::ColorBlending;

                            auto accessSrc = BarrierAccessFlags::ColorAttachment;
                            if (_previous.m_depthPass)
                            {
                                accessSrc = _previous.m_attachment->m_readOnly
                                    ? BarrierAccessFlags::DepthStencilRead
                                    : BarrierAccessFlags::DepthStencilWrite;
                            }

                            if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
                            {
                                _previous.m_attachment->m_layoutAfter = dependency.m_targetLayout;

                                // When using partial indexing, purge the attachment to update the state as the
                                // attachment layout after can only be overridden once, subsequent state changes
                                // must use barriers
                                if (_partial)
                                {
                                    m_attachmentsToPurge.emplace(_previous.m_attachment);
                                }
                            }
                            else
                            {
                                m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                                        .m_stagesSrc = stageSrc,
                                        .m_stagesDst = dependency.m_targetSyncStage,
                                        .m_accessSrc = accessSrc,
                                        .m_accessDst = dependency.m_targetAccessFlags,
                                        .m_texture = underlyingResource.m_rawTextureData.m_texture,
                                        .m_arrayStart = _range.m_arrayStart,
                                        .m_arrayCount = _range.m_arrayCount == 0 ? static_cast<u16>(0xffff) : _range.m_arrayCount,
                                        .m_layoutSrc = _previous.m_attachment->m_layoutAfter,
                                        .m_layoutDst = dependency.m_targetLayout,
                                        .m_mipStart = _range.m_mipStart,
                                        .m_mipCount = _range.m_mipCount == 0 ? static_cast<u8>(0xff) : _range.m_mipCount,
                                        .m_planes = dependency.m_planes,
                                    });
                            }
                        }
                        else
                        {
                            m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                                    .m_stagesSrc = _previous.m_syncStage,
                                    .m_stagesDst = dependency.m_targetSyncStage,
                                    .m_accessSrc = _previous.m_accessFlags,
                                    .m_accessDst = dependency.m_targetAccessFlags,
                                    .m_texture = underlyingResource.m_rawTextureData.m_texture,
                                    .m_arrayStart = _range.m_arrayStart,
                                    .m_arrayCount = _range.m_arrayCount == 0 ? static_cast<u16>(0xffff) : _range.m_arrayCount,
                                    .m_layoutSrc = _previous.m_layout,
                                    .m_layoutDst = dependency.m_targetLayout,
                                    .m_mipStart = _range.m_mipStart,
                                    .m_mipCount = _range.m_mipCount == 0 ? static_cast<u8>(0xff) : _range.m_mipCount,
                                    .m_planes = dependency.m_planes,
                                });
                        }
                    });
                    SetRangeState(states, range, newState);

                    for (const auto* attachment : m_attachmentsToPurge)
                    {
                        if (attachment != nullptr)
                            states.PurgeAttachment(attachment);
                    }
                }
            };

            parseDependencies(pass.m_readDependencies);
            parseDependencies(pass.m_writeDependencies);

            const auto parseAttachment = [&](PassAttachmentDeclaration& _attachment, const bool _depth)
            {
                m_attachmentsToPurge.clear();

                const SimplePoolHandle underlyingResource = _registry.GetUnderlyingResource(_attachment.m_rtv);
                const Resource& rtvResource = _registry.GetResource(_attachment.m_rtv);

                const Resource& underlyingRes = _registry.GetResource(underlyingResource);

                TextureStates& states = GetOrCreateTextureStates(underlyingResource, underlyingRes);
                const TextureSubResourceRange range = ResolveRange(rtvResource.GetTextureSubResourceRange(), states);

                const TextureState newState { {}, _depth, &_attachment };

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
                _attachment.m_layoutBefore = _depth
                    ? _attachment.m_readOnly
                        ? TextureLayout::DepthStencilReadOnly
                        : TextureLayout::DepthStencilAttachment
                    : TextureLayout::ColorAttachment;

                const BarrierSyncStageFlags stageDst = _depth
                    ? BarrierSyncStageFlags::DepthStencilTesting
                    : BarrierSyncStageFlags::ColorBlending;
                auto accessDst = BarrierAccessFlags::ColorAttachment;
                if (_depth)
                {
                    accessDst = _attachment.m_readOnly
                        ? BarrierAccessFlags::DepthStencilRead
                        : BarrierAccessFlags::DepthStencilWrite;
                }

                // NOTE: if this attachment's range spans sub-resources with heterogeneous previous
                // states (multiple merged runs), m_layoutBefore below reflects the last run
                // processed. This can't currently happen: every attachment declaration maps to a
                // single RTV range, and per-cascade shadow RTVs are always a single array layer.
                forEachTransitioningSubResourceRange(
                    states,
                    range,
                    [&](const TextureSubResourceRange& _range, const TextureState& _previousState, const bool _partial)
                    {
                        if (!WasTouched(_previousState))
                        {
                            if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
                            {
                                _attachment.m_layoutBefore = TextureLayout::Unknown;
                            }
                            else
                            {
                                m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                                    .m_stagesSrc = BarrierSyncStageFlags::All,
                                    .m_stagesDst = stageDst,
                                    .m_accessSrc = BarrierAccessFlags::None,
                                    .m_accessDst = accessDst,
                                    .m_texture = underlyingRes.m_rawTextureData.m_texture,
                                    .m_arrayStart = _range.m_arrayStart,
                                    .m_arrayCount = _range.m_arrayCount,
                                    .m_layoutSrc = TextureLayout::Unknown,
                                    .m_layoutDst = _attachment.m_layoutAfter,
                                    .m_mipStart = _range.m_mipStart,
                                    .m_mipCount = _range.m_mipCount,
                                    .m_planes = _depth ? TexturePlane::Depth | TexturePlane::Stencil : TexturePlane::Color,
                                });
                            }
                        }
                        else if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
                        {
                            if (_previousState.m_attachment != nullptr)
                            {
                                _previousState.m_attachment->m_layoutAfter = _depth
                                    ? _attachment.m_readOnly
                                        ? TextureLayout::DepthStencilReadOnly
                                        : TextureLayout::DepthStencilAttachment
                                    : TextureLayout::ColorAttachment;
                                _attachment.m_layoutBefore = _previousState.m_attachment->m_layoutAfter;

                                if (_partial)
                                    m_attachmentsToPurge.emplace(_previousState.m_attachment);
                            }
                            else
                            {
                                _attachment.m_layoutBefore = _previousState.m_layout;
                            }
                        }
                        else
                        {
                            if (_previousState.m_attachment != nullptr)
                            {
                                const BarrierSyncStageFlags stageSrc = _previousState.m_depthPass
                                    ? BarrierSyncStageFlags::DepthStencilTesting
                                    : BarrierSyncStageFlags::ColorBlending;

                                auto accessSrc = BarrierAccessFlags::ColorAttachment;
                                if (_previousState.m_depthPass)
                                {
                                    accessSrc = _previousState.m_attachment->m_readOnly
                                        ? BarrierAccessFlags::DepthStencilRead
                                        : BarrierAccessFlags::DepthStencilWrite;
                                }

                                m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                                    .m_stagesSrc = stageSrc,
                                    .m_stagesDst = stageDst,
                                    .m_accessSrc = accessSrc,
                                    .m_accessDst = accessDst,
                                    .m_texture = underlyingRes.m_rawTextureData.m_texture,
                                    .m_arrayStart = _range.m_arrayStart,
                                    .m_arrayCount = _range.m_arrayCount,
                                    .m_layoutSrc = _previousState.m_attachment->m_layoutAfter,
                                    .m_layoutDst = _attachment.m_layoutAfter,
                                    .m_mipStart = _range.m_mipStart,
                                    .m_mipCount = _range.m_mipCount,
                                    .m_planes = _depth ? TexturePlane::Depth | TexturePlane::Stencil : TexturePlane::Color,
                                });
                            }
                            else
                            {
                                m_textureMemoryBarriers.emplace_back(TextureMemoryBarrier {
                                    .m_stagesSrc = _previousState.m_syncStage,
                                    .m_stagesDst = stageDst,
                                    .m_accessSrc = _previousState.m_accessFlags,
                                    .m_accessDst = accessDst,
                                    .m_texture = underlyingRes.m_rawTextureData.m_texture,
                                    .m_arrayStart = _range.m_arrayStart,
                                    .m_arrayCount = _range.m_arrayCount,
                                    .m_layoutSrc = _previousState.m_layout,
                                    .m_layoutDst = _attachment.m_layoutAfter,
                                    .m_mipStart = _range.m_mipStart,
                                    .m_mipCount = _range.m_mipCount,
                                    .m_planes = _depth ? TexturePlane::Depth | TexturePlane::Stencil : TexturePlane::Color,
                                });
                            }
                        }
                    });

                SetRangeState(states, range, newState);

                for (const auto* attachment : m_attachmentsToPurge)
                {
                    if (attachment != nullptr)
                        states.PurgeAttachment(attachment);
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

    ResourceStateTracker::PassBarriers ResourceStateTracker::GetPassBarriers(const u32 _passIndex)
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

    size_t ResourceStateTracker::TextureStates::GetSubResourceCount() const
    {
        return eastl::max<size_t>(m_arraySize, 1) * eastl::max<size_t>(m_mipCount, 1);
    }

    size_t ResourceStateTracker::TextureStates::Index(const u16 _arrayLayer, const u8 _mip) const
    {
        if (m_mipCount == RawTextureData::kNoMipPartialIndexing)
        {
            return _arrayLayer;
        }
        return _mip * m_arraySize + _arrayLayer;
    }

    void ResourceStateTracker::TextureStates::ExplodeIfNeeded()
    {
        if (!m_isUniform)
        {
            return;
        }
        KE_ASSERT_MSG(
            m_arraySize != RawTextureData::kNoArrayPartialIndexing || m_mipCount != RawTextureData::kNoMipPartialIndexing,
            "The resource does not support partial indexing");
        m_perSubResourceStates.assign(
            GetSubResourceCount(),
            m_uniformState);
        m_isUniform = false;
    }

    void ResourceStateTracker::TextureStates::PurgeAttachment(const PassAttachmentDeclaration* _attachment)
    {
        if (m_isUniform)
            return;

        const TextureState* current = nullptr;
        for (auto& state: m_perSubResourceStates)
        {
            if (state.m_attachment == _attachment)
            {
                if (current == nullptr)
                {
                    state.m_syncStage = state.m_depthPass
                        ? BarrierSyncStageFlags::DepthStencilTesting
                        : BarrierSyncStageFlags::ColorBlending;

                    state.m_accessFlags = BarrierAccessFlags::ColorAttachment;
                    if (state.m_depthPass)
                    {
                        state.m_accessFlags = state.m_attachment->m_readOnly
                            ? BarrierAccessFlags::DepthStencilRead
                            : BarrierAccessFlags::DepthStencilWrite;
                    }

                    state.m_layout = _attachment->m_layoutAfter;
                }
                else
                {
                    state = *current;
                }
            }
        }
    }

    ResourceStateTracker::TextureStates& ResourceStateTracker::GetOrCreateTextureStates(
        const SimplePoolHandle _handle,
        const Resource& _underlyingTexture)
    {
        const auto result = m_trackedTextureStates.try_emplace(_handle);
        if (result.second)
        {
            result.first->second.m_arraySize = _underlyingTexture.m_rawTextureData.m_arraySize;
            result.first->second.m_mipCount = _underlyingTexture.m_rawTextureData.m_mipCount;
        }
        return result.first->second;
    }

    bool ResourceStateTracker::TextureStates::CoversWholeExtent(const TextureSubResourceRange& _range) const
    {
        return !_range.IsPartial() ||
            (_range.m_arrayStart == 0 && _range.m_arrayCount == m_arraySize
            && _range.m_mipStart == 0 && _range.m_mipCount == m_mipCount);
    }

    void ResourceStateTracker::SetRangeState(
        TextureStates& _states,
        const TextureSubResourceRange& _range,
        const TextureState& _newState)
    {
        const bool coversWholeExtent = _states.CoversWholeExtent(_range);

        if (coversWholeExtent)
        {
            // Collapse back to the cheap uniform path: every sub-resource ends up in the same state.
            _states.m_isUniform = true;
            _states.m_uniformState = _newState;
            _states.m_perSubResourceStates.clear();
            return;
        }

        _states.ExplodeIfNeeded();
        const u16 arrayEnd = _range.m_arrayStart + _range.m_arrayCount;
        const u8 mipEnd = _range.m_mipStart + _range.m_mipCount;
        for (u16 layer = _range.m_arrayStart; layer < arrayEnd; ++layer)
        {
            for (u8 mip = _range.m_mipStart; mip < mipEnd; ++mip)
            {
                _states.m_perSubResourceStates[_states.Index(layer, mip)] = _newState;
            }
        }
    }

    bool ResourceStateTracker::SameBarrierSource(const TextureState& _a, const TextureState& _b)
    {
        if ((_a.m_attachment != nullptr) != (_b.m_attachment != nullptr))
        {
            return false;
        }
        if (_a.m_attachment != nullptr)
        {
            // When render passes place attachment barriers automatically, each attachment's own
            // m_layoutAfter gets individually overridden by the backend when its render pass runs -
            // sub-resources can't be collapsed into a shared update even if they'd end up in the
            // same state, so they must stay distinct per attachment (pointer identity).
            // Otherwise, an explicit TextureMemoryBarrier is built from the tracked state itself, so
            // sub-resources with an equivalent resulting transition can merge into a single barrier
            // (value comparison).
            if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
            {
                return _a.m_attachment == _b.m_attachment;
            }
            return _a.m_depthPass == _b.m_depthPass
                && _a.m_attachment->m_layoutAfter == _b.m_attachment->m_layoutAfter
                && _a.m_attachment->m_readOnly == _b.m_attachment->m_readOnly;
        }
        return _a.m_syncStage == _b.m_syncStage
            && _a.m_accessFlags == _b.m_accessFlags
            && _a.m_layout == _b.m_layout;
    }

    bool ResourceStateTracker::WasTouched(const TextureState& _state)
    {
        constexpr TextureState kNeverTouched {};
        return _state.m_attachment != nullptr
            || _state.m_syncStage != kNeverTouched.m_syncStage
            || _state.m_accessFlags != kNeverTouched.m_accessFlags
            || _state.m_layout != kNeverTouched.m_layout;
    }

    TextureSubResourceRange ResourceStateTracker::ResolveRange(
        const TextureSubResourceRange& _range,
        const TextureStates& _states)
    {
        TextureSubResourceRange resolved = _range;
        if (resolved.m_arrayCount == TextureSubResourceRange::kAllArrayLayers && _states.m_arraySize != RawTextureData::kNoArrayPartialIndexing)
        {
            resolved.m_arrayCount = _states.m_arraySize > resolved.m_arrayStart
                ? static_cast<u16>(_states.m_arraySize - resolved.m_arrayStart)
                : 0;
        }
        if (resolved.m_mipCount == TextureSubResourceRange::kAllMipLevels && _states.m_mipCount != RawTextureData::kNoMipPartialIndexing)
        {
            resolved.m_mipCount = _states.m_mipCount > resolved.m_mipStart
                ? static_cast<u8>(_states.m_mipCount - resolved.m_mipStart)
                : 0;
        }
        return resolved;
    }
} // namespace KryneEngine
