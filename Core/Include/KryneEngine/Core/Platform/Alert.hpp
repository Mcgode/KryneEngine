/**
 * @file
 * @author Max Godefroy
 * @date 04/11/2026.
 */

#pragma once

#include <EASTL/string_view.h>

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine::Platform
{
    /**
     * @brief The severity of an alert, which selects the icon displayed next to its message.
     */
    enum class AlertType: u8
    {
        Info,
        Warning,
        Error,
    };

    /**
     * @brief Displays a native modal alert window with a single "OK" button, and waits for the user to dismiss it.
     *
     * @details
     * Intended to report problems that prevent the application from running (e.g. the host machine not supporting
     * the required graphics features), so it doesn't depend on any engine window or graphics context, and can be
     * called from any thread.
     *
     * - Windows: a `MessageBox`.
     * - macOS: a `CFUserNotification` alert.
     * - Linux: the first of `zenity`, `kdialog` or `xmessage` that is available. Without a display server or any
     *   of these tools, the alert is written to the standard error output instead.
     *
     * @param _title The title of the alert window.
     * @param _message The message to display, UTF-8 encoded.
     * @param _type The severity of the alert.
     */
    void ShowAlert(eastl::string_view _title, eastl::string_view _message, AlertType _type = AlertType::Error);
}
