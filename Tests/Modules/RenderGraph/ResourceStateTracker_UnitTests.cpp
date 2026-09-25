/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2026.
 */

#include <KryneEngine/Core/Graphics/RenderPass.hpp>
#include <KryneEngine/Core/Memory/GenerationalPool.hpp>
#include <KryneEngine/Modules/RenderGraph/Builder.hpp>
#include <KryneEngine/Modules/RenderGraph/Registry.hpp>
#include <KryneEngine/Modules/RenderGraph/Resource.hpp>
#include <KryneEngine/Modules/RenderGraph/Utils/ResourceStateTracker.hpp>
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Modules::RenderGraph::Tests
{
    using namespace KryneEngine::Tests;

    namespace
    {
        // ResourceStateTracker::Process() only needs Registry/Builder-level data - none of these
        // handles are ever dereferenced into a real GPU resource, so any distinct fabricated value
        // is fine for testing.
        TextureHandle FakeTextureHandle(const u32 _index) { return TextureHandle { GenPool::Handle { _index, 0 } }; }
        BufferHandle FakeBufferHandle(const u32 _index) { return BufferHandle { GenPool::Handle { _index, 0 } }; }
        TextureViewHandle FakeTextureViewHandle(const u32 _index) { return TextureViewHandle { GenPool::Handle { _index, 0 } }; }
        RenderTargetViewHandle FakeRtvHandle(const u32 _index) { return RenderTargetViewHandle { GenPool::Handle { _index, 0 } }; }

        // A pass's barrier list can hold entries for several distinct resources (e.g. when a
        // "sink" resource is added only to keep the pass alive through DAG culling); these pick out
        // the one for a specific handle rather than assuming array position.
        const BufferMemoryBarrier* FindBarrier(const eastl::span<BufferMemoryBarrier>& _barriers, const BufferHandle& _handle)
        {
            for (const auto& barrier : _barriers)
            {
                if (barrier.m_buffer.m_handle == _handle.m_handle)
                {
                    return &barrier;
                }
            }
            return nullptr;
        }

        const TextureMemoryBarrier* FindBarrier(const eastl::span<TextureMemoryBarrier>& _barriers, const TextureHandle& _handle)
        {
            for (const auto& barrier : _barriers)
            {
                if (barrier.m_texture.m_handle == _handle.m_handle)
                {
                    return &barrier;
                }
            }
            return nullptr;
        }
    }

    TEST(ResourceStateTracker, BufferReadAfterWrite)
    {
        Registry registry;
        Builder builder(registry);

        const BufferHandle bufferHandle = FakeBufferHandle(1);
        const SimplePoolHandle buffer = registry.RegisterRawBuffer(bufferHandle, "TestBuffer");

        builder.DeclarePass(PassType::Transfer)
            .SetName("Write")
            .WriteDependency({
                .m_resource = buffer,
                .m_targetSyncStage = BarrierSyncStageFlags::Transfer,
                .m_targetAccessFlags = BarrierAccessFlags::TransferDst,
            })
            .Done();

        // A pass that only reads never gets a direct "alive" mark from DAG culling (only writes to
        // a declared target resource do, and culling only propagates alive-ness to parents, not
        // children) - give it a trivial write so it survives culling, matching every other test
        // below.
        const SimplePoolHandle sink = registry.RegisterRawBuffer(FakeBufferHandle(2), "Sink");

        builder.DeclarePass(PassType::Compute)
            .SetName("Read")
            .ReadDependency({
                .m_resource = buffer,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
            })
            .WriteDependency({
                .m_resource = sink,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
            })
            .Done();

        builder.DeclareTargetResource(buffer);
        builder.DeclareTargetResource(sink);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        const auto writeBarriers = tracker.GetPassBarriers(0);
        ASSERT_EQ(writeBarriers.m_bufferMemoryBarriers.size(), 1u);
        EXPECT_EQ(writeBarriers.m_bufferMemoryBarriers[0].m_stagesDst, BarrierSyncStageFlags::Transfer);
        EXPECT_EQ(writeBarriers.m_bufferMemoryBarriers[0].m_accessDst, BarrierAccessFlags::TransferDst);

        const auto readBarriers = tracker.GetPassBarriers(1);
        ASSERT_EQ(readBarriers.m_bufferMemoryBarriers.size(), 2u); // one for `buffer`, one for the keep-alive `sink`
        const BufferMemoryBarrier* readBarrier = FindBarrier(readBarriers.m_bufferMemoryBarriers, bufferHandle);
        ASSERT_NE(readBarrier, nullptr);
        EXPECT_EQ(readBarrier->m_stagesSrc, BarrierSyncStageFlags::Transfer);
        EXPECT_EQ(readBarrier->m_accessSrc, BarrierAccessFlags::TransferDst);
        EXPECT_EQ(readBarrier->m_stagesDst, BarrierSyncStageFlags::ComputeShading);
        EXPECT_EQ(readBarrier->m_accessDst, BarrierAccessFlags::ShaderResource);
    }

    TEST(ResourceStateTracker, ConfiguredWholeTextureReadAfterWrite)
    {
        Registry registry;
        Builder builder(registry);

        // A texture with a real (non-sentinel) array size/mip count, always addressed as a whole:
        // this exercises the uniform fast path, and ResolveRange turning a raw-texture dependency's
        // implicit "whole resource" sentinel range into this texture's real, single-subresource extent.
        const TextureHandle textureHandle = FakeTextureHandle(1);
        const SimplePoolHandle texture = registry.RegisterRawTexture(textureHandle, 1, 1, "TestTexture");

        builder.DeclarePass(PassType::Compute)
            .SetName("Write")
            .WriteDependency({
                .m_resource = texture,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
                .m_targetLayout = TextureLayout::UnorderedAccess,
            })
            .Done();

        const SimplePoolHandle sink = registry.RegisterRawBuffer(FakeBufferHandle(2), "Sink");

        builder.DeclarePass(PassType::Compute)
            .SetName("Read")
            .ReadDependency({
                .m_resource = texture,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
            })
            .WriteDependency({
                .m_resource = sink,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
            })
            .Done();

        builder.DeclareTargetResource(texture);
        builder.DeclareTargetResource(sink);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        const auto readBarriers = tracker.GetPassBarriers(1);
        ASSERT_EQ(readBarriers.m_textureMemoryBarriers.size(), 1u); // sink is a buffer, so it doesn't add a texture barrier here
        const TextureMemoryBarrier* barrier = FindBarrier(readBarriers.m_textureMemoryBarriers, textureHandle);
        ASSERT_NE(barrier, nullptr);
        EXPECT_EQ(barrier->m_layoutSrc, TextureLayout::UnorderedAccess);
        EXPECT_EQ(barrier->m_layoutDst, TextureLayout::ShaderResource);
        EXPECT_EQ(barrier->m_arrayStart, 0u);
        EXPECT_EQ(barrier->m_arrayCount, 1u);
        EXPECT_EQ(barrier->m_mipStart, 0u);
        EXPECT_EQ(barrier->m_mipCount, 1u);
    }

    TEST(ResourceStateTracker, PartialIndexingOnUnconfiguredResourceAsserts)
    {
        ScopedAssertCatcher assertCatcher;

        Registry registry;
        Builder builder(registry);

        // Registered without a real array size or mip count in either dimension: this resource has
        // not opted into partial sub-resource indexing, so slicing it should be caught loudly
        // instead of silently mis-tracked.
        const SimplePoolHandle texture = registry.RegisterRawTexture(
            FakeTextureHandle(1),
            RawTextureData::kNoArrayPartialIndexing,
            RawTextureData::kNoMipPartialIndexing,
            "UnconfiguredTexture");

        const SimplePoolHandle rtv = registry.RegisterRenderTargetView(
            FakeRtvHandle(1),
            texture,
            TextureSubResourceRange { .m_arrayStart = 0, .m_arrayCount = 1 },
            "PartialRTV");

        builder.DeclarePass(PassType::Render)
            .SetName("PartialWrite")
            .SetDepthAttachment(rtv)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .Done()
            .Done();

        builder.DeclareTargetResource(texture);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        assertCatcher.ExpectMessageCount(1);
    }

    TEST(ResourceStateTracker, PartialArrayIndexingMergesIdenticalStatesOnRead)
    {
        ScopedAssertCatcher assertCatcher;

        Registry registry;
        Builder builder(registry);

        constexpr u16 kLayerCount = 4;
        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1), kLayerCount, 1, "ShadowArray");

        SimplePoolHandle rtvs[kLayerCount];
        for (u16 i = 0; i < kLayerCount; ++i)
        {
            rtvs[i] = registry.RegisterRenderTargetView(
                FakeRtvHandle(1 + i),
                texture,
                TextureSubResourceRange { .m_arrayStart = i, .m_arrayCount = 1 },
                "CascadeRTV");

            builder.DeclarePass(PassType::Render)
                .SetName("CascadePass")
                .SetDepthAttachment(rtvs[i])
                    .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                    .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                    .Done()
                .Done();
        }

        const SimplePoolHandle arrayView = registry.RegisterTextureView(
            FakeTextureViewHandle(1),
            texture,
            TextureSubResourceRange {
                .m_arrayStart = 0,
                .m_arrayCount = kLayerCount,
            },
            "ShadowArrayView");

        // A dependency-only sink, purely so the read pass itself survives DAG culling (a pass that
        // never writes to a declared target resource, directly or transitively, gets culled).
        const SimplePoolHandle sink = registry.RegisterRawBuffer(FakeBufferHandle(99), "Sink");

        builder.DeclarePass(PassType::Compute)
            .SetName("ReadCascades")
            .ReadDependency({
                .m_resource = arrayView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
                .m_planes = TexturePlane::Depth,
            })
            .WriteDependency({
                .m_resource = sink,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
            })
            .Done();

        builder.DeclareTargetResource(texture);
        builder.DeclareTargetResource(sink);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
        {
            for (u32 i = 0; i < kLayerCount; ++i)
            {
                EXPECT_EQ(tracker.GetPassBarriers(i).m_textureMemoryBarriers.size(), 0);
                const PassDeclaration& pass = builder.GetPass(i);
                EXPECT_EQ(pass.m_depthAttachment->m_layoutAfter, TextureLayout::ShaderResource);
            }
        }
        else
        {
            // Each cascade pass writes a single, previously-untouched array layer: one barrier each,
            // scoped to just that layer.
            for (u16 i = 0; i < kLayerCount; ++i)
            {
                const auto barriers = tracker.GetPassBarriers(i);
                EXPECT_EQ(barriers.m_textureMemoryBarriers.size(), 1u) << "cascade " << i;
                if (!barriers.m_textureMemoryBarriers.empty())
                {
                    const auto& barrier = barriers.m_textureMemoryBarriers[0];
                    EXPECT_EQ(barrier.m_arrayStart, i) << "cascade " << i;
                    EXPECT_EQ(barrier.m_arrayCount, 1u) << "cascade " << i;
                }
            }

            // The read pass covers all 4 layers, which all ended up in an identical state (every
            // cascade pass writes to the same resulting depth-attachment layout) - they should merge
            // into a single barrier spanning the whole array, not one barrier per layer.
            const auto readBarriers = tracker.GetPassBarriers(kLayerCount);
            EXPECT_EQ(readBarriers.m_textureMemoryBarriers.size(), 1u);
            if (!readBarriers.m_textureMemoryBarriers.empty())
            {
                const auto& merged = readBarriers.m_textureMemoryBarriers[0];
                EXPECT_EQ(merged.m_arrayStart, 0u);
                EXPECT_EQ(merged.m_arrayCount, kLayerCount);
                EXPECT_EQ(merged.m_mipStart, 0u);
                EXPECT_EQ(merged.m_mipCount, 1u);
                EXPECT_EQ(merged.m_layoutSrc, TextureLayout::DepthStencilAttachment);
                EXPECT_EQ(merged.m_layoutDst, TextureLayout::ShaderResource);
            }
        }

        assertCatcher.ExpectNoMessage();
    }

    TEST(ResourceStateTracker, UnconfiguredWholeTextureAttachmentThenRead)
    {
        // Mirrors how "always touched as a whole" textures (swap-chain buffers, G-buffer targets)
        // are registered in the samples: no real array size/mip count, since the resource is never
        // sliced. A "whole resource" sentinel range on such a texture must still resolve to
        // covering it for barrier purposes, even though its real extent isn't tracked.
        //
        // As of this writing this also currently trips ExplodeIfNeeded()'s "doesn't support partial
        // indexing" assert on the read pass (a resolved, already-concrete range of {0,0,...} is
        // re-checked with IsPartial(), which compares against the *unresolved* sentinel constants
        // and misreads "0" as partial) - caught here rather than left to crash the whole binary.
        ScopedAssertCatcher assertCatcher;

        Registry registry;
        Builder builder(registry);

        const SimplePoolHandle texture = registry.RegisterRawTexture(
            FakeTextureHandle(1),
            RawTextureData::kNoArrayPartialIndexing,
            RawTextureData::kNoMipPartialIndexing,
            "SwapchainLikeTexture");
        const SimplePoolHandle rtv = registry.RegisterRenderTargetView(FakeRtvHandle(1), texture, {}, "SwapchainLikeRTV");

        builder.DeclarePass(PassType::Render)
            .SetName("Write")
            .AddColorAttachment(rtv)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .Done()
            .Done();

        const SimplePoolHandle sink = registry.RegisterRawBuffer(FakeBufferHandle(2), "Sink");
        builder.DeclarePass(PassType::Compute)
            .SetName("Read")
            .ReadDependency({
                .m_resource = texture,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
            })
            .WriteDependency({
                .m_resource = sink,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
            })
            .Done();

        builder.DeclareTargetResource(texture);
        builder.DeclareTargetResource(sink);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
        {
            EXPECT_EQ(tracker.GetPassBarriers(0).m_textureMemoryBarriers.size(), 0u);

            const PassDeclaration& pass = builder.GetPass(0);
            EXPECT_EQ(pass.m_colorAttachments[0].m_layoutAfter, TextureLayout::ShaderResource);
        }
        else
        {
            // The read must see a real transition away from the colour-attachment layout the write
            // pass left it in - not silently emit zero barriers because the resource's tracked extent
            // is unconfigured.
            const auto readBarriers = tracker.GetPassBarriers(1);
            ASSERT_EQ(readBarriers.m_textureMemoryBarriers.size(), 1u);
            EXPECT_EQ(readBarriers.m_textureMemoryBarriers[0].m_layoutDst, TextureLayout::ShaderResource);
        }

        assertCatcher.ExpectNoMessage();
    }

    TEST(ResourceStateTracker, PartialAttachmentRewritePreservesRemainderLayout)
    {
        // A shadow-cascade-like scenario: the whole array is first written as a single attachment,
        // then only its first layer is re-written as a second, distinct (read-only) attachment - a
        // partial write relative to the tracked extent. A later pass then reads the untouched
        // remainder (layers 2-3).
        //
        // This exercises the `if (_partial) m_attachmentsToPurge.emplace(...)` branch in
        // ResourceStateTracker::Process, only reachable when
        // GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers() is true: the first
        // attachment (WriteAll)'s m_layoutAfter is a single field, and the backend transitions
        // *everything* WriteAll covers (all 4 layers) into it automatically once, so as soon as a
        // more specific consumer (RewriteCascade0, layer 0 only) claims that field for its own
        // needs, it becomes the real physical layout for the untouched layers 2-3 too - they just
        // can no longer rely on that shared, still-mutable field for their own future transition,
        // since a later consumer might claim it again for something else entirely. Purging must
        // detach them from the attachment once their moment to read the field has passed, baking
        // a concrete state from it so their own later transition goes through an explicit barrier
        // instead of silently reusing (or further clobbering) WriteAll's field.
        ScopedAssertCatcher assertCatcher;

        Registry registry;
        Builder builder(registry);

        constexpr u16 kLayerCount = 4;
        const SimplePoolHandle texture = registry.RegisterRawTexture(FakeTextureHandle(1), kLayerCount, 1, "ShadowArray");

        const SimplePoolHandle rtvAll = registry.RegisterRenderTargetView(
            FakeRtvHandle(1), texture, TextureSubResourceRange { .m_arrayStart = 0, .m_arrayCount = kLayerCount }, "AllRTV");
        builder.DeclarePass(PassType::Render)
            .SetName("WriteAll")
            .SetDepthAttachment(rtvAll)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Clear)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .Done()
            .Done();

        // Re-touches only layer 0, as a *read-only* attachment - deliberately a different
        // resulting layout (DepthStencilReadOnly) than WriteAll's default (DepthStencilAttachment),
        // so whichever one ends up governing the remainder's later transition is distinguishable.
        const SimplePoolHandle rtvPartial = registry.RegisterRenderTargetView(
            FakeRtvHandle(2), texture, TextureSubResourceRange { .m_arrayStart = 0, .m_arrayCount = 1 }, "PartialRTV");
        builder.DeclarePass(PassType::Render)
            .SetName("RewriteCascade0")
            .SetDepthAttachment(rtvPartial)
                .SetLoadOperation(RenderPassDesc::Attachment::LoadOperation::Load)
                .SetStoreOperation(RenderPassDesc::Attachment::StoreOperation::Store)
                .SetReadOnlyDepthStencil(true)
                .Done()
            .Done();

        // Reads layers 2-3, which RewriteCascade0 never touched directly - but which WriteAll's
        // whole-range attachment transition still physically carries along to whatever
        // RewriteCascade0 claimed for it.
        const SimplePoolHandle remainderView = registry.RegisterTextureView(
            FakeTextureViewHandle(1),
            texture,
            TextureSubResourceRange { .m_arrayStart = 2, .m_arrayCount = 2 },
            "RemainderView");
        const SimplePoolHandle sink = registry.RegisterRawBuffer(FakeBufferHandle(99), "Sink");
        builder.DeclarePass(PassType::Compute)
            .SetName("ReadRemainder")
            .ReadDependency({
                .m_resource = remainderView,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::ShaderResource,
                .m_targetLayout = TextureLayout::ShaderResource,
                .m_planes = TexturePlane::Depth,
            })
            .WriteDependency({
                .m_resource = sink,
                .m_targetSyncStage = BarrierSyncStageFlags::ComputeShading,
                .m_targetAccessFlags = BarrierAccessFlags::UnorderedAccess,
            })
            .Done();

        builder.DeclareTargetResource(texture);
        builder.DeclareTargetResource(sink);
        builder.BuildDag();

        ResourceStateTracker tracker;
        tracker.Process(builder, registry);

        const PassDeclaration& writeAll = builder.GetPass(0);
        const PassDeclaration& rewriteCascade0 = builder.GetPass(1);

        if (GraphicsContext::RenderPassesAutomaticallyPlaceAttachmentBarriers())
        {
            // WriteAll -> RewriteCascade0 is handled automatically by the backend (via
            // m_layoutBefore/m_layoutAfter), so neither pass needs an explicit barrier...
            EXPECT_EQ(tracker.GetPassBarriers(0).m_textureMemoryBarriers.size(), 0u);
            EXPECT_EQ(tracker.GetPassBarriers(1).m_textureMemoryBarriers.size(), 0u);

            // ...RewriteCascade0 claims WriteAll's shared m_layoutAfter field for its own
            // (read-only) needs, and both ends of that automatic transition agree on it...
            EXPECT_EQ(rewriteCascade0.m_depthAttachment->m_layoutBefore, TextureLayout::DepthStencilReadOnly);
            EXPECT_EQ(writeAll.m_depthAttachment->m_layoutAfter, TextureLayout::DepthStencilReadOnly);

            // ...and critically, ReadRemainder (layers 2-3, which RewriteCascade0 never touched)
            // does NOT get to silently reuse or further overwrite that same field: it was purged
            // once RewriteCascade0 claimed it, so it must fall back to an explicit barrier,
            // correctly sourced from the layout WriteAll's attachment was just claimed into
            // (DepthStencilReadOnly - the real physical layout the whole range ends up in), not
            // ReadRemainder's own target layout and not left as a missing (zero-count) barrier.
            EXPECT_EQ(writeAll.m_depthAttachment->m_layoutAfter, rewriteCascade0.m_depthAttachment->m_layoutBefore);
            const auto remainderBarriers = tracker.GetPassBarriers(2);
            ASSERT_EQ(remainderBarriers.m_textureMemoryBarriers.size(), 1u);
            const auto& remainderBarrier = remainderBarriers.m_textureMemoryBarriers[0];
            EXPECT_EQ(remainderBarrier.m_arrayStart, 2u);
            EXPECT_EQ(remainderBarrier.m_arrayCount, 2u);
            EXPECT_EQ(remainderBarrier.m_layoutSrc, TextureLayout::DepthStencilReadOnly);
            EXPECT_EQ(remainderBarrier.m_layoutDst, TextureLayout::ShaderResource);
        }
        else
        {
            // No automatic placement: every transition already goes through an explicit barrier,
            // so the purge path never triggers (its guarding `if` never runs) - each barrier's
            // m_layoutSrc should still independently reflect the right predecessor.
            const auto writeAllBarriers = tracker.GetPassBarriers(0);
            ASSERT_EQ(writeAllBarriers.m_textureMemoryBarriers.size(), 1u);
            EXPECT_EQ(writeAllBarriers.m_textureMemoryBarriers[0].m_layoutDst, TextureLayout::DepthStencilAttachment);

            const auto rewriteBarriers = tracker.GetPassBarriers(1);
            ASSERT_EQ(rewriteBarriers.m_textureMemoryBarriers.size(), 1u);
            EXPECT_EQ(rewriteBarriers.m_textureMemoryBarriers[0].m_arrayStart, 0u);
            EXPECT_EQ(rewriteBarriers.m_textureMemoryBarriers[0].m_arrayCount, 1u);
            EXPECT_EQ(rewriteBarriers.m_textureMemoryBarriers[0].m_layoutSrc, TextureLayout::DepthStencilAttachment);
            EXPECT_EQ(rewriteBarriers.m_textureMemoryBarriers[0].m_layoutDst, TextureLayout::DepthStencilReadOnly);

            const auto remainderBarriers = tracker.GetPassBarriers(2);
            ASSERT_EQ(remainderBarriers.m_textureMemoryBarriers.size(), 1u);
            const auto& remainderBarrier = remainderBarriers.m_textureMemoryBarriers[0];
            EXPECT_EQ(remainderBarrier.m_arrayStart, 2u);
            EXPECT_EQ(remainderBarrier.m_arrayCount, 2u);
            EXPECT_EQ(remainderBarrier.m_layoutSrc, TextureLayout::DepthStencilAttachment);
            EXPECT_EQ(remainderBarrier.m_layoutDst, TextureLayout::ShaderResource);
        }

        assertCatcher.ExpectNoMessage();
    }

} // namespace KryneEngine::Modules::RenderGraph::Tests
