/**
 * @file
 * @author Max Godefroy
 * @date 08/02/2026.
 */


#include "KryneEngine/Core/Memory/Containers/ConcurrentQueue.hpp"

namespace KryneEngine
{
    AllocatorInstance ConcurrentQueueTraits::s_globalConcurrentQueueAllocator = {};
}