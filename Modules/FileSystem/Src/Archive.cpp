/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#include "KryneEngine/Modules/FileSystem/Archive.hpp"

#include <zstd.h>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"

namespace KryneEngine
{
    Modules::FileSystem::ArchiveMaker::ArchiveMaker(
        std::ofstream& _file,
        const eastl::string_view _mountPoint,
        const u32 _fileCount,
        const size_t _arenaByteSize,
        const AllocatorInstance _allocator)
            : m_file(_file)
            , m_stringBuffer(_allocator)
            , m_entries(_allocator)
    {
        constexpr Archive::Header header { Archive::kMagicNumber, Archive::kVersion };
        m_file.write(reinterpret_cast<const char*>(&header), sizeof(Archive::Header));

        m_entries.reserve(_fileCount);

        // Reserve space for the string based on vague estimation
        constexpr size_t estimatedAverageStringSize = 32;
        m_stringBuffer.reserve(estimatedAverageStringSize * _fileCount);

        m_stringBuffer.insert(m_stringBuffer.end(), _mountPoint.begin(), _mountPoint.end());
        if (m_stringBuffer.back() != '\0')
            m_stringBuffer.push_back('\0');

        const size_t arenaSize = eastl::max(
            _arenaByteSize,
            ZSTD_CStreamInSize() + ZSTD_CStreamOutSize());
        m_arena = { _allocator.Allocate<char>(arenaSize), arenaSize };

        m_zstdCompressionContext = ZSTD_createCCtx();
    }

    Modules::FileSystem::ArchiveMaker::~ArchiveMaker()
    {
        ZSTD_freeCCtx(m_zstdCompressionContext);
        m_entries.get_allocator().deallocate(m_arena.data(), m_arena.size());
    }

    void Modules::FileSystem::ArchiveMaker::AddFile(
        std::ifstream& _file,
        const eastl::string_view _path,
        const FileFlags _flags)
    {
        Archive::Entry& entry = m_entries.emplace_back();

        entry.m_fileNameOffset = m_stringBuffer.size();
        m_stringBuffer.insert(m_stringBuffer.end(), _path.begin(), _path.end());
        if (m_stringBuffer.back() != '\0')
            m_stringBuffer.push_back('\0');

        entry.m_flags = _flags;

        entry.m_offset = m_file.tellp();

        _file.seekg(0, std::ios::end);
        const size_t fileSize = _file.tellg();
        _file.seekg(0, std::ios::beg);
        if (BitUtils::EnumHasAny(_flags, FileFlags::ZstdCompressed))
        {
            const eastl::span inputBuffer { m_arena.data(), ZSTD_CStreamInSize() };
            const eastl::span outputBuffer { m_arena.data() + ZSTD_CStreamInSize(), ZSTD_CStreamOutSize() };

            {
                const size_t result = ZSTD_CCtx_reset(m_zstdCompressionContext, ZSTD_reset_session_only);
                KE_ASSERT(!ZSTD_isError(result));
            }

            {
                const size_t result = ZSTD_CCtx_setParameter(
                    m_zstdCompressionContext,
                    ZSTD_c_compressionLevel,
                    m_compressionLevel);
                KE_ASSERT(!ZSTD_isError(result));
            }

            {
                const size_t result = ZSTD_CCtx_setPledgedSrcSize(m_zstdCompressionContext, fileSize);
                KE_ASSERT(!ZSTD_isError(result));
            }

            bool finishedReading = false;
            do
            {
                const size_t readBytes = _file.read(inputBuffer.data(), static_cast<std::streamsize>(inputBuffer.size())).gcount();

                finishedReading = readBytes == 0;

                ZSTD_inBuffer in {
                    .src = inputBuffer.data(),
                    .size = readBytes,
                    .pos = 0,
                };

                do
                {
                    ZSTD_outBuffer out {
                        .dst = outputBuffer.data(),
                        .size = outputBuffer.size(),
                        .pos = 0,
                    };

                    const size_t result = ZSTD_compressStream2(
                        m_zstdCompressionContext,
                        &out,
                        &in,
                        finishedReading ? ZSTD_e_end : ZSTD_e_continue);
                    KE_ASSERT(!ZSTD_isError(result));

                    m_file.write(outputBuffer.data(), static_cast<std::streamsize>(out.pos));
                }
                while (in.pos < in.size);
            }
            while (!finishedReading);
        }
        else
        {
            size_t readBytes = 0;
            while (readBytes < fileSize)
            {
                const size_t size = eastl::min(fileSize - readBytes, m_arena.size());
                _file.read(m_arena.data(), static_cast<std::streamsize>(size));
                m_file.write(m_arena.data(), static_cast<std::streamsize>(size));
                readBytes += size;
            }
        }

        const size_t position = m_file.tellp();
        entry.m_size = position - entry.m_offset;

        // Pad to alignment
        constexpr char padding[Archive::kAlignment] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        m_file.write(padding, static_cast<std::streamsize>(Alignment::AlignUp(position, Archive::kAlignment) - position));
    }

    void Modules::FileSystem::ArchiveMaker::Finish()
    {
        const Archive::Tail tail {
            .m_tableSize = static_cast<u64>(m_entries.size() * sizeof(Archive::Entry)),
            .m_stringsOffset = static_cast<u64>(m_file.tellp()),
        };

        while (Alignment::AlignUp(m_stringBuffer.size(), Archive::kAlignment) != m_stringBuffer.size())
            m_stringBuffer.push_back('\0');

        m_file.write(m_stringBuffer.data(), static_cast<std::streamsize>(m_stringBuffer.size()));
        m_file.write(reinterpret_cast<const char*>(m_entries.data()), static_cast<std::streamsize>(m_entries.size() * sizeof(Archive::Entry)));
        m_file.write(reinterpret_cast<const char*>(&tail), sizeof(Archive::Tail));
        m_file.close();
    }
} // KryneEngine