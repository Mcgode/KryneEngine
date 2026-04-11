/**
 * @file
 * @author Max Godefroy
 * @date 23/02/2025.
 */

#include "KryneEngine/Core/Platform/StdAlloc.hpp"

#include <new>

#if defined(__linux__)
#   include <stdlib.h>
#endif

namespace KryneEngine::StdAlloc
{
    void* Malloc(size_t _size)
    {
        return MemAlign(_size, sizeof(size_t));
    }

    void* MemAlign(size_t _size, size_t _alignment)
    {
        _alignment = _alignment < sizeof(size_t) ? sizeof(size_t) : _alignment;
#if defined(_WIN32)
        return _aligned_malloc(_size, _alignment);
#else
        void* ptr = nullptr;
        posix_memalign(&ptr, _alignment, _size);
        return ptr;
#endif
    }

    void Free(void* _ptr)
    {
#if defined(_WIN32)
        _aligned_free(_ptr);
#else
        free(_ptr);
#endif
    }
}