/**
 * @file
 * @author Max Godefroy
 * @date 05/02/2026.
 */

#pragma once

#include <moodycamel/concurrentqueue.h>

#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    struct ConcurrentQueueTraits: moodycamel::ConcurrentQueueDefaultTraits
    {
        static AllocatorInstance s_globalConcurrentQueueAllocator;

        static void* malloc(const size_t _size) { return s_globalConcurrentQueueAllocator.allocate(_size); }
        static void free(void* _ptr) { s_globalConcurrentQueueAllocator.deallocate(_ptr); }
    };

    template<class T>
    using ConcurrentQueue = moodycamel::ConcurrentQueue<T, ConcurrentQueueTraits>;
}
