/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2024.
 */

#include "AssertUtils.hpp"

#include <gtest/gtest.h>

namespace KryneEngine::Tests
{
    ScopedAssertCatcher* ScopedAssertCatcher::s_currentCatcher = nullptr;

    ScopedAssertCatcher::ScopedAssertCatcher()
    {
        m_previousCallback = Assertion::CaptureAssertions(Callback);
        if (s_currentCatcher != nullptr)
        {
            m_previousCatcher = s_currentCatcher;
        }
        s_currentCatcher = this;
    }

    ScopedAssertCatcher::~ScopedAssertCatcher()
    {
        Assertion::AssertCaptureFunction current = Assertion::CaptureAssertions(m_previousCallback);
        EXPECT_EQ(current, Callback);

        s_currentCatcher = m_previousCatcher;
    }

    bool ScopedAssertCatcher::Callback(const char* _function, uint32_t _line, const char* _file, const char* _message)
    {
        EXPECT_NE(s_currentCatcher, nullptr);
        s_currentCatcher->m_caughtMessages.push_back({
            .m_message = _message,
            .m_functionName = _function,
            .m_fileName = _file,
            .m_lineIndex = _line,
        });
        return false; // Don't trigger debug break
    }

    void ScopedAssertCatcher::ExpectMessageCount(u32 count)
    {
        EXPECT_EQ(m_caughtMessages.size(), count);
        if (m_caughtMessages.size() == count)
        {
            return;
        }

        constexpr u32 maxPrintMessageCount = 5;
        const u32 printMessageCount = eastl::min<u32>(maxPrintMessageCount, m_caughtMessages.size());

        if (printMessageCount == 0)
        {
            std::cout << "Did not catch any assertion message" << std::endl;
        }
        else
        {
            std::cout << "Printing last " << printMessageCount << " caught assert messages:" << std::endl;

            for (u32 i = 1; i <= printMessageCount; i++)
            {
                const Message& message = m_caughtMessages[m_caughtMessages.size() - i];

                std::cout
                    << " - \""
                    << message.m_message.c_str()
                    << "\" in "
                    << message.m_functionName.c_str()
                    << " ("
                    << message.m_fileName.c_str()
                    << ":"
                    << message.m_lineIndex
                    << ")"
                    << std::endl;
            }
        }
    }
}
