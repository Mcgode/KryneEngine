/**
 * @file
 * @author Max Godefroy
 * @date 12/11/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/Utils/ScopeGuard.hpp>

namespace KryneEngine::Tests
{
    namespace
    {
        struct IncrementFunctor
        {
            int* m_counter;

            void operator()() const { ++(*m_counter); }
        };

        void IncrementGlobal();
        int g_counter = 0;
        void IncrementGlobal() { ++g_counter; }
    }

    TEST(ScopeGuard, RunsLambdaOnScopeExit)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        int counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard([&counter] { ++counter; });
            EXPECT_EQ(counter, 0);
        }
        EXPECT_EQ(counter, 1);
    }

    TEST(ScopeGuard, CancelLambda)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        int counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard([&counter] { ++counter; });
            guard.Cancel();
        }
        EXPECT_EQ(counter, 0);
    }

    TEST(ScopeGuard, RunsFunctorOnScopeExit)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        int counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard(IncrementFunctor { &counter });
            EXPECT_EQ(counter, 0);
        }
        EXPECT_EQ(counter, 1);
    }

    TEST(ScopeGuard, CancelFunctor)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        int counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard(IncrementFunctor { &counter });
            guard.Cancel();
        }
        EXPECT_EQ(counter, 0);
    }

    TEST(ScopeGuard, FunctionPointer)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        g_counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard(&IncrementGlobal);
        }
        EXPECT_EQ(g_counter, 1);

        {
            ScopeGuard guard(&IncrementGlobal);
            guard.Cancel();
        }
        EXPECT_EQ(g_counter, 1);
    }

    TEST(ScopeGuard, CancelIsIdempotent)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        int counter = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ScopeGuard guard([&counter] { ++counter; });
            guard.Cancel();
            guard.Cancel();
        }
        EXPECT_EQ(counter, 0);
    }
}
