/**
 * @file
 * @author Max Godefroy
 * @date 05/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine
{
    /**
     * @brief Platform-agnostic bundle of the native OS handles backing a @ref Window.
     *
     * @details
     * This is the single seam through which platform-specific graphics code (swap chain / surface
     * creation) obtains the underlying OS window, without depending on the windowing backend (GLFW
     * today) or pulling in platform headers.
     *
     * Field contents per @ref Kind:
     * | Kind    | m_windowHandle              | m_displayHandle |
     * |---------|----------------------------|-----------------|
     * | Win32   | `HWND`                     | `nullptr`       |
     * | Cocoa   | `NSWindow*`                | `nullptr`       |
     * | Xlib    | `Window` (XID, via uintptr)| `Display*`      |
     * | Wayland | `wl_surface*`              | `wl_display*`   |
     */
    struct NativeWindowHandle
    {
        enum class Kind : u8
        {
            Unknown,
            Win32,
            Cocoa,
            Xlib,
            Wayland,
        };

        Kind m_kind = Kind::Unknown;
        void* m_windowHandle = nullptr;
        void* m_displayHandle = nullptr;
    };
}
