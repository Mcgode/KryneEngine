/**
 * @file
 * @author Max Godefroy
 * @date 02/08/2021.
 */

#pragma once

#include "Types.hpp"

namespace KryneEngine::Assertion
{
    enum class CallbackResponse
    {
        Break,
        Continue,
        Ignore,
    };

    using AssertionCallback = CallbackResponse (*)(const char*, u32, const char*, const char*);

    bool Error(const char* _function, u32 _line, const char* _file, const char* _formatMessage, ...);

    AssertionCallback SetAssertionCallback(AssertionCallback _userCallback);
}

#if defined(__APPLE__)
#   define KE_DEBUG_BREAK() __builtin_debugtrap()
#else
#   define KE_DEBUG_BREAK() __debugbreak()
#endif

#define KE_ASSERT_MSG(condition, ...) do \
	{ \
		if (!(condition)) [[unlikely]] \
		{\
			if (KryneEngine::Assertion::Error(__builtin_FUNCTION(), __builtin_LINE(), __builtin_FILE(), __VA_ARGS__)) \
			{ \
				KE_DEBUG_BREAK(); \
			} \
		} \
	} \
	while(0)
#define KE_ASSERT(condition) KE_ASSERT_MSG(condition, #condition)

#define KE_ASSERT_FATAL_MSG(condition, ...) do \
    { \
        if (!(condition)) [[unlikely]] \
        { \
            KryneEngine::Assertion::Error(__builtin_FUNCTION(), __builtin_LINE(), __builtin_FILE(), __VA_ARGS__); \
            abort(); \
        } \
    } \
    while(0)
#define KE_ASSERT_FATAL(condition) KE_ASSERT_FATAL_MSG(condition, #condition)

#define KE_VERIFY_MSG(condition, ...) ((condition) ? true: \
	KryneEngine::Assertion::Error(__builtin_FUNCTION(), __builtin_LINE(), __builtin_FILE(), __VA_ARGS__) ? KE_DEBUG_BREAK(), false: false)
#define KE_VERIFY(condition) KE_VERIFY_MSG(condition, #condition)

#define KE_ERROR(...) do { if (KryneEngine::Assertion::Error(__builtin_FUNCTION(), __builtin_LINE(), __builtin_FILE(), __VA_ARGS__)) KE_DEBUG_BREAK(); } while (0)
#define KE_FATAL(...) do { KryneEngine::Assertion::Error(__builtin_FUNCTION(), __builtin_LINE(), __builtin_FILE(), __VA_ARGS__); abort(); } while (0)

#define IF_NOT_VERIFY(cond) if (!KE_VERIFY(cond)) [[unlikely]]
#define IF_NOT_VERIFY_MSG(cond, ...) if (!KE_VERIFY_MSG(cond, __VA_ARGS__)) [[unlikely]]
#define VERIFY_OR_RETURN(cond, returnValue) IF_NOT_VERIFY(cond) return returnValue
#define VERIFY_OR_RETURN_VOID(cond)  VERIFY_OR_RETURN(cond, )