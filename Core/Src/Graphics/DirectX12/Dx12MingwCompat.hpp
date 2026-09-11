/**
 * @file
 * @brief Fills gaps in the mingw-w64 SDK headers when cross-compiling the DirectX 12
 *        backend from macOS with Clang.
 *
 * @details
 * Include this straight after the Direct3D 12 / DXGI headers.
 *
 * Two unrelated problems are handled:
 *
 *  1. `__uuidof(ID3D12*)` - Microsoft's DirectX-Headers do not attach a compiler
 *     GUID to their interface types. `dxguids/dxguids.h` provides both a portable
 *     `uuidof<T>()` and, on mingw, the `__CRT_UUID_DECL` declarations that make the
 *     `__uuidof` / `IID_PPV_ARGS` intrinsics link.
 *
 *  2. A handful of debug-only GUID constants are simply missing from the mingw SDK
 *     (`WKPDID_D3DDebugObjectNameW`, `DXGI_DEBUG_D3D12`). Their values are stable and
 *     defined here with internal linkage.
 */

#pragma once

#if defined(__MINGW32__)

#include <dxguids/dxguids.h>

#include <guiddef.h>

#if !defined(WKPDID_D3DDebugObjectNameW)
// {4CCA5FD8-921F-42C8-8566-70CAF2A9B741}
inline constexpr GUID WKPDID_D3DDebugObjectNameW =
    { 0x4cca5fd8, 0x921f, 0x42c8, { 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41 } };
#endif

#if !defined(DXGI_DEBUG_D3D12)
// {CF59A98C-A950-4326-91EF-9BBAA17BFD95}
inline constexpr GUID DXGI_DEBUG_D3D12 =
    { 0xcf59a98c, 0xa950, 0x4326, { 0x91, 0xef, 0x9b, 0xba, 0xa1, 0x7b, 0xfd, 0x95 } };
#endif

#endif // __MINGW32__
