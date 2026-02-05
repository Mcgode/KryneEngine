/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include "KryneEngine/Core/Threads/RwSpinLock.hpp"


#include <EASTL/vector_set.h>
#include <KryneEngine/Core/Threads/SpinLock.hpp>

#include "KryneEngine/Modules/Resources/IResourceLoader.hpp"

namespace KryneEngine::Modules::Resources
{
    class SerialResourceLoader final: public IResourceLoader
    {
    public:
        explicit SerialResourceLoader(AllocatorInstance _allocator);

        void RequestLoad(
            const StringHash& _path,
            ResourceEntry* _entry,
            IResourceManager* _resourceManager,
            u64 _loadFlags) override;

    private:
        eastl::vector_set<StringHash> m_pendingRequests;
        alignas(Threads::kCacheLineSize) SpinLock m_lock;
    };
}
