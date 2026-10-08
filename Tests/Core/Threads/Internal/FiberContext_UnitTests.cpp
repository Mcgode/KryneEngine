/**
* @file
* @author Max Godefroy
* @date 10/10/2024.
*/

#include <gtest/gtest.h>

#if defined(_WIN32)
#   include <KryneEngine/Core/Platform/Windows.h>
#endif

#include "../../../Core/Src/Threads/Internal/FiberContext.hpp"
#include <KryneEngine/Core/Platform/StdAlloc.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    TEST(FiberContextAllocator, Init)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        EXPECT_NO_THROW(FiberContextAllocator allocator { AllocatorInstance() });

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        EXPECT_TRUE(catcher.GetCaughtMessages().empty());
    }

    TEST(FiberContextAllocator, GetContext)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        FiberContextAllocator allocator { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u16 i = 0;
        for (; i < FiberContextAllocator::kSmallStackCount + FiberContextAllocator::kBigStackCount; i++)
        {
            FiberContext* ctx = allocator.GetContext(i);
            EXPECT_NE(ctx, nullptr);
            if (ctx != nullptr)
            {
                EXPECT_NE(ctx->m_context, nullptr);
            }
        }

        EXPECT_TRUE(catcher.GetCaughtMessages().empty());

        EXPECT_EQ(allocator.GetContext(i), nullptr);
        EXPECT_EQ(catcher.GetCaughtMessages().size(), 1);
    }

    TEST(FiberContextAllocator, Allocate)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        FiberContextAllocator allocator { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32 expectedErrorCount = 0;

        const auto testStackType = [&](const u16 count, const bool bigStack)
        {
            constexpr u16 initial = ~0;
            u16 id = initial;

            for (u16 i = 0; i < count; i++)
            {
                const u16 prevId = id;
                EXPECT_TRUE(allocator.Allocate(bigStack, id));

                if (prevId != initial)
                {
                    EXPECT_LE(prevId, id);
                }
            }

            EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedErrorCount);

            const u16 prevId = id;
            EXPECT_FALSE(allocator.Allocate(bigStack, id));
            expectedErrorCount++;
            EXPECT_EQ(prevId, id);
            EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedErrorCount);
            EXPECT_EQ(catcher.GetLastCaughtMessages().m_message, "Out of Fiber stacks!");
        };

        testStackType(FiberContextAllocator::kSmallStackCount, false);
        testStackType(FiberContextAllocator::kBigStackCount, true);
    }

    TEST(FiberContextAllocator, Free)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        FiberContextAllocator allocator { AllocatorInstance() };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const auto testStackType = [&](const u16 count, const bool bigStack)
        {
            u16 id = 0;
            u16 first = 0;

            allocator.Allocate(bigStack, first);
            for (u16 i = 1; i < count; i++)
            {
                EXPECT_TRUE(allocator.Allocate(bigStack, id));
            }

            allocator.Free(first);
            EXPECT_TRUE(allocator.Allocate(bigStack, id));
            EXPECT_EQ(id, first);

            // Make sure priority queue works appropriately
            allocator.Free(first + 1);
            allocator.Free(first);
            EXPECT_TRUE(allocator.Allocate(bigStack, id));
            EXPECT_EQ(id, first);

            EXPECT_TRUE(catcher.GetCaughtMessages().empty());
        };

        testStackType(FiberContextAllocator::kSmallStackCount, false);
        testStackType(FiberContextAllocator::kBigStackCount, true);

        allocator.Free(FiberContextAllocator::kBigStackCount + FiberContextAllocator::kSmallStackCount);
        EXPECT_EQ(catcher.GetCaughtMessages().size(), 1);
    }

    TEST(FiberContext, SwapContext)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        struct Contexts
        {
            FiberContext starting;
            FiberContext target;
        } contexts;

        contexts.starting.m_name = "Starting";
        contexts.target.m_name = "Target";

        constexpr auto targetFunction = [](boost::context::detail::transfer_t _transfer)
        {
            auto* contexts = static_cast<Contexts*>(_transfer.data);

            contexts->starting.m_context = _transfer.fctx;

            contexts->target.m_name = "Targeted";

            contexts->target.SwapContext(&contexts->starting);
        };

        constexpr size_t stackSize = 1 << 16;
        void* stack = StdAlloc::MemAlign(16, stackSize);

        std::thread startThread(
            [&](){
                contexts.target.m_context = boost::context::detail::make_fcontext(
                    static_cast<u8*>(stack) + stackSize, // Stack starts from the end
                    stackSize,
                    targetFunction);

                contexts.starting.SwapContext(&contexts.target);

                // The target fiber swapped back to us, which locked our mutex. In a real scenario, the next swap
                // away from this context would unlock it; here, release it on the owning thread before destruction.
                contexts.starting.m_mutex.ManualUnlock();
            });
        startThread.join();

        EXPECT_EQ(contexts.target.m_name, "Targeted");

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        StdAlloc::Free(stack);
        EXPECT_TRUE(catcher.GetCaughtMessages().empty());
    }
}
