/**
 * @file
 * @author Max Godefroy
 * @date 02/01/2026.
 */

#include "TextureGenerator.hpp"

#include "KryneEngine/Core/Graphics/Buffer.hpp"

TextureGenerator::TextureGenerator(
    AllocatorInstance _allocator,
    size_t _textureCount)
        : m_textures(_allocator, _textureCount)
        , m_textureViews(_allocator, _textureCount)
        , m_stagingBuffers(_allocator, _textureCount)
{
}

void TextureGenerator::HandleUpload(GraphicsContext& _graphicsContext, CommandListHandle _transfer)
{
    if (m_stagingBuffers.Empty() && m_uploadFrame != ~0ull)
        return;

    if (m_uploadFrame != ~0ull)
    {
        if (_graphicsContext.IsFrameExecuted(m_uploadFrame))
        {
            for (const BufferHandle stagingBuffer : m_stagingBuffers)
                _graphicsContext.DestroyBuffer(stagingBuffer);
            m_stagingBuffers.Clear();
        }
        return;
    }

    m_uploadFrame = _graphicsContext.GetFrameId();

    TextureDesc textureDesc {
        .m_dimensions = { kTextureDimensions, kTextureDimensions, 1 },
        .m_format = TextureFormat::RGBA8_UNorm,
    };

    TextureMemoryFootprint footprint = _graphicsContext.FetchTextureSubResourcesMemoryFootprints(textureDesc)[0];

    for (auto i = 0u; i < m_textures.Size(); ++i)
    {
#if !defined(KE_FINAL)
        textureDesc.m_debugName.sprintf("Generated texture %d", i);
#endif

        m_stagingBuffers.Init(i, _graphicsContext.CreateStagingBuffer(textureDesc, { &footprint, 1 }));
        m_textures.Init(i, _graphicsContext.CreateTexture(TextureCreateDesc {
            .m_desc = textureDesc,
            .m_footprintPerSubResource = { &footprint, 1 },
            .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage | MemoryUsage::TransferDstImage,
        }));

        m_textureViews.Init(i, _graphicsContext.CreateTextureView({
            .m_texture = m_textures[i],
            .m_format = TextureFormat::RGBA8_UNorm,
#if !defined(KE_FINAL)
            .m_debugName = textureDesc.m_debugName + " view",
#endif
        }));

        // Create a small CPU-side 16x16 RGBA8 image that is visually identifiable per texture index.
        // Pattern: base color derived from index, diagonal marker, and index encoded in top-left pixels.
        constexpr uint32_t width = kTextureDimensions;
        constexpr uint32_t height = kTextureDimensions;
        constexpr uint32_t channels = 4; // RGBA8
        eastl::array<uint8_t, width * height * channels> pixels {};
        {
            // Derive a base color from the index so each texture looks different.
            auto indexToColor = [&](const u32 _idx) -> eastl::array<u8, 3> {
                // Use some simple hash/mix to get a varied color.
                const auto r = static_cast<u8>((_idx * 37u) & 0xFF);
                const auto g = static_cast<u8>((_idx * 73u + 47u) & 0xFF);
                const auto b = static_cast<u8>((_idx * 191u + 13u) & 0xFF);
                return { r, g, b };
            };

            const auto base = indexToColor(i);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const size_t pxIndex = (y * width + x) * channels;

                    // Start with base color
                    uint8_t r = base[0];
                    uint8_t g = base[1];
                    uint8_t b = base[2];
                    uint8_t a = 255;

                    // Add a contrasting diagonal stripe
                    if ((x + y) % 8 == 0)
                    {
                        r = uint8_t(255 - r);
                        g = uint8_t(255 - g);
                        b = uint8_t(255 - b);
                    }

                    // Draw a small index "barcode" in top-left: up to 8 pixels horizontally encode bits of i
                    if (y < 2 && x < 8)
                    {
                        const uint32_t bit = 1u << x;
                        if ((i & bit) != 0)
                        {
                            // set bright marker for '1'
                            r = 255;
                            g = 255;
                            b = 0;
                        }
                        else
                        {
                            // darker marker for '0'
                            r = 32;
                            g = 32;
                            b = 32;
                        }
                    }

                    pixels[pxIndex + 0] = r;
                    pixels[pxIndex + 1] = g;
                    pixels[pxIndex + 2] = b;
                    pixels[pxIndex + 3] = a;
                }
            }
        }

        // Upload CPU pixels to the staging buffer / texture.
        _graphicsContext.SetTextureData(
            _transfer,
            m_stagingBuffers[i],
            m_textures[i],
            footprint,
            SubResourceIndexing(textureDesc, 0),
            pixels.data());
    }
}

TextureViewHandle TextureGenerator::GetTextureView(const size_t _index) const
{
    return m_textureViews[_index];
}

void TextureGenerator::Destroy(GraphicsContext& _graphicsContext)
{
    for (const TextureViewHandle textureView : m_textureViews)
    {
        _graphicsContext.DestroyTextureView(textureView);
    }
    m_textureViews.Clear();

    for (const TextureHandle texture : m_textures)
    {
        _graphicsContext.DestroyTexture(texture);
    }
    m_textures.Clear();

    for (const BufferHandle stagingBuffer : m_stagingBuffers)
        _graphicsContext.DestroyBuffer(stagingBuffer);
    m_stagingBuffers.Clear();
}
