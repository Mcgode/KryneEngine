/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include <EASTL/unique_ptr.h>
#include <EASTL/vector_map.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

struct GLFWwindow;

namespace KryneEngine
{
    class GraphicsContext;
    class InputManager;

    class Window
    {
    public:

        explicit Window(const GraphicsCommon::ApplicationInfo& _appInfo, AllocatorInstance _allocator);

        virtual ~Window();

        [[nodiscard]] bool WaitForEvents();
        [[nodiscard]] GLFWwindow* GetGlfwWindow() const { return m_glfwWindow; }
        [[nodiscard]] GraphicsContext* GetGraphicsContext() const { return m_graphicsContext; }
        [[nodiscard]] InputManager* GetInputManager() const { return m_inputManager; }

        uint2 GetFramebufferSize() const;
        float2 GetDpiScale() const;

        [[nodiscard]] u32 RegisterWindowFocusEventCallback(eastl::function<void(bool)>&& _callback);
        void UnregisterWindowFocusEventCallback(u32 _id);

        [[nodiscard]] u32 RegisterDpiChangeEventCallback(eastl::function<void(const float2&)>&& _callback);
        void UnregisterDpiChangeEventCallback(u32 _id);

        [[nodiscard]] bool WasResizedThisFrame() const { return m_resizedThisFrame; }
        [[nodiscard]] bool ShouldResizeSwapChain() const { return m_resizedSwapChain; }
        void NotifySwapChainResized() { m_resizedSwapChain = true; }

    private:
        AllocatorInstance m_allocator;
        GLFWwindow* m_glfwWindow;

        GraphicsContext* m_graphicsContext;
        InputManager* m_inputManager;

        uint2 m_previousFramebufferSize;
        bool m_resizedThisFrame = false;
        bool m_resizedSwapChain = true;

        LightweightMutex m_callbackMutex;

        static void WindowFocusCallback(GLFWwindow* _window, s32 _focused);
        eastl::vector_map<u32, eastl::function<void(bool)>> m_windowFocusEventListeners;
        u32 m_windowFocusEventCounter = 0;

        static void DpiChangeCallback(GLFWwindow* _window, float _xScale, float _yScale);
        eastl::vector_map<u32, eastl::function<void(const float2&)>> m_dpiChangeEventListeners;
        u32 m_dpiChangeEventCounter = 0;

        static void ResizeCallback(GLFWwindow* _window, int _width, int _height);
    };
}


