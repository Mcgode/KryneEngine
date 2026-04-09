/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2024.
 */


#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/Assert.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    TEST(Assert, SetCustomCallback)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        constexpr auto callback0 = [](const char*, u32, const char*, const char* message)
        {
            EXPECT_STREQ(message, "Callback 0");
            return Assertion::CallbackResponse::Break;
        };

        constexpr auto callback1 = [](const char*, u32, const char*, const char* message)
        {
            EXPECT_STREQ(message, "Callback 1");
            return Assertion::CallbackResponse::Continue;
        };

        constexpr auto callback2 = [](const char*, u32, const char*, const char* message)
        {
            EXPECT_STREQ(message, "Callback 2");
            return Assertion::CallbackResponse::Ignore;
        };

        const u64 line = __LINE__;


        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        Assertion::AssertionCallback previousCallback = Assertion::SetAssertionCallback(callback0);
        EXPECT_EQ(previousCallback, nullptr); // Should be nullptr, aka default callback

        bool result = Assertion::Error("", line, __FILE__, "Callback 0");
        EXPECT_TRUE(result);

        previousCallback = Assertion::SetAssertionCallback(callback1);
        EXPECT_EQ(previousCallback, callback0);

        result = Assertion::Error("", line, __FILE__, "Callback 1");
        EXPECT_FALSE(result);

        previousCallback = Assertion::SetAssertionCallback(callback2);
        EXPECT_EQ(previousCallback, callback1);

        result = Assertion::Error("", line, __FILE__, "Callback 2");
        EXPECT_FALSE(result);

        previousCallback = Assertion::SetAssertionCallback(callback0);
        EXPECT_EQ(previousCallback, callback2);
        result = Assertion::Error("", line, __FILE__, "Callback 0");
        EXPECT_FALSE(result);
        result = Assertion::Error("", line + 1, __FILE__, "Callback 0");
        EXPECT_TRUE(result);
        result = Assertion::Error("", line, __FILE__ "p", "Callback 0");
        EXPECT_TRUE(result);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        Assertion::SetAssertionCallback(nullptr);
    }

    TEST(AssertUtils, ProperScoping)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        constexpr auto getCurrentCallback = []()
        {
            Assertion::AssertionCallback callback = Assertion::SetAssertionCallback(nullptr);
            Assertion::SetAssertionCallback(callback);
            return callback;
        };

        constexpr auto customCallback = [](const char*, u32, const char*, const char*)
        {
            return Assertion::CallbackResponse::Continue;
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // Basic scoping
        {
            ScopedAssertCatcher catcher;
            EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
            EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher);
        }
        EXPECT_EQ(getCurrentCallback(), nullptr);
        EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, nullptr);

        // Multi-scoped
        {
            ScopedAssertCatcher catcher0;
            EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
            EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher0);
            {
                ScopedAssertCatcher catcher1;
                EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
                EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher1);
                {
                    ScopedAssertCatcher catcher2;
                    EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
                    EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher2);
                }
                EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
                EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher1);
            }
            EXPECT_EQ(ScopedAssertCatcher::Callback, getCurrentCallback());
            EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, &catcher0);
        }
        EXPECT_EQ(getCurrentCallback(), nullptr);
        EXPECT_EQ(ScopedAssertCatcher::s_currentCatcher, nullptr);

        // Stop overriding custom callback when unscoped
        Assertion::SetAssertionCallback(customCallback);
        {
            ScopedAssertCatcher catcher;
        }
        EXPECT_EQ(customCallback, getCurrentCallback());

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------
        Assertion::SetAssertionCallback(nullptr);
    }

    TEST(AssertUtils, CaughtValues)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        const char* file = __builtin_FILE();
        const char* function = __builtin_FUNCTION();

        const char* messages[] = {
            "Message 0",
            "Message 1",
            "Message 2",
            "Message 3",
            "Message 4",
        };

        const auto checkValidMessage = [&](const ScopedAssertCatcher::Message& _message, u32 _index)
        {
            EXPECT_STREQ(_message.m_fileName.c_str(), file);
            EXPECT_STREQ(_message.m_functionName.c_str(), function);
            EXPECT_STREQ(_message.m_message.c_str(), messages[_index]);
            EXPECT_EQ(_message.m_lineIndex, _index);
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const bool result = Assertion::Error(function, 0, file, messages[0]);
        EXPECT_FALSE(result);
        EXPECT_EQ(catcher.GetCaughtMessages().size(), 1);
        checkValidMessage(catcher.GetCaughtMessages()[0], 0);

        Assertion::Error(function, 3, file, messages[3]);
        Assertion::Error(function, 4, file, messages[4]);
        Assertion::Error(function, 2, file, messages[2]);
        Assertion::Error(function, 1, file, messages[1]);
        EXPECT_EQ(catcher.GetCaughtMessages().size(), 5);
        checkValidMessage(catcher.GetCaughtMessages()[1], 3);
        checkValidMessage(catcher.GetCaughtMessages()[2], 4);
        checkValidMessage(catcher.GetCaughtMessages()[3], 2);
        checkValidMessage(catcher.GetCaughtMessages()[4], 1);

        constexpr u32 count = 1000;
        for (u32 i = 0; i < count; i++)
        {
            Assertion::Error(function, 0, file, messages[0]);
        }
        EXPECT_EQ(catcher.GetCaughtMessages().size(), 5 + count);
        for (u32 i = 0; i < count; i++)
        {
            checkValidMessage(catcher.GetCaughtMessages()[i + 5], 0);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------
    }

    TEST(Assert, KE_ASSERT)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        const char* file = __builtin_FILE();
        const char* function = __builtin_FUNCTION();

        const auto checkValidMessage = [&](const char* _msg)
        {
            const ScopedAssertCatcher::Message& message = catcher.GetLastCaughtMessages();
            EXPECT_STREQ(message.m_fileName.c_str(), file);
            EXPECT_STREQ(message.m_functionName.c_str(), function);
            EXPECT_STREQ(message.m_message.c_str(), _msg);
        };

        u32 expectedSize = 0;
        const auto checkSize = [&]()
        {
            EXPECT_EQ(catcher.GetCaughtMessages().size(), expectedSize);
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        KE_ASSERT(true);
        checkSize();

        KE_ASSERT(false);
        expectedSize++;
        checkSize();
        checkValidMessage("false");

        KE_ASSERT((1 + 1) == 2);
        checkSize();

        KE_ASSERT((1 == 2 ));
        expectedSize++;
        checkSize();
        checkValidMessage("(1 == 2 )");

        KE_ASSERT_MSG(false, "Message");
        expectedSize++;
        checkSize();
        checkValidMessage("Message");

        constexpr u32 count = 100;
        constexpr u32 divider = 3;
        for (u32 i = 0; i < count; i++)
        {
            const char* msg = "%d is not dividable by %d";
            KE_ASSERT_MSG(i % divider == 0, msg, i, divider);

            if (i % divider != 0)
            {
                expectedSize++;
                checkSize();
                eastl::string message;
                message.sprintf(msg, i, divider);
                checkValidMessage(message.c_str());
            }
        }

        KE_ERROR("Message");
        expectedSize++;
        checkSize();
        checkValidMessage("Message");

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------
    }

    TEST(Assert, KE_VERIFY)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        bool result;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        result = KE_VERIFY(true);
        EXPECT_TRUE(result);

        result = KE_VERIFY(false);
        EXPECT_FALSE(result);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
#pragma ide diagnostic ignored "UnusedValue"
    TEST(Assert, KE_FATAL)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        GTEST_FLAG_SET(death_test_style, "threadsafe");
        ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            ASSERT_NO_FATAL_FAILURE({
                KE_ASSERT_FATAL(1 == 1);
            });
        }

        {
            ASSERT_DEATH(
                {
                    KE_ASSERT_FATAL(1 == 2);
                },
                "");
        }

        {
            ASSERT_DEATH(
                {
                    KE_ASSERT_FATAL_MSG(1 == 2, "Bad value");
                },
                "");
            // EXPECT_STREQ(catcher.GetLastCaughtMessages().m_message.c_str(), "Bad value");
        }

        {
            ASSERT_DEATH(
                {
                    KE_FATAL("Message");
                },
                "");
            // EXPECT_STREQ(catcher.GetLastCaughtMessages().m_message.c_str(), "Message");
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------
    }
#pragma clang diagnostic pop
}