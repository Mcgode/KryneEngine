/**
 * @file
 * @author Max Godefroy
 * @date 23/03/2026.
 */

#include "KryneEngine/Modules/TextRendering/FontFiles/PreBakedFontFile.hpp"

#include <zdict.h>
#include <zstd.h>
#include <KryneEngine/Core/Common/Utils/Alignment.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>

namespace KryneEngine::Modules::TextRendering
{
    PreBakedFontFile::PreBakedFontFile(std::ifstream& _file, const size_t _fileSize, const AllocatorInstance _allocator)
    {
        _file.read(reinterpret_cast<char*>(&m_header), sizeof(Header));

        KE_ASSERT(m_header.m_magicNumber == kMagicNumber);

        if (m_header.m_options.m_compressed)
        {
            // Decompress and retrieve tables
            {
                size_t dstSize = m_header.m_glyphCount * sizeof(GlyphEntry);
                if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Msdf))
                    dstSize += m_header.m_glyphCount * sizeof(MsdfEntry);
                if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Outlines))
                    dstSize += m_header.m_glyphCount * sizeof(OutlineEntry);

                auto* tablesBuffer = _allocator.Allocate<std::byte>(dstSize);

                u32 compressedTablesSize;
                _file.read(reinterpret_cast<char*>(&compressedTablesSize), sizeof(u32));
                const u32 alignedCompressedTablesSize = Alignment::AlignUp<u32>(compressedTablesSize, 4u);

                auto* compressedTables = _allocator.Allocate<std::byte>(compressedTablesSize);
                _file.read(reinterpret_cast<char*>(compressedTables), compressedTablesSize);
                _file.seekg(alignedCompressedTablesSize - compressedTablesSize, std::ios::cur);

                KE_ASSERT(ZSTD_getFrameContentSize(compressedTables, compressedTablesSize) == dstSize);
                ZSTD_decompress(tablesBuffer, dstSize, compressedTables, compressedTablesSize);

                _allocator.deallocate(compressedTables, compressedTablesSize);

                m_glyphs = reinterpret_cast<GlyphEntry*>(tablesBuffer);
                auto currentPtr = reinterpret_cast<std::byte*>(m_glyphs + m_header.m_glyphCount);
                if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Msdf))
                {
                    m_msdfEntries = reinterpret_cast<MsdfEntry*>(currentPtr);
                    currentPtr += m_header.m_glyphCount * sizeof(MsdfEntry);
                }
                if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Outlines))
                    m_outlineEntries = reinterpret_cast<OutlineEntry*>(currentPtr);
            }

            // Retrieve render data compression dictionary
            {
                u32 dictBufferSize;
                _file.read(reinterpret_cast<char*>(&dictBufferSize), sizeof(u32));
                const u32 alignedDictBufferSize = Alignment::AlignUp<u32>(dictBufferSize, 4u);

                m_dictBuffer = {
                    _allocator.Allocate<std::byte>(dictBufferSize),
                    dictBufferSize,
                };

                _file.read(reinterpret_cast<char*>(m_dictBuffer.data()), dictBufferSize);
                _file.seekg(alignedDictBufferSize- dictBufferSize, std::ios::cur);
            }

            // Retrieve render data payload
            {
                const size_t payloadSize = _fileSize - _file.tellg();

                m_data = { _allocator.Allocate<std::byte>(payloadSize), payloadSize };
                _file.read(reinterpret_cast<char*>(m_data.data()), static_cast<std::streamsize>(payloadSize));
            }
        }
        else
        {
            const size_t payloadSize = _fileSize - sizeof(Header);

            _file.seekg(0, std::ios::beg);
            m_data = { _allocator.Allocate<std::byte>(payloadSize), payloadSize };
            _file.read(reinterpret_cast<char*>(m_data.data()), static_cast<std::streamsize>(payloadSize));

            m_glyphs = reinterpret_cast<GlyphEntry*>(m_data.data());
            auto ptr = reinterpret_cast<std::byte*>(m_glyphs + m_header.m_glyphCount);
            if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Msdf))
            {
                m_msdfEntries = reinterpret_cast<MsdfEntry*>(ptr);
                ptr += m_header.m_glyphCount * sizeof(MsdfEntry);
            }
            if (BitUtils::EnumHasAny(m_header.m_options.m_renderInfo, BakedRenderInfo::Outlines))
                m_outlineEntries = reinterpret_cast<OutlineEntry*>(ptr);
        }
    }

    bool PreBakedFontFile::IsPreBakedFontFile(const eastl::span<const std::byte> _data)
    {
        return _data.size() >= sizeof(u64) && *reinterpret_cast<const u64*>(_data.data()) == kMagicNumber;
    }

    eastl::span<std::byte> PreBakedFontFile::Bake(
        const BakedRenderInfo _renderInfo,
        const FontMetrics _fontMetrics,
        const eastl::span<const GlyphBakeInfo> _glyphs,
        const eastl::span<float2> _outlinePoints,
        const eastl::span<OutlineTag> _outlineTags,
        const AllocatorInstance _allocator)
    {
        size_t totalSize = 0;

        totalSize += sizeof(Header);

        totalSize += _glyphs.size() * sizeof(GlyphEntry);

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            totalSize += _glyphs.size() * sizeof(MsdfEntry);
            for (const auto& glyph : _glyphs)
                totalSize += Alignment::AlignUp<size_t>(glyph.m_msdfBitmap.size(), 4ull);
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            totalSize += _glyphs.size() * sizeof(OutlineEntry);

            for (const auto& glyph : _glyphs)
            {
                size_t pointsCount = 0;
                for (const OutlineTag tag: eastl::span(_outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount))
                {
                    switch (tag)
                    {
                    case OutlineTag::NewContour:
                    case OutlineTag::Line:
                        pointsCount += 1;
                        break;
                    case OutlineTag::Conic:
                        pointsCount += 2;
                        break;
                    case OutlineTag::Cubic:
                        pointsCount += 3;
                        break;
                    }
                }
                totalSize += pointsCount * sizeof(float2);
                totalSize += Alignment::AlignUp<size_t>(glyph.m_outlineTagCount * sizeof(OutlineTag), 4ull);
            }
        }

        auto* bytes = _allocator.Allocate<std::byte>(totalSize);
        const eastl::span bytesSpan { bytes, totalSize };

        *reinterpret_cast<Header*>(bytes) = Header {
            .m_magicNumber = kMagicNumber,
            .m_fontMetrics = _fontMetrics,
            .m_options = {
                .m_compressed = false,
                .m_renderInfo = _renderInfo,
            },
            .m_glyphCount = static_cast<u32>(_glyphs.size())
        };
        bytes += sizeof(Header);

        for (const auto& glyph : _glyphs)
        {
            *reinterpret_cast<GlyphEntry*>(bytes) = glyph.m_glyph;
            bytes += sizeof(GlyphEntry);
        }

        MsdfEntry* msdfEntries = nullptr;
        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            msdfEntries = reinterpret_cast<MsdfEntry*>(bytes);
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                msdfEntries[i] = {
                    .m_offset = 0,
                    .m_glyphWidth = _glyphs[i].m_msdfWidth,
                    .m_glyphHeight = _glyphs[i].m_msdfHeight,
                    .m_bakedFontSize = _glyphs[i].m_msdfBakedFontSize,
                };
            }
            bytes += _glyphs.size() * sizeof(MsdfEntry);
        }

        OutlineEntry* outlineEntries = nullptr;
        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            outlineEntries = reinterpret_cast<OutlineEntry*>(bytes);
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                outlineEntries[i] = {
                    .m_pointsOffset = 0,
                    .m_tagsOffset = 0,
                    .m_tagsCount = _glyphs[i].m_outlineTagCount,
                };
            }
            bytes += _glyphs.size() * sizeof(OutlineEntry);
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
        {
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                const auto& glyph = _glyphs[i];
                memcpy(bytes, glyph.m_msdfBitmap.data(), glyph.m_msdfBitmap.size());
                msdfEntries[i].m_offset = bytes - bytesSpan.data();
                bytes += Alignment::AlignUp<size_t>(glyph.m_msdfBitmap.size(), 4ull);
            }
        }

        if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
        {
            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                const auto& glyph = _glyphs[i];
                outlineEntries[i].m_pointsOffset = bytes - bytesSpan.data();

                size_t pointsCount = 0;
                for (const OutlineTag tag: eastl::span(_outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount))
                {
                    switch (tag)
                    {
                    case OutlineTag::NewContour:
                    case OutlineTag::Line:
                        pointsCount += 1;
                        break;
                    case OutlineTag::Conic:
                        pointsCount += 2;
                        break;
                    case OutlineTag::Cubic:
                        pointsCount += 3;
                        break;
                    }
                }
                memcpy(bytes, _outlinePoints.begin() + glyph.m_outlineStartPoint, pointsCount * sizeof(float2));
                bytes += pointsCount * sizeof(float2);

                outlineEntries[i].m_tagsOffset = bytes - bytesSpan.data();
                memcpy(bytes, _outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount * sizeof(OutlineTag));
                bytes += Alignment::AlignUp<size_t>(glyph.m_outlineTagCount * sizeof(OutlineTag), 4ull);
            }
        }

        KE_ASSERT(bytes == bytesSpan.end());

        return bytesSpan;
    }

    eastl::span<std::byte> PreBakedFontFile::BakeCompressed(
        const BakedRenderInfo _renderInfo,
        const FontMetrics _fontMetrics,
        const eastl::span<const GlyphBakeInfo> _glyphs,
        const eastl::span<float2> _outlinePoints,
        const eastl::span<OutlineTag> _outlineTags,
        const AllocatorInstance _allocator)
    {
        struct OutlineBlob
        {
            eastl::span<std::byte> m_blob;
            size_t m_pointCount;
        };
        DynamicArray<OutlineBlob> outlineBlobs(_allocator);

        eastl::vector<std::byte> bigBlob(_allocator);
        eastl::vector<size_t> blobSizes(_allocator);

        {
            KE_ZoneScoped("Prepare dictionary training samples");

            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
            {
                for (const auto& glyph : _glyphs)
                {
                    blobSizes.emplace_back(glyph.m_msdfBitmap.size());
                    bigBlob.insert(bigBlob.end(), glyph.m_msdfBitmap.begin(), glyph.m_msdfBitmap.end());
                }
            }

            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
            {
                outlineBlobs.Resize(_glyphs.size());
                for (size_t i = 0; i < _glyphs.size(); ++i)
                {
                    const auto& glyph = _glyphs[i];
                    auto& outlineBlob = outlineBlobs[i];
                    size_t pointsCount = 0;
                    for (const OutlineTag tag: eastl::span(_outlineTags.begin() + glyph.m_outlineFirstTag, glyph.m_outlineTagCount))
                    {
                        switch (tag)
                        {
                        case OutlineTag::NewContour:
                        case OutlineTag::Line:
                            pointsCount += 1;
                            break;
                        case OutlineTag::Conic:
                            pointsCount += 2;
                            break;
                        case OutlineTag::Cubic:
                            pointsCount += 3;
                        }
                    }
                    outlineBlob.m_pointCount = pointsCount;
                    const size_t blobSize = pointsCount * sizeof(float2) + Alignment::AlignUp<size_t>(glyph.m_outlineTagCount * sizeof(OutlineTag), 4ull);
                    outlineBlob.m_blob = { _allocator.Allocate<std::byte>(blobSize), blobSize };

                    size_t offset = 0;
                    memcpy(outlineBlob.m_blob.data(), _outlinePoints.begin() + glyph.m_outlineStartPoint, pointsCount * sizeof(float2));
                    offset += pointsCount * sizeof(float2);
                    memcpy(
                        outlineBlob.m_blob.data() + offset,
                        _outlineTags.begin() + glyph.m_outlineFirstTag,
                        glyph.m_outlineTagCount * sizeof(OutlineTag));
                    offset += glyph.m_outlineTagCount * sizeof(OutlineTag);
                    memset(outlineBlob.m_blob.data() + offset, 0, blobSize - offset);

                    bigBlob.insert(bigBlob.end(), outlineBlob.m_blob.begin(), outlineBlob.m_blob.end());
                    blobSizes.push_back(blobSize);
                }
            }
        }

        constexpr size_t dictCapacity = 64 << 10;
        void* dictBuffer = nullptr;
        size_t dictSize = 0;
        ZSTD_CDict* dict = nullptr;
        constexpr s32 compressionLevel = 0;
        if (!bigBlob.empty())
        {
            KE_ZoneScoped("Train dictionary");

            dictBuffer = _allocator.allocate(dictCapacity);
            dictSize = ZDICT_trainFromBuffer(dictBuffer, dictCapacity, bigBlob.data(), blobSizes.data(), blobSizes.size());

            auto *tmpBuffer = _allocator.allocate(dictCapacity);
            dictSize = ZDICT_finalizeDictionary(
                tmpBuffer, dictCapacity,
                dictBuffer, dictSize,
                bigBlob.data(), blobSizes.data(), blobSizes.size(),
                {});

            _allocator.deallocate(dictBuffer, dictCapacity);
            dictBuffer = tmpBuffer;

            dict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
        }

        eastl::span<std::byte> tables;
        MsdfEntry* msdfEntries = nullptr;
        OutlineEntry* outlineEntries = nullptr;
        {
            KE_ZoneScoped("Prepare tables buffer");
            size_t size = _glyphs.size() * sizeof(GlyphEntry);
            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
                size += _glyphs.size() * sizeof(MsdfEntry);
            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
                size += _glyphs.size() * sizeof(OutlineEntry);
            tables = { _allocator.Allocate<std::byte>(size), size };

            {
                auto* currentPtr = tables.data() + sizeof(GlyphEntry) * _glyphs.size();
                if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
                {
                    msdfEntries = reinterpret_cast<MsdfEntry*>(currentPtr);
                    currentPtr += _glyphs.size() * sizeof(MsdfEntry);
                }
                if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
                {
                    outlineEntries = reinterpret_cast<OutlineEntry*>(currentPtr);
                    currentPtr += _glyphs.size() * sizeof(OutlineEntry);
                }
            }

            for (auto i = 0u; i < _glyphs.size(); ++i)
            {
                auto& glyph = _glyphs[i];
                auto& entry = reinterpret_cast<GlyphEntry*>(tables.data())[i];
                entry = glyph.m_glyph;
            }
        }

        ZSTD_CCtx* ctx = ZSTD_createCCtx();
        const auto compressBlob = [ctx](const eastl::span<std::byte> _blob, eastl::vector<std::byte>& _vector, ZSTD_CDict* _dict)
        {
            const size_t sizeOffset = _vector.size();
            _vector.resize(_vector.size() + sizeof(u32)); // Preinsert size
            const size_t blobOffset = _vector.size();
            const size_t estimatedBlobSize = ZSTD_compressBound(_blob.size_bytes());
            _vector.resize(_vector.size() + estimatedBlobSize);
            size_t blobSize;
            if (_dict != nullptr)
                blobSize = ZSTD_compress_usingCDict(
                    ctx,
                    _vector.data() + blobOffset,
                    estimatedBlobSize,
                    _blob.data(),
                    _blob.size_bytes(),
                    _dict);
            else
                blobSize = ZSTD_compressCCtx(
                    ctx,
                    _vector.data() + blobOffset,
                    estimatedBlobSize,
                    _blob.data(), _blob.size_bytes(),
                    compressionLevel);
            const auto newSize = Alignment::AlignUp<size_t>(blobOffset + blobSize, 4ull);
            _vector.resize(newSize);
            memset(_vector.data() + blobOffset + blobSize, 0, newSize - blobOffset - blobSize);

            *reinterpret_cast<u32*>(_vector.data() + sizeOffset) = static_cast<u32>(blobSize);
            return sizeOffset;
        };

        eastl::vector<std::byte> growingTail(_allocator);
        {
            KE_ZoneScoped("Compressing data blobs");

            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Msdf))
            {
                KE_ZoneScoped("Compressing MSDF blobs");
                for (auto i = 0u; i < _glyphs.size(); ++i)
                {
                    const auto& glyph = _glyphs[i];
                    auto& entry = msdfEntries[i];
                    entry.m_glyphHeight = glyph.m_msdfHeight;
                    entry.m_glyphWidth = glyph.m_msdfWidth;
                    entry.m_bakedFontSize = glyph.m_msdfBakedFontSize;

                    entry.m_offset = compressBlob(glyph.m_msdfBitmap, growingTail, dict);
                }
            }

            if (BitUtils::EnumHasAny(_renderInfo, BakedRenderInfo::Outlines))
            {
                KE_ZoneScoped("Compressing outline blobs");
                for (auto i = 0u; i < _glyphs.size(); ++i)
                {
                    const auto& glyph = _glyphs[i];
                    auto& entry = outlineEntries[i];
                    entry.m_tagsCount = glyph.m_outlineTagCount;
                    entry.m_pointsOffset = compressBlob(outlineBlobs[i].m_blob, growingTail, dict);
                    entry.m_tagsOffset = outlineBlobs[i].m_pointCount * sizeof(float2);
                }
            }
        }

        eastl::vector<std::byte> growingHead(_allocator);
        {
            KE_ZoneScoped("Setup header");
            growingHead.resize(sizeof(Header));
            *reinterpret_cast<Header*>(growingHead.data()) = {
                .m_magicNumber = kMagicNumber,
                .m_fontMetrics = _fontMetrics,
                .m_options = {
                    .m_compressed = true,
                    .m_renderInfo = _renderInfo,
                },
                .m_glyphCount = static_cast<u32>(_glyphs.size())
            };
        }

        {
            KE_ZoneScoped("Compress tables");
            compressBlob(tables, growingHead, nullptr);
        }

        {
            KE_ZoneScoped("Export to final binary blob");

            const auto alignedDictSize = Alignment::AlignUp<size_t>(dictSize, 4ull);
            const size_t finalSize = growingHead.size() + sizeof(u32) + alignedDictSize + growingTail.size();
            auto* bytes = _allocator.Allocate<std::byte>(finalSize);

            size_t offset = 0;
            memcpy(bytes, growingHead.data(), growingHead.size());
            offset += growingHead.size();
            memcpy(bytes + offset, &dictSize, sizeof(u32));
            offset += sizeof(u32);
            memcpy(bytes + offset, dictBuffer, dictSize);
            memset(bytes + offset + dictSize, 0, alignedDictSize - dictSize);
            offset += alignedDictSize;
            memcpy(bytes + offset, growingTail.data(), growingTail.size());

            return { bytes, finalSize };
        }
    }
}
