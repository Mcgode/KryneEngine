/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

#include <fstream>
#include <zstd.h>
#include <KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp>

namespace KryneEngine::Modules::Resources
{
    eastl::span<std::byte> IResourceManager::LoadResource(ResourceEntry* _entry, const FileSystem::ReadOnlyFile& _file)
    {
        const size_t expectedSize = _file.GetSize();
        auto* data = GetAllocator().Allocate<std::byte>(expectedSize);
        const size_t size = _file.Read(0, { data, expectedSize });
        if (!KE_VERIFY(size == expectedSize))
        {
            GetAllocator().deallocate(data, expectedSize);
            return {};
        }

        if (BitUtils::EnumHasAny(_file.GetFlags(), FileSystem::FileFlags::ZstdCompressed))
        {
            const size_t decompressedSize = ZSTD_getFrameContentSize(data, size);
            auto* decompressedData = GetAllocator().Allocate<std::byte>(decompressedSize);
            ZSTD_decompress(decompressedData, decompressedSize, data, size);
            GetAllocator().deallocate(data, size);
            return { decompressedData, decompressedSize };
        }

        return { data, size };
    }
}
