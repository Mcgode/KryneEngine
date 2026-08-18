/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#include "KryneEngine/Core/Threads/FiberTls.hpp"

#include "KryneEngine/Core/Threads/FibersManager.hpp"
#include "KryneEngine/Core/Window/Input/Enums.hpp"
#include "eathread/eathread.h"

namespace KryneEngine
{
    template<class T, class Allocator>
    inline void FiberTls<T, Allocator>::Init(const FibersManager *_fibersManager, const T &_value)
    {
        m_array.Resize(_fibersManager->GetFiberThreadCount());
        m_array.InitAll(_value);
    }

    template<class T, class Allocator>
    template<typename F>
    void FiberTls<T, Allocator>::InitFunc(const FibersManager *_fibersManager, F _initFunction)
    {
        m_array.Resize(_fibersManager->GetFiberThreadCount());

        for (auto& localFiberValue: m_array)
        {
            _initFunction(localFiberValue);
        }
    }

    template <class T, class Allocator>
    void FiberTls<T, Allocator>::InitDefault(const FibersManager* _fibersManager)
    {
        m_array.Resize(_fibersManager->GetFiberThreadCount());
        m_array.InitAll();
    }

    template<class T, class Allocator>
    inline T& FiberTls<T, Allocator>::Load()
    {
        KE_ASSERT(FiberThread::IsFiberThread());

        return Load(FiberThread::GetCurrentFiberThreadIndex());
    }
} // KryneEngine