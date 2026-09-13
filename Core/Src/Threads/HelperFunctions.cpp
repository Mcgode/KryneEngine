/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#include "KryneEngine/Core/Threads/HelperFunctions.hpp"

#if defined(_WIN32) || defined(WIN32)
#   define WINDOWS_THREADS
#   include <KryneEngine/Core/Platform/Windows.h>
#   if defined(__MINGW32__)
        // libstdc++ on mingw implements std::thread on top of winpthreads, so
        // native_handle() is a pthread_t rather than a Win32 HANDLE.
#       include <pthread.h>
#   endif
#elif defined(__unix__)
#   define PTHREADS
#   include <pthread.h>
#   include <csignal>
#elif defined(__APPLE__)
#   define MACOS_THREADS
#endif

#include "KryneEngine/Core/Common/Assert.hpp"

namespace KryneEngine::Threads
{
    bool SetThreadHardwareAffinity(std::thread &_thread, u32 _coreIndex)
    {
#if defined(WINDOWS_THREADS)
#   if defined(__MINGW32__)
        const HANDLE nativeHandle = static_cast<HANDLE>(pthread_gethandle(_thread.native_handle()));
#   else
        const HANDLE nativeHandle = _thread.native_handle();
#   endif
        DWORD_PTR dw = SetThreadAffinityMask(nativeHandle, DWORD_PTR(1) << _coreIndex);
        if (dw == 0)
        {
            eastl::string msg;
            msg.sprintf("Unable to set thread affinity mask: %s", GetLastError());
            KE_ERROR(msg.c_str());
        }
        return dw != 0;
#elif defined(PTHREADS)
        cpu_set_t coreSet {};
        CPU_ZERO(&coreSet);
        CPU_SET(_coreIndex, &coreSet);
        s32 result = pthread_setaffinity_np(_thread.native_handle(), 1, &coreSet);
        return result == 0;
#elif defined(MACOS_THREADS)
        // macOS doesn't allow us to assign a thread to a specific cpu core sadly
        return true;
#else
        #error No supported thread API
        return false;
#endif
    }

    bool DisableThreadSignals()
    {
#if defined(WINDOWS_THREADS)
        return true;
#elif defined(PTHREADS) || defined(MACOS_THREADS)
        sigset_t mask;
        sigfillset(&mask);
        // Crash/error signals stay unblocked. They're synchronous -- raised by this thread itself
        // faulting, not delivered asynchronously from outside -- so blocking them alongside the rest
        // doesn't protect anything (unlike an async signal, they can't land mid fiber-stack-switch);
        // it only hides the fault. Without a debugger attached, that turns an immediate, loud crash
        // into a silent, unrecoverable hang instead: SIGTRAP from KE_DEBUG_BREAK()/asserts, or
        // SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGSYS/SIGABRT from a real fault or a sanitizer-detected one
        // (observed live: ASan/TSan's own allocator instrumentation raising SIGSEGV while
        // symbolizing a fiber-stack allocation).
        sigdelset(&mask, SIGTRAP);
        sigdelset(&mask, SIGSEGV);
        sigdelset(&mask, SIGBUS);
        sigdelset(&mask, SIGILL);
        sigdelset(&mask, SIGFPE);
        sigdelset(&mask, SIGSYS);
        sigdelset(&mask, SIGABRT);
        return pthread_sigmask(SIG_BLOCK, &mask, nullptr) == 0;
#else
#error No supported thread API
        return false;
#endif
    }
}