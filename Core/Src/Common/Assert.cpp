#include "KryneEngine/Core/Common/Assert.hpp"

#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"
#include <cstdio>

namespace KryneEngine::Assertion
{
    static AssertCaptureFunction s_captureFunction = nullptr;

	bool Error(const char* _function, const u32 _line, const char* _file, const char* _formatMessage, ...)
	{
        char buffer[4096];
        va_list arguments;
        va_start(arguments, _formatMessage);
        vsnprintf(buffer, sizeof(buffer), _formatMessage, arguments);
        va_end(arguments);

	    if (s_captureFunction != nullptr)
	    {
	        return s_captureFunction(_function, _line, _file, buffer);
	    }

        printf("Assertion failed in %s (at %s:%d):\n\n\t%s\n", _function, _file, _line, buffer);

	    return true;
	}

    AssertCaptureFunction CaptureAssertions(const AssertCaptureFunction _captureFunction)
	{
        const AssertCaptureFunction old = s_captureFunction;
	    s_captureFunction = _captureFunction;
	    return old;
	}
} // namespace KryneEngine::Assertion
