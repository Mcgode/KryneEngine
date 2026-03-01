/**
 * @file
 * @author Max Godefroy
 * @date 04/01/2026.
 */

#include "KryneEngine/Modules/TextRendering/MsdfAtlasManager.hpp"

#include <cmath>
#include <KryneEngine/Core/Graphics/Buffer.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <KryneEngine/Core/Math/Color.hpp>
#include <KryneEngine/Core/Math/Hashing.hpp>

#include "KryneEngine/Modules/TextRendering/Font.hpp"
#include "KryneEngine/Modules/TextRendering/FontManager.hpp"

namespace KryneEngine::Modules::TextRendering
{
    MsdfAtlasManager::MsdfAtlasManager(
        AllocatorInstance _allocator,
        GraphicsContext* _graphicsContext,
        FontManager* _fontManager,
        u32 _atlasSize,
        u32 _glyphBaseSize)
            : m_allocator(_allocator)
            , m_fontManager(_fontManager)
            , m_atlasAllocator(_allocator, {.m_atlasSize = {_atlasSize, _atlasSize}})
            , m_stagingBuffers(_allocator, _graphicsContext->GetFrameContextCount(), {})
            , m_atlasSize(_atlasSize)
            , m_glyphSlotMap(_allocator)
    {
        const TextureDesc atlasTextureDesc {
            .m_dimensions = { m_atlasSize, m_atlasSize, 1 },
            .m_format = TextureFormat::RGBA8_UNorm,
#if !defined(KE_FINAL)
            .m_debugName = "MSDF font atlas"
#endif
        };

        m_atlasFootprint = _graphicsContext->FetchTextureSubResourcesMemoryFootprints(atlasTextureDesc).front();
        m_atlasTexture = _graphicsContext->CreateTexture({
            .m_desc = atlasTextureDesc,
            .m_footprintPerSubResource = { &m_atlasFootprint, 1 },
            .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage | MemoryUsage::TransferDstImage,
        });

        m_atlasTextureSubresourceIndex = SubResourceIndexing(atlasTextureDesc, 0);

        m_atlasView = _graphicsContext->CreateTextureView({
            .m_texture = m_atlasTexture,
            .m_format = TextureFormat::RGBA8_UNorm,
        });
    }

    MsdfAtlasManager::~MsdfAtlasManager()
    {}

    MsdfAtlasManager::GlyphRegion MsdfAtlasManager::GetGlyphRegion(
        Font* _font,
        const u32 _unicodeCodepoint,
        const u32 _fontSize)
    {
        const u16 pxRange = GetPxRange(_fontSize);
        {
            const auto lock = m_lock.AutoLock();
            const auto it = m_glyphSlotMap.find({ _font, _unicodeCodepoint });
            if (it != m_glyphSlotMap.end())
            {
                KE_ASSERT(it->second.m_fontSize == 0 || it->second.m_fontSize == _fontSize);
                return {
                    .m_x = it->second.m_offsetX,
                    .m_y = it->second.m_offsetY,
                    .m_width = it->second.m_width,
                    .m_height = it->second.m_height,
                    .m_baseline = it->second.m_baseline,
                    .m_pxRange = it->second.m_fontSize == 0 ? static_cast<u16>(0u) : pxRange,
                };
            }
        }

        const GlyphLayoutMetrics glyphMetrics = _font->GetGlyphLayoutMetrics(_unicodeCodepoint, static_cast<float>(_fontSize));
        KE_ASSERT(glyphMetrics.m_advanceX != 0.f);

        // Special characters that have no outline are not rendered
        if (glyphMetrics.m_height == 0 || glyphMetrics.m_width == 0)
        {
            // Save invalid glyph slot for future fetches
            const auto lock = m_lock.AutoLock();
            m_glyphSlotMap.emplace(GlyphKey { _font, _unicodeCodepoint }, GlyphSlot {});
            return {};
        }

        float* buffer = _font->GenerateMsdf(_unicodeCodepoint, static_cast<float>(_fontSize), pxRange, m_allocator);
        KE_ASSERT(buffer != nullptr);

        Rect slotRect {};
        GlyphSlot glyphSlot {};
        {
            // At least 2 px of padding
            constexpr u16 padding = 2;

            const auto lock = m_lock.AutoLock();

            const uint2 glyphSize {
                static_cast<u32>(std::ceil(glyphMetrics.m_width)) + pxRange + padding,
                static_cast<u32>(std::ceil(glyphMetrics.m_bearingY) + std::ceil(glyphMetrics.m_height - glyphMetrics.m_bearingY)) + pxRange + padding,
            };

            const u32 slot = m_atlasAllocator.Allocate(glyphSize);
            slotRect = m_atlasAllocator.GetSlotRect(slot);
            glyphSlot = {
                .m_offsetX = static_cast<u16>(slotRect.m_left + padding / 2),
                .m_offsetY = static_cast<u16>(slotRect.m_top + padding / 2),
                .m_width = static_cast<u16>(glyphSize.x - padding),
                .m_height = static_cast<u16>(glyphSize.y - padding),
                .m_baseline = static_cast<u16>(std::ceil(glyphMetrics.m_bearingY) + static_cast<float>(pxRange) * 0.5f),
                .m_fontSize = static_cast<u16>(_fontSize),
                .m_allocatorSlot = slot,
            };

            m_glyphSlotMap.emplace(GlyphKey { _font, _unicodeCodepoint }, glyphSlot);
        }

        m_loadQueue.enqueue({ glyphSlot, slotRect, buffer });

        return {};
    }

    void MsdfAtlasManager::FlushLoads(GraphicsContext& _graphicsContext, CommandListHandle _transfer)
    {
        eastl::vector<GlyphLoadRequest> requests { m_allocator };
        constexpr size_t kMaxRequestsPerFlush = 32;
        requests.resize(kMaxRequestsPerFlush);

        size_t flushCount = m_loadQueue.try_dequeue_bulk(requests.begin(), kMaxRequestsPerFlush);
        if (flushCount == 0)
            return;

        if (GraphicsContext::SupportsNonGlobalBarriers())
        {
            const TextureMemoryBarrier barrier {
                .m_stagesSrc = BarrierSyncStageFlags::All,
                .m_stagesDst = BarrierSyncStageFlags::AllShading,
                .m_accessSrc = BarrierAccessFlags::ShaderResource,
                .m_accessDst = BarrierAccessFlags::TransferDst,
                .m_texture = m_atlasTexture,
                .m_layoutSrc = TextureLayout::ShaderResource,
                .m_layoutDst = TextureLayout::TransferDst,
            };
            _graphicsContext.PlaceMemoryBarriers(_transfer, {}, {}, { &barrier, 1 });
        }

        // Flush all requests into a single vector
        do
        {
            if (flushCount == kMaxRequestsPerFlush)
            {
                const size_t offset = requests.size();
                requests.resize(requests.size() + kMaxRequestsPerFlush);
                flushCount = m_loadQueue.try_dequeue_bulk(requests.begin() + offset, kMaxRequestsPerFlush);

                if (flushCount == 0)
                    requests.resize(offset);
            }
            else
            {
                requests.resize(requests.size() + flushCount - kMaxRequestsPerFlush);
                flushCount = 0;
            }
        }
        while (flushCount > 0);

        eastl::vector<TextureMemoryFootprint> slotFootprints { m_allocator };
        u64 cumulatedSize = 0;
        for (const auto& request : requests)
        {
            TextureMemoryFootprint& footprint = slotFootprints.emplace_back();
            footprint = _graphicsContext.FetchTextureSubResourcesMemoryFootprints({
                .m_dimensions = {
                    request.m_dstRegion.m_right - request.m_dstRegion.m_left,
                    request.m_dstRegion.m_bottom - request.m_dstRegion.m_top,
                    1
                },
                .m_format = m_atlasFootprint.m_format,
            }).front();
            cumulatedSize += footprint.m_lineByteAlignedSize * footprint.m_height;
        }

        StagingBuffer& stagingBuffer = m_stagingBuffers[_graphicsContext.GetCurrentFrameContextIndex()];

        if (stagingBuffer.m_size < cumulatedSize)
        {
            if (stagingBuffer.m_buffer != GenPool::kInvalidHandle)
                _graphicsContext.DestroyBuffer(stagingBuffer.m_buffer);

            stagingBuffer.m_buffer = _graphicsContext.CreateBuffer({
                .m_desc = {
                    .m_size = cumulatedSize,
#if !defined(KE_FINAL)
                    .m_debugName = eastl::string().sprintf("MSDF font atlas staging buffer %d", _graphicsContext.GetCurrentFrameContextIndex()),
#endif
                },
                .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
            });

            if (GraphicsContext::SupportsNonGlobalBarriers())
            {
                const BufferMemoryBarrier barrier[] = {
                    {
                        .m_stagesSrc = BarrierSyncStageFlags::None,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = stagingBuffer.m_buffer,
                    }
                };
                _graphicsContext.PlaceMemoryBarriers(_transfer, {}, barrier, {});
            }

            stagingBuffer.m_size = cumulatedSize;
        }

        BufferMapping mapping { stagingBuffer.m_buffer, cumulatedSize };
        _graphicsContext.MapBuffer(mapping);
        size_t progress = 0;
        for (u32 i = 0; i < requests.size(); ++i)
        {
            const GlyphLoadRequest& request = requests[i];
            const GlyphSlot& slot = request.m_slot;

            const TextureMemoryFootprint& footprint = slotFootprints[i];
            const u64 size = footprint.m_lineByteAlignedSize * footprint.m_height;

            KE_ASSERT(request.m_buffer != nullptr);

            u32 localProgress = progress;
            for (u32 y = request.m_dstRegion.m_top; y < request.m_dstRegion.m_bottom; ++y)
            {
                s32 ry = static_cast<s32>(y) - static_cast<s32>(slot.m_offsetY);

                auto* pixels = reinterpret_cast<u32*>(mapping.m_ptr + localProgress);
                auto* pixelsEnd = reinterpret_cast<u32*>(mapping.m_ptr + localProgress + footprint.m_lineByteAlignedSize);

                if (ry < 0 || ry >= slot.m_height)
                {
                    memset(pixels, 0, (pixelsEnd - pixels) * sizeof(u32));
                    localProgress += footprint.m_lineByteAlignedSize;
                    continue;
                }

                for (u32 x = request.m_dstRegion.m_left; x < request.m_dstRegion.m_right; ++x)
                {
                    s32 rx = static_cast<s32>(x) - static_cast<s32>(slot.m_offsetX);

                    if (rx < 0 || rx >= slot.m_width)
                    {
                        *pixels = 0;
                    }
                    else
                    {
                        const Color pixelColor {
                            eastl::clamp(request.m_buffer[ry * slot.m_width * 3 + 3 * rx + 0], 0.f, 1.f),
                            eastl::clamp(request.m_buffer[ry * slot.m_width * 3 + 3 * rx + 1], 0.f, 1.f),
                            eastl::clamp(request.m_buffer[ry * slot.m_width * 3 + 3 * rx + 2], 0.f, 1.f),
                            1.f,
                        };
                        *pixels = pixelColor.ToRgba8();
                    }
                    ++pixels;
                }

                if (pixels != pixelsEnd)
                {
                    memset(pixels, 0, (pixelsEnd - pixels) * sizeof(u32));
                }
                localProgress += footprint.m_lineByteAlignedSize;
            }
            m_allocator.deallocate(request.m_buffer);

            const uint3 offset ;
            _graphicsContext.SetTextureRegionData(
                _transfer,
                { .m_size = size, .m_offset = progress, .m_buffer = stagingBuffer.m_buffer },
                m_atlasTexture,
                footprint,
                m_atlasTextureSubresourceIndex,
                { request.m_dstRegion.m_left, request.m_dstRegion.m_top, 0 },
                {
                    request.m_dstRegion.m_right - request.m_dstRegion.m_left,
                    request.m_dstRegion.m_bottom - request.m_dstRegion.m_top,
                    1
                });
            progress += size;
        }
        _graphicsContext.UnmapBuffer(mapping);

        if (GraphicsContext::SupportsNonGlobalBarriers())
        {
            const TextureMemoryBarrier barrier {
                .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                .m_stagesDst = BarrierSyncStageFlags::AllShading,
                .m_accessSrc = BarrierAccessFlags::TransferDst,
                .m_accessDst = BarrierAccessFlags::ShaderResource,
                .m_texture = m_atlasTexture,
                .m_layoutSrc = TextureLayout::TransferDst,
                .m_layoutDst = TextureLayout::ShaderResource,
            };
            _graphicsContext.PlaceMemoryBarriers(_transfer, {}, {}, { &barrier, 1 });
        }
    }

    u16 MsdfAtlasManager::GetPxRange(const u32 _fontSize)
    {
        // Keep the minimal pxRange at 4px and scale it in increments of 2px proportionally to the font size
        return 2u * static_cast<u16>(std::round(eastl::max(32.f, static_cast<float>(_fontSize)) / 16.f));
    }
} // namespace KryneEngine::Modules::TextRendering
