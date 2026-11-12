/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

#include <fstream>
#include <KryneEngine/Core/Memory/Allocators/GlobalScratchAllocator.hpp>
#include <KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

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
            const u64 decompressedSize = ZSTD_getFrameContentSize(data, size);
            if (!KE_VERIFY(decompressedSize != ZSTD_CONTENTSIZE_UNKNOWN && decompressedSize != ZSTD_CONTENTSIZE_ERROR))
            {
                GetAllocator().deallocate(data, size);
                return {};
            }
            auto* decompressedData = GetAllocator().Allocate<std::byte>(decompressedSize);

            size_t result;
            {
                // The decompression context only lives for this call, and nothing below yields: it goes in the
                // scratch allocator rather than in zstd's own `malloc`.
                const auto scratchScope = GlobalScratchAllocator::GetScratchAllocator();
                const size_t workspaceSize = ZSTD_estimateDCtxSize();
                void* workspace = scratchScope.GetAllocator().allocate(workspaceSize, alignof(u64));
                ZSTD_DCtx* context = workspace != nullptr ? ZSTD_initStaticDCtx(workspace, workspaceSize) : nullptr;
                result = KE_VERIFY(context != nullptr)
                    ? ZSTD_decompressDCtx(context, decompressedData, decompressedSize, data, size)
                    : ZSTD_decompress(decompressedData, decompressedSize, data, size);
            }

            GetAllocator().deallocate(data, size);
            if (!KE_VERIFY(!ZSTD_isError(result) && result == decompressedSize))
            {
                GetAllocator().deallocate(decompressedData, decompressedSize);
                return {};
            }
            return { decompressedData, decompressedSize };
        }

        return { data, size };
    }
}
