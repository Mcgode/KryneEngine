#include "KryneEngine/Core/Common/Assert.hpp"

#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"
#include <EASTL/vector_set.h>
#include <cstdio>

#if defined(_WIN32)
#	include <KryneEngine/Core/Platform/Windows.h>
#endif

namespace KryneEngine::Assertion
{
	void Error(const char* _function, const u32 _line, const char* _file, const char* _formatMessage, ...)
	{
        char buffer[4096];
        va_list arguments;
        va_start(arguments, _formatMessage);
        snprintf(buffer, sizeof(buffer), _formatMessage, arguments);
        va_end(arguments);

        printf("Assertion failed in %s (at %s:%d):\n\n\t%s\n", _function, _file, _line, buffer);
	}
}
