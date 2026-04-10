/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <fstream>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Math/Hashing.hpp>

#include "KryneEngine/Core/Platform/FileSystem.hpp"
#include "KryneEngine/Modules/FileSystem/Flags.hpp"

struct ZSTD_CCtx_s;

namespace KryneEngine::Modules::FileSystem
{
    class ReadOnlyFile;

    class Archive
    {
        friend class ArchiveMaker;

    public:
        struct Header
        {
            u64 m_magicNumber;
            Version m_version;
        };

        struct Tail
        {
            u64 m_tableSize;
            u64 m_stringsOffset;
        };

        struct Entry
        {
            u64 m_offset;
            u64 m_size;
            u32 m_fileNameOffset;
            FileFlags m_flags;
        };

        struct FileEntry
        {
            u64 m_offset;
            u64 m_size;
            FileFlags m_flags;
        };

        static constexpr u64 kMagicNumber = Hashing::Hash64Static("Kryne Engine Archive");
        static constexpr Version kVersion = Version::DateBased(2026, 0, 2026, 4, 8);
        static constexpr size_t kAlignment = sizeof(u64);

        static Archive* Load(AllocatorInstance _allocator, Platform::ReadOnlyFileDescriptor* _file);

        [[nodiscard]] eastl::string_view GetMountPoint() const { return m_mountPoint; }
        [[nodiscard]] const FileEntry* GetFileEntry(StringViewHash _hash) const;
        [[nodiscard]] Platform::ReadOnlyFileDescriptor* GetFileDescriptor() const { return m_file; }

    private:
        Platform::ReadOnlyFileDescriptor* m_file;
        eastl::string m_mountPoint;
        eastl::vector_map<StringHashBase, FileEntry> m_fileTable;
    };

    class ArchiveMaker
    {
    public:
        ArchiveMaker(
            std::ofstream& _file,
            eastl::string_view _mountPoint,
            u32 _fileCount,
            size_t _arenaByteSize = 512 << 10,
            AllocatorInstance _allocator = {});

        ~ArchiveMaker();

        void SetCompressionLevel(const s32 _compressionLevel) { m_compressionLevel = _compressionLevel; }
        [[nodiscard]] s32 GetCompressionLevel() const { return m_compressionLevel; }

        void AddFile(std::ifstream& _file, eastl::string_view _path, FileFlags _flags);

        void Finish();

    private:
        std::ofstream& m_file;
        eastl::vector<char> m_stringBuffer;
        eastl::vector<Archive::Entry> m_entries;
        eastl::span<char> m_arena;
        s32 m_compressionLevel = 3;
        ZSTD_CCtx_s* m_zstdCompressionContext = nullptr;
    };
}
