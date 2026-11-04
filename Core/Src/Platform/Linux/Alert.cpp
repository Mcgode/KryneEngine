/**
 * @file
 * @author Max Godefroy
 * @date 04/11/2026.
 */

#include "KryneEngine/Core/Platform/Alert.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace KryneEngine::Platform
{
    namespace
    {
        /// @brief Runs a dialog tool and waits for it to exit.
        /// @return `true` if the tool was found and displayed its dialog.
        bool RunDialogTool(const char* const* _argv)
        {
            pid_t pid;
            if (posix_spawnp(&pid, _argv[0], nullptr, nullptr, const_cast<char* const*>(_argv), environ) != 0)
            {
                return false;
            }

            int status;
            while (waitpid(pid, &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    return false;
                }
            }

            // Closing the dialog instead of pressing "OK" also returns 1 with zenity and kdialog.
            return WIFEXITED(status) && (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 1);
        }
    }

    void ShowAlert(const eastl::string_view _title, const eastl::string_view _message, const AlertType _type)
    {
        // The arguments are passed straight to exec, so no shell escaping is needed.
        const eastl::string title(_title.data(), _title.size());
        const eastl::string message(_message.data(), _message.size());

        const bool hasDisplay = getenv("DISPLAY") != nullptr || getenv("WAYLAND_DISPLAY") != nullptr;
        if (hasDisplay)
        {
            const char* zenityType = "--error";
            const char* kdialogType = "--error";
            switch (_type)
            {
            case AlertType::Info:
                zenityType = "--info";
                kdialogType = "--msgbox";
                break;
            case AlertType::Warning:
                zenityType = "--warning";
                kdialogType = "--sorry";
                break;
            case AlertType::Error:
                break;
            }

            const eastl::string zenityTitle = "--title=" + title;
            const eastl::string zenityText = "--text=" + message;
            const char* zenity[] = { "zenity", zenityType, "--no-markup", zenityTitle.c_str(), zenityText.c_str(), nullptr };
            if (RunDialogTool(zenity))
            {
                return;
            }

            const char* kdialog[] = { "kdialog", "--title", title.c_str(), kdialogType, message.c_str(), nullptr };
            if (RunDialogTool(kdialog))
            {
                return;
            }

            const char* xmessage[] = { "xmessage", "-center", "-title", title.c_str(), message.c_str(), nullptr };
            if (RunDialogTool(xmessage))
            {
                return;
            }
        }

        fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());
    }
}
