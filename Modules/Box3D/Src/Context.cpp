/**
 * @file
 * @author Max Godefroy
 * @date 18/08/2026.
 */


#include "KryneEngine/Modules/Box3D/Context.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>


namespace KryneEngine::Modules::Box3D
{
    Context::Context(FibersManager* _fibersManager)
        : m_fibersManager(_fibersManager)
    {
    }

    void Context::InitWorldDef(b3WorldDef& _worldDef, u32 _maxWorkers)
    {
        _worldDef = b3DefaultWorldDef();

        if (m_fibersManager)
        {
            _worldDef.workerCount = _maxWorkers == 0
                ? m_fibersManager->GetFiberThreadCount()
                : eastl::min<u32>(_maxWorkers, m_fibersManager->GetFiberThreadCount());
            _worldDef.enqueueTask = Enqueue;
            _worldDef.finishTask = Finish;
            _worldDef.userTaskContext = this;
        }
        else
        {
            _worldDef.workerCount = _maxWorkers;
        }
    }

    void Context::SetAllocator(const AllocatorInstance _instance)
    {
        s_allocator = _instance;
        if (s_allocator != nullptr)
            b3SetAllocator(Allocate, Deallocate);
        else
            b3SetAllocator(nullptr, nullptr);
    }

    void* Context::Allocate(const s32 _size, const s32 _alignment)
    {
        return s_allocator.allocate(_size, _alignment);
    }

    void Context::Deallocate(void* _ptr)
    {
        s_allocator.deallocate(_ptr);
    }

    void* Context::Enqueue(b3TaskCallback* _taskCallback, void* _taskContext, void* _userContext, const char* _name)
    {
        static_assert(sizeof(void*) >= sizeof(SyncCounterId));

        auto* fibersManager = static_cast<Context*>(_userContext)->m_fibersManager;
        const SyncCounterId counter = fibersManager->InitAndBatchJobs({
            .m_function = [_taskCallback, _taskContext, _name](u16)
            {
                KE_ZoneScopedF("%s", _name);
                _taskCallback(_taskContext);
            }
        });

        void* value = nullptr;
        memcpy(&value, &counter, sizeof(counter));
        return value;
    }

    void Context::Finish(void* _userTask, void* _userContext)
    {
        auto* fibersManager = static_cast<Context*>(_userContext)->m_fibersManager;

        SyncCounterId id;
        memcpy(&id, &_userTask, sizeof(id));
        fibersManager->WaitForCounterAndReset(id);
    }

    AllocatorInstance Context::s_allocator {};
}