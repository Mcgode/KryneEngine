/**
 * @file
 * @author Max Godefroy
 * @date 12/11/2026.
 */

#include "KryneEngine/Modules/FileSystem/SequentialFileReader.hpp"

#include <KryneEngine/Core/Common/BitUtils.hpp>
#include <zstd.h>

#include "KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp"

namespace KryneEngine::Modules::FileSystem
{
    SequentialFileReader::SequentialFileReader(const AllocatorInstance _allocator, const ReadOnlyFile& _file)
        : m_allocator(_allocator)
        , m_file(_file)
    {
        if (!_file.IsValid())
            return;

        if (!BitUtils::EnumHasAny(_file.GetFlags(), FileFlags::ZstdCompressed))
        {
            m_size = _file.GetSize();
            m_valid = true;
            return;
        }

        const size_t compressedSize = _file.GetSize();
        if (compressedSize == 0)
            return;
        m_compressedData = { m_allocator.Allocate<std::byte>(compressedSize), compressedSize };
        if (_file.Read(0, m_compressedData) != compressedSize)
            return;

        const u64 contentSize = ZSTD_getFrameContentSize(m_compressedData.data(), compressedSize);
        if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN || contentSize == ZSTD_CONTENTSIZE_ERROR)
            return;
        m_size = contentSize;

        m_zstdDecompressionContext = ZSTD_createDCtx();
        m_valid = m_zstdDecompressionContext != nullptr;
    }

    SequentialFileReader::~SequentialFileReader()
    {
        if (m_zstdDecompressionContext != nullptr)
            ZSTD_freeDCtx(m_zstdDecompressionContext);
        if (!m_compressedData.empty())
            m_allocator.deallocate(m_compressedData.data(), m_compressedData.size());
    }

    bool SequentialFileReader::Read(const eastl::span<std::byte> _destination)
    {
        if (!m_valid || _destination.size() > m_size - m_position)
            return false;

        if (m_zstdDecompressionContext == nullptr)
        {
            if (m_file.Read(m_position, _destination) != _destination.size())
                return false;
        }
        else
        {
            ZSTD_inBuffer input { m_compressedData.data(), m_compressedData.size(), m_compressedPosition };
            ZSTD_outBuffer output { _destination.data(), _destination.size(), 0 };
            while (output.pos < output.size)
            {
                const size_t previousOutput = output.pos;
                const size_t previousInput = input.pos;
                const size_t result = ZSTD_decompressStream(m_zstdDecompressionContext, &output, &input);
                m_compressedPosition = input.pos;

                // No progress means a truncated or corrupted stream
                if (ZSTD_isError(result) || (output.pos == previousOutput && input.pos == previousInput))
                    return false;
            }
        }

        m_position += _destination.size();
        return true;
    }
}
