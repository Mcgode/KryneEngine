/**
 * @file
 * @author Max Godefroy
 * @date 10/03/2025.
 */

#pragma once

#include "EASTL/vector_set.h"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"
#include <EASTL/hash_map.h>
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <KryneEngine/Core/Memory/SimplePool.hpp>

namespace KryneEngine::Modules::RenderGraph
{
    class Builder;
    class Registry;
    struct PassAttachmentDeclaration;

    class ResourceStateTracker
    {
    public:
        void Process(Builder& _builder, const Registry& _registry);

        struct PassBarriers
        {
            eastl::span<BufferMemoryBarrier> m_bufferMemoryBarriers {};
            eastl::span<TextureMemoryBarrier> m_textureMemoryBarriers {};
        };

        PassBarriers GetPassBarriers(u32 _passIndex);

    private:
        struct ResourceState
        {
            BarrierSyncStageFlags m_syncStage = BarrierSyncStageFlags::All;
            BarrierAccessFlags m_accessFlags = BarrierAccessFlags::All;
            TextureLayout m_layout = TextureLayout::Unknown;
        };

        struct BufferState: ResourceState {};

        struct TextureState: ResourceState
        {
            bool m_depthPass = false;
            PassAttachmentDeclaration* m_attachment = nullptr;
        };

        // Per-sub-resource state for a texture. Most textures are always transitioned as a whole
        // (single mip/layer texture, or a resource that's only ever touched in full), so a single
        // "uniform" state is kept as a fast path; the state is only exploded into one ResourceState
        // per (array layer, mip) the first time the texture is touched through a range that does
        // not cover its whole extent.
        struct TextureStates
        {
            u16 m_arraySize {};
            u8 m_mipCount {};
            bool m_isUniform = true;
            TextureState m_uniformState {};
            eastl::vector<TextureState> m_perSubResourceStates {};

            [[nodiscard]] size_t GetSubResourceCount() const;

            [[nodiscard]] size_t Index(u16 _arrayLayer, u8 _mip) const;

            // Whether _range (already resolved, see ResolveRange) covers this texture's whole
            // tracked extent - either because it's still the raw "whole resource" sentinel range
            // (never resolved against a real extent, e.g. an unconfigured resource), or because it
            // resolved to exactly [0, m_arraySize) x [0, m_mipCount).
            [[nodiscard]] bool CoversWholeExtent(const TextureSubResourceRange& _range) const;

            void ExplodeIfNeeded();

            void PurgeAttachment(const PassAttachmentDeclaration* _attachment);
        };

        struct PassBarriersRaw
        {
            size_t m_bufferMemoryBarriersStart = 0;
            size_t m_bufferMemoryBarriersCount = 0;
            size_t m_textureMemoryBarriersStart = 0;
            size_t m_textureMemoryBarriersCount = 0;
        };

        [[nodiscard]] TextureStates& GetOrCreateTextureStates(
            SimplePoolHandle _handle,
            const Resource& _underlyingTexture);

        // Applies _newState to every sub-resource in _range. If _range covers the whole tracked
        // extent, the state collapses back to the uniform fast path.
        static void SetRangeState(TextureStates& _states, const TextureSubResourceRange& _range, const TextureState& _newState);

        // Whether two previous states would produce an identical barrier source (sync/access/layout),
        // used to merge contiguous sub-resources into a single barrier.
        static bool SameBarrierSource(const TextureState& _a, const TextureState& _b);

        // Whether _state differs from a texture's initial, never-written state. Used in place of an
        // explicit "touched" flag: a sub-resource that has never been assigned a real state still
        // holds TextureState {}'s default field values.
        static bool WasTouched(const TextureState& _state);

        // Turns a range's kAllArrayLayers/kAllMipLevels sentinels into concrete remaining counts,
        // using _states' tracked real extent. Every range must be resolved through this before it's
        // used in any array/mip arithmetic (loop bounds, "does this cover the whole resource" checks):
        // an unresolved sentinel is u16/u8-max, not "the rest of the resource".
        static TextureSubResourceRange ResolveRange(const TextureSubResourceRange& _range, const TextureStates& _states);

        eastl::vector<BufferMemoryBarrier> m_bufferMemoryBarriers;
        eastl::vector<TextureMemoryBarrier> m_textureMemoryBarriers;
        eastl::vector<PassBarriersRaw> m_passBarriers;

        eastl::vector_set<PassAttachmentDeclaration*> m_attachmentsToPurge;

        eastl::hash_map<SimplePoolHandle, BufferState> m_trackedBufferStates;
        eastl::hash_map<SimplePoolHandle, TextureStates> m_trackedTextureStates;
    };
} // namespace KryneEngine
