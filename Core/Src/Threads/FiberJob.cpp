/**
 * @file
 * @author Max Godefroy
 * @date 02/07/2022.
 */

#include "KryneEngine/Core/Threads/FiberJob.hpp"

#include "Threads/Internal/FiberContext.hpp"

namespace KryneEngine
{
    FiberJob::FiberJob() = default;

    void FiberJob::SetContext(const u16 _contextId, FiberContext* _context)
    {
        m_contextId = _contextId;
        m_context = _context;
    }

    void FiberJob::ResetContext()
    {
        m_contextId = kInvalidContextId;
        m_context = nullptr;
    }
} // KryneEngine