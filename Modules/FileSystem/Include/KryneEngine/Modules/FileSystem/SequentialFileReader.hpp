/**
 * @file
 * @author Max Godefroy
 * @date 12/11/2026.
 */

#pragma once

#include <EASTL/span.h>
#include <KryneEngine/Core/Common/Utils/Macros.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

struct ZSTD_DCtx_s;

namespace KryneEngine::Modules::FileSystem
{
    class ReadOnlyFile;

    /**
     * @brief Reads the content of a `ReadOnlyFile` front to back, whether it is stored compressed or not.
     *
     * @details
     * A file flagged `FileFlags::ZstdCompressed` is decompressed as a stream, straight into the destination of each
     * read. No decompressed copy of the whole file is made, so the parts of a file can each be read into their final
     * place (a header on the stack, a blob on the heap, a mapped GPU buffer, ...).
     *
     * For such a file, the compressed data is read in one go at construction, and held until destruction, along with
     * the decompression context and its streaming buffers. All of it is allocated with the provided allocator.
     * Only pass a scratch allocator if the reader is destroyed before anything allocated from it afterwards, and
     * never lives across a yield.
     * Sizes and positions are always those of the decompressed content.
     *
     * The file must outlive the reader. A reader is not thread-safe.
     */
    class SequentialFileReader
    {
    public:
        SequentialFileReader(AllocatorInstance _allocator, const ReadOnlyFile& _file);

        ~SequentialFileReader();

        KE_DEFINE_COPY_MOVE_SEMANTICS(SequentialFileReader, delete, delete);

        /// @return false if the file is invalid, or if its compressed stream can't be opened. Every read then fails.
        [[nodiscard]] bool IsValid() const { return m_valid; }

        /// @return The size of the file's content, once decompressed.
        [[nodiscard]] size_t GetSize() const { return m_size; }

        /// @return The offset of the next read, in the decompressed content.
        [[nodiscard]] size_t GetPosition() const { return m_position; }

        /**
         * @brief Fills `_destination` with the next bytes of the file, and moves the position past them.
         *
         * @return false if they can't all be read. The reader should not be used afterwards: its position is
         * unchanged, but part of the compressed stream may have been consumed.
         */
        bool Read(eastl::span<std::byte> _destination);

        template <class T>
        bool ReadT(T* _destination, const size_t _count = 1)
        {
            return Read({ reinterpret_cast<std::byte*>(_destination), _count * sizeof(T) });
        }

    private:
        AllocatorInstance m_allocator;
        const ReadOnlyFile& m_file;
        size_t m_size = 0;
        size_t m_position = 0;
        bool m_valid = false;

        eastl::span<std::byte> m_compressedData {};
        size_t m_compressedPosition = 0;
        eastl::span<std::byte> m_zstdWorkspace {};
        ZSTD_DCtx_s* m_zstdDecompressionContext = nullptr; ///< Lives inside `m_zstdWorkspace`
    };
}
