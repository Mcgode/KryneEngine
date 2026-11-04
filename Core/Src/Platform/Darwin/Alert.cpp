/**
 * @file
 * @author Max Godefroy
 * @date 04/11/2026.
 */

#include "KryneEngine/Core/Platform/Alert.hpp"

#include <CoreFoundation/CoreFoundation.h>

namespace KryneEngine::Platform
{
    void ShowAlert(const eastl::string_view _title, const eastl::string_view _message, const AlertType _type)
    {
        // CFUserNotification rather than NSAlert: it doesn't need an NSApplication, and is safe off the main thread.
        const CFStringRef title = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(_title.data()),
            static_cast<CFIndex>(_title.size()),
            kCFStringEncodingUTF8,
            false);
        const CFStringRef message = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(_message.data()),
            static_cast<CFIndex>(_message.size()),
            kCFStringEncodingUTF8,
            false);

        CFOptionFlags alertLevel = kCFUserNotificationStopAlertLevel;
        switch (_type)
        {
        case AlertType::Info:
            alertLevel = kCFUserNotificationNoteAlertLevel;
            break;
        case AlertType::Warning:
            alertLevel = kCFUserNotificationCautionAlertLevel;
            break;
        case AlertType::Error:
            alertLevel = kCFUserNotificationStopAlertLevel;
            break;
        }

        CFOptionFlags response;
        CFUserNotificationDisplayAlert(
            0, // No timeout
            alertLevel,
            nullptr,
            nullptr,
            nullptr,
            title,
            message,
            nullptr, // Default "OK" button
            nullptr,
            nullptr,
            &response);

        if (message != nullptr)
        {
            CFRelease(message);
        }
        if (title != nullptr)
        {
            CFRelease(title);
        }
    }
}
