/**
 * @file
 * @author Max Godefroy
 * @date 04/11/2026.
 */

#include "KryneEngine/Core/Platform/Alert.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace KryneEngine::Platform
{
    namespace
    {
        eastl::wstring ToUtf16(const eastl::string_view _utf8)
        {
            eastl::wstring result;
            result.resize(MultiByteToWideChar(
                CP_UTF8,
                0,
                _utf8.data(),
                static_cast<int>(_utf8.size()),
                nullptr,
                0));
            MultiByteToWideChar(
                CP_UTF8,
                0,
                _utf8.data(),
                static_cast<int>(_utf8.size()),
                result.data(),
                static_cast<int>(result.size()));
            return result;
        }
    }

    void ShowAlert(const eastl::string_view _title, const eastl::string_view _message, const AlertType _type)
    {
        UINT icon = MB_ICONERROR;
        switch (_type)
        {
        case AlertType::Info:
            icon = MB_ICONINFORMATION;
            break;
        case AlertType::Warning:
            icon = MB_ICONWARNING;
            break;
        case AlertType::Error:
            icon = MB_ICONERROR;
            break;
        }

        const eastl::wstring title = ToUtf16(_title);
        const eastl::wstring message = ToUtf16(_message);
        MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_OK | MB_SETFOREGROUND | icon);
    }
}
