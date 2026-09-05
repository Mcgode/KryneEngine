/**
 * @file
 * @author Max Godefroy
 * @date 05/09/2026.
 */

#pragma once

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
     * Field contents per platform:
     * | Platform | m_windowHandle              | m_displayHandle |
     * |----------|----------------------------|-----------------|
     * | Windows  | `HWND`                     | `nullptr`       |
     * | macOS    | `NSWindow*`                | `nullptr`       |
     * | X11      | `Window` (XID, via uintptr)| `Display*`      |
     * | Wayland  | `wl_surface*`              | `wl_display*`   |
     */
    struct NativeWindowHandle
    {
        void* m_windowHandle = nullptr;
        void* m_displayHandle = nullptr;
    };
}
