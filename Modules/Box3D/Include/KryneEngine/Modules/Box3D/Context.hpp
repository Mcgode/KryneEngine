/**
 * @file
 * @author Max Godefroy
 * @date 18/08/2026.
 */

#pragma once

#include <box3d/box3d.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>


namespace KryneEngine
{
    class FibersManager;
}

namespace KryneEngine::Modules::Box3D
{
    class Context
    {
    public:
        explicit Context(FibersManager* _fibersManager);

        void InitWorldDef(b3WorldDef& _worldDef, u32 _maxWorkers = 0);

        static void SetAllocator(AllocatorInstance _instance);
        static AllocatorInstance GetAllocator() { return s_allocator; }

    private:
        static void* Allocate(s32 _size, s32 _alignment);
        static void Deallocate(void* _ptr);

        static void* Enqueue(b3TaskCallback* _taskCallback, void* _taskContext, void* _userContext, const char* _name);

        static void Finish(void* _userTask, void* _userContext);

        static AllocatorInstance s_allocator;

        FibersManager* m_fibersManager;
    };
}