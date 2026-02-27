/**
 * @file
 * @author Max Godefroy
 * @date 06/07/2025.
 */

#pragma once

/***
 * @brief A util macro to quickly define keyword constructors for copy and move semantics.
 *
 * @param className The name of the class to define the keyword constructors for.
 * @param copyKeyword The keyword to use for the copy semantics.
 * @param moveKeyword The keyword to use for the move semantics.
 *
 * @example Here is the example for a class Foo with deleted copy, but default move:
 * @code
 * class Foo
 * {
 * public:
 *     Foo() = default;
 *     KE_DEFINE_COPY_MOVE_SEMANTICS(Foo, delete, default);
 * }
 * @endcode
 */
#define KE_DEFINE_COPY_MOVE_SEMANTICS(className, copyKeyword, moveKeyword)    \
    className(const className& _other) = copyKeyword;                         \
    className& operator=(const className& _other) = copyKeyword;              \
    className(className&& _other) noexcept = moveKeyword;                     \
    className& operator=(className&& _other) noexcept = moveKeyword

#if !defined(KE_FORCEINLINE)
#  if defined(_MSC_VER)
#    define KE_FORCEINLINE __forceinline
#  elif defined(__GNUC__) && __GNUC__ > 3
     // Clang also defines __GNUC__ (as 4)
#    define KE_FORCEINLINE inline __attribute__ ((__always_inline__))
#  else
#    define KE_FORCEINLINE inline
#  endif
#endif