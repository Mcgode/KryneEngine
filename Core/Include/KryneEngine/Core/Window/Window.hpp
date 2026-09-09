/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include <EASTL/string_view.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Window/NativeWindowHandle.hpp"

struct GLFWwindow;

namespace KryneEngine
{
    /**
     * @brief A thin handle over an OS window.
     *
     * @details
     * A `Window` is created, owned and destroyed by the @ref WindowManager — it does not touch GLFW's
     * process-global state, drive the message pump, or own any input state. It only exposes the queries
     * an application needs to set up a swap chain and lay out its rendering. The `Window*` returned by
     * @ref WindowManager::CreateWindow is itself the stable handle used with the rest of the manager API.
     */
    class Window
    {
        friend class WindowManager;

    public:
        /// @brief Constructed by the @ref WindowManager — applications go through @ref WindowManager::CreateWindow.
        Window(GLFWwindow* _glfwWindow, AllocatorInstance _allocator);

        virtual ~Window();

        /// @brief Retrieves the native OS handles backing this window (see @ref NativeWindowHandle).
        [[nodiscard]] NativeWindowHandle GetNativeHandle() const;

        /// @brief Size of the drawable surface, in pixels (drives swap-chain / viewport dimensions).
        [[nodiscard]] uint2 GetFramebufferSize() const;
        [[nodiscard]] float2 GetDpiScale() const;

        /// @brief Window client-area position, in virtual screen coordinates (top-left corner).
        [[nodiscard]] int2 GetPosition() const;
        void SetPosition(int2 _position) const;

        /// @brief Window client-area size, in screen coordinates (may differ from @ref GetFramebufferSize on HiDPI).
        [[nodiscard]] uint2 GetSize() const;
        void SetSize(uint2 _size) const;

        [[nodiscard]] bool IsFocused() const;
        void Focus() const;

        [[nodiscard]] bool IsMinimized() const;

        void SetTitle(const eastl::string_view& _title) const;

        void Show() const;
        void Hide() const;

    private:
        AllocatorInstance m_allocator;
        GLFWwindow* m_glfwWindow;

        // WindowManager-managed state (updated from the GLFW callbacks it registers).
        uint2 m_lastFramebufferSize {};
        bool m_resizePending = false;
    };
}
