/**
 * @file
 * @author Max Godefroy
 * @date 02/07/2022.
 */

#pragma once

#include <EASTL/array.h>
#include <EASTL/priority_queue.h>
#include <boost/context/detail/fcontext.hpp>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"
#include "KryneEngine/Core/Threads/SpinLock.hpp"

// These macros are defined by GCC and/or clang
#if defined(__SANITIZE_ADDRESS__)
#   define HAS_ASAN 1
#elif defined(__has_feature)
#   if __has_feature(address_sanitizer)
#       define HAS_ASAN 1
#   endif
#endif

namespace KryneEngine
{
    struct FiberContext
    {
        friend struct FiberContextAllocator;

        boost::context::detail::fcontext_t m_context {};
        eastl::string m_name {};
        LightweightMutex m_mutex {};
#if defined(HAS_ASAN)
        const void* m_stackBottom = nullptr;
        size_t m_stackSize = 0;
#endif

        void SwapContext(FiberContext *_new);

    private:
        static void RunFiber(boost::context::detail::transfer_t _transfer);
    };

    struct FiberContextAllocator
    {
    public:
        explicit FiberContextAllocator(AllocatorInstance _allocator);

        ~FiberContextAllocator();

        bool Allocate(bool _bigStack, u16 &id_);

        void Free(u16 _id);

        FiberContext* GetContext(u16 _id);

        static constexpr u16 kSmallStackCount = 128;
        static constexpr u16 kBigStackCount = 32;

    private:
        static constexpr size_t kSmallStackSize = 64 * 1024; // 64 KiB
        static constexpr size_t kBigStackSize = 512 * 1024; // 512 KiB
        static constexpr size_t kStackAlignment = 16;

        static_assert(Alignment::IsAligned(kSmallStackSize, kStackAlignment));
        static_assert(Alignment::IsAligned(kBigStackSize, kStackAlignment));

        struct StackIdQueue
        {
            struct Comparator
            {
                constexpr bool operator()(const u16 _a, const u16 _b) const { return _a > _b; }
            };

            eastl::priority_queue<u16, eastl::vector<u16>, Comparator> m_priorityQueue;
            SpinLock m_spinLock;
        };
        StackIdQueue m_availableSmallContextsIds;
        StackIdQueue m_availableBigContextsIds;

        eastl::array<FiberContext, kSmallStackCount + kBigStackCount> m_contexts {};

        using SmallStack = u8[kSmallStackSize];
        SmallStack* m_smallStacks = nullptr;

        using BigStack = u8[kBigStackSize];
        BigStack* m_bigStacks = nullptr;
    };
}