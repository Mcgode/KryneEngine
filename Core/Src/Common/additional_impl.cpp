/**
 * @file
 * @author Max Godefroy
 * @date 10/10/2021.
 */

#include <cstdint>

#include "KryneEngine/Core/Platform/StdAlloc.hpp"

void* operator new[](
    const size_t _size,
    const char* /* _name */,
    int /* _flags */,
    unsigned /* _debugFlags */,
    const char* /* _file */,
    int /* _line */)
{
    return KryneEngine::StdAlloc::Malloc(_size);
}

void* operator new[](
    const size_t _size,
    const size_t _alignment,
    const size_t _alignmentOffset,
    const char* /* _name */,
    int /* _flags */,
    unsigned /* _debugFlags */,
    const char* /* _file */,
    int /* _line */)
{
    void* ptr = KryneEngine::StdAlloc::MemAlign(_size, _alignment);
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + _alignmentOffset);
}

void operator delete(
    void* _p,
    const char* /* _name */,
    int /* _flags */,
    unsigned /* _debugFlags */,
    const char* /* _file */,
    int /* _line */)
{
    if (_p != nullptr)
    {
        KryneEngine::StdAlloc::Free(_p);
    }
}

void operator delete[](
    void* _p,
    const char* /* _name */,
    int /* _flags */,
    unsigned /* _debugFlags */,
    const char* /* _file */,
    int /* _line */)
{
    if (_p != nullptr)
    {
        KryneEngine::StdAlloc::Free(_p);
    }
}