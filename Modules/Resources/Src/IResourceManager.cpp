/**
 * @file
 * @author Max Godefroy
 * @date 24/03/2026.
 */

#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

#include <fstream>

namespace KryneEngine::Modules::Resources
{
    eastl::span<std::byte> IResourceManager::LoadResource(ResourceEntry* _entry, eastl::string_view _path)
    {
        std::ifstream file(_path.data(), std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }

        const size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        char* data = GetAllocator().Allocate<char>(size);
        file.read(data, size);
        file.close();
        return { reinterpret_cast<std::byte*>(data), size };
    }
}
