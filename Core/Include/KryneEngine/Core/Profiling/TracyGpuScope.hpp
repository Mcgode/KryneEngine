/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2025.
 */

#pragma once

#include "EASTL/vector_map.h"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    struct TracyGpuScope
    {
        TracyGpuScope(
            GraphicsContext* _graphicsContext,
            TracyGpuProfilerContext* _context,
            CommandListHandle _commandList,
            const tracy::SourceLocationData* _src,
            bool _isActive)
            : m_graphicsContext(_graphicsContext)
            , m_context(_context)
            , m_commandList(_commandList)
#ifdef TRACY_ONDEMAND
            , m_active( _isActive && tracy::GetProfiler().IsConnected() )
#else
            , m_isActive(_isActive)
#endif
        {
            if (!m_isActive || m_context == nullptr)
                return;

            const TimestampHandle gpuTimestamp = m_graphicsContext->PutTimestamp(m_commandList);
            if (gpuTimestamp.m_index == ~0u)
            {
                m_isActive = false;
                return;
            }

            m_queryId = m_context->ReserveQuery();
            m_context->SetQueryTimestampIndex(m_queryId, gpuTimestamp.m_index);

            using namespace tracy;
            QueueItem* item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginSerial);
            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, reinterpret_cast<u64>(_src));
            MemWrite(&item->gpuZoneBegin.thread, GetThreadHandle());
            MemWrite(&item->gpuZoneBegin.queryId, m_queryId);
            MemWrite(&item->gpuZoneBegin.context, m_context->GetContextId());
            Profiler::QueueSerialFinish();
        }

        TracyGpuScope(
            GraphicsContext* _graphicsContext,
            TracyGpuProfilerContext* _context,
            CommandListHandle _commandList,
            u32 _line,
            const eastl::string_view _file,
            const eastl::string_view _function,
            u32 _color,
            bool _isActive,
            const char* _nameFmt,
            ...)
            : m_graphicsContext(_graphicsContext)
            , m_context(_context)
            , m_commandList(_commandList)
#ifdef TRACY_ONDEMAND
            , m_active( _isActive && tracy::GetProfiler().IsConnected() )
#else
            , m_isActive(_isActive)
#endif
        {
            if (!m_isActive || m_context == nullptr)
                return;

            const TimestampHandle gpuTimestamp = m_graphicsContext->PutTimestamp(m_commandList);
            if (gpuTimestamp.m_index == ~0u)
            {
                m_isActive = false;
                return;
            }

            m_queryId = m_context->ReserveQuery();
            m_context->SetQueryTimestampIndex(m_queryId, gpuTimestamp.m_index);

            using namespace tracy;
            QueueItem* item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginAllocSrcLocSerial);

            char name[256];
            va_list args;
            va_start(args, _nameFmt);
            const u32 finalNameSize = vsnprintf(name, 256, _nameFmt, args);
            va_end(args);

            auto src = Profiler::AllocSourceLocation(
                _line,
                _file.data(),
                _file.size(),
                _function.data(),
                _function.size(),
                name,
                finalNameSize,
                _color);

            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, src);
            MemWrite(&item->gpuZoneBegin.thread, GetThreadHandle());
            MemWrite(&item->gpuZoneBegin.queryId, m_queryId);
            MemWrite(&item->gpuZoneBegin.context, m_context->GetContextId());
            Profiler::QueueSerialFinish();
        }

        ~TracyGpuScope()
        {
            if (!m_isActive || m_context == nullptr)
                return;

            const TimestampHandle gpuTimestamp = m_graphicsContext->PutTimestamp(m_commandList);
            m_context->SetQueryTimestampIndex(m_queryId + 1, gpuTimestamp.m_index);

            using namespace tracy;
            QueueItem* item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneEndSerial);
            MemWrite(&item->gpuZoneEnd.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneEnd.thread, GetThreadHandle());
            MemWrite(&item->gpuZoneEnd.queryId, m_queryId + 1);
            MemWrite(&item->gpuZoneEnd.context, m_context->GetContextId());
            Profiler::QueueSerialFinish();
        }

    private:
        GraphicsContext* m_graphicsContext = nullptr;
        TracyGpuProfilerContext* m_context = nullptr;
        CommandListHandle m_commandList = {};
        u16 m_queryId = 0;
        bool m_isActive;
    };

#define GpuZoneNamed( varname, graphicsContext, profilerContext, commandList, name, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, static_cast<uint32_t>(TracyLine), color }; KryneEngine::TracyGpuScope varname( graphicsContext, profilerContext, commandList, &TracyConcat(__tracy_source_location,TracyLine), active )
#define GpuZoneTransient( varname, graphicsContext, profilerContext, commandList, color, active, nameFmt,... ) KryneEngine::TracyGpuScope varname( graphicsContext, profilerContext, commandList, TracyLine, TracyFile, TracyFunction, color, active, nameFmt, __VA_ARGS__ )

#define KE_GpuZoneScoped( graphicsContext, profilerContext, commandList, name ) GpuZoneNamed( TracyConcat(___tracy_gpu_scoped_zone,TracyLine) , graphicsContext, profilerContext, commandList, name, KE_TRACY_COLOR, true )
#define KE_GpuZoneScopedC( graphicsContext, profilerContext, commandList, color, name ) GpuZoneNamed( TracyConcat(___tracy_gpu_scoped_zone,TracyLine) , graphicsContext, profilerContext, commandList, name, color, true )
#define KE_GpuZoneScopedF( graphicsContext, profilerContext, commandList, nameFmt, ... ) GpuZoneTransient( TracyConcat(___tracy_gpu_scoped_zone,TracyLine), graphicsContext, profilerContext, commandList, KE_TRACY_COLOR, true, nameFmt, __VA_ARGS__ )
#define KE_GpuZoneScopedCF( graphicsContext, profilerContext, commandList, color, nameFmt, ... ) GpuZoneTransient( TracyConcat(___tracy_gpu_scoped_zone,TracyLine), graphicsContext, profilerContext, commandList, color, true, nameFmt, __VA_ARGS__ )
}
