/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2024.
 */

#pragma once

#include <EASTL/span.h>
#include <KryneEngine/Core/Common/Assert.hpp>

namespace KryneEngine::Tests
{
    class ScopedAssertCatcher
    {
        friend class AssertUtils_ProperScoping_Test;

    public:
        struct Message
        {
            eastl::string m_message;
            eastl::string m_functionName;
            eastl::string m_fileName;
            u32 m_lineIndex;
        };

        ScopedAssertCatcher();

        ~ScopedAssertCatcher();

        [[nodiscard]] eastl::span<const Message> GetCaughtMessages() const { return m_caughtMessages; }
        [[nodiscard]] const Message& GetLastCaughtMessages() const { return m_caughtMessages.back(); }

        void ExpectMessageCount(u32 count);
        inline void ExpectNoMessage() { ExpectMessageCount(0); }

    private:
        static bool Callback(const char* _function, uint32_t _line, const char* _file, const char* _message);

        Assertion::AssertCaptureFunction m_previousCallback = nullptr;
        ScopedAssertCatcher* m_previousCatcher = nullptr;

        static ScopedAssertCatcher* s_currentCatcher;
        eastl::vector<Message> m_caughtMessages;
    };
}