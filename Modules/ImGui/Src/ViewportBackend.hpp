/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"


#include <EASTL/vector.h>
#include <KryneEngine/Core/Graphics/Enums.hpp>
#include <KryneEngine/Core/Graphics/Handles.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Window/WindowManager.hpp>
#include <imgui.h>

namespace KryneEngine
{
    class GraphicsContext;
    class Window;
    class WindowManager;
}

namespace KryneEngine::Modules::ImGui
{
    class Context;

    /**
     * @brief Dear ImGui multi-viewport backend — wires `ImGui::GetPlatformIO()` to the engine's
     *        @ref WindowManager (platform side) and @ref Context / @ref GraphicsContext (renderer side).
     *
     * @details Inert unless the ImGui context has `ImGuiConfigFlags_ViewportsEnable`. One instance per
     * @ref Context; recovered from `ImGui::GetIO().BackendPlatformUserData` inside the C-style callbacks.
     */
    class ViewportBackend final
    {
    public:
        ViewportBackend(
            Context* _context,
            Window* _mainWindow,
            WindowManager* _windowManager,
            GraphicsContext* _graphicsContext,
            TextureFormat _targetFormat,
            AllocatorInstance _allocator);
        ~ViewportBackend();

        /// @brief Refreshes `ImGui::GetPlatformIO().Monitors` + the main viewport geometry. Call in NewFrame.
        void NewFrame() const;

        /// @brief `ImGui::UpdatePlatformWindows()` + `RenderPlatformWindowsDefault()`, collecting the
        ///        secondary-viewport swap chains to present.
        void UpdateAndRenderPlatformWindows(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

        [[nodiscard]] eastl::span<const SwapChainHandle> GetSecondarySwapChains() const { return m_secondarySwapChains; }

    private:
        // --- WindowManager per-window OS events (bound in the ctor via SetWindowEventCallbacks) ---
        void OnWindowFocus(Window* _window, bool _focused);
        void OnWindowMove(Window* _window, int2 _position);
        void OnWindowResize(Window* _window, uint2 _size);
        void OnWindowCloseRequest(Window* _window);
        void OnCursorEnter(Window* _window, bool _entered);
        void OnDpiChange(Window* _window, float2 _dpiScale);

        struct ViewportData
        {
            Window* m_window = nullptr;
            bool m_ownedByBackend = false;

            SwapChainHandle m_swapChain { GenPool::kInvalidHandle };
            DynamicArray<RenderTargetViewHandle> m_renderTargetViews;
            DynamicArray<RenderPassHandle> m_renderPasses;

            u32 m_firstVertex = 0;
            u32 m_firstIndex = 0;
        };

        Context* m_context;
        Window* m_mainWindow;
        WindowManager* m_windowManager;
        GraphicsContext* m_graphicsContext;
        TextureFormat m_targetFormat;
        AllocatorInstance m_allocator;

        eastl::vector<SwapChainHandle> m_secondarySwapChains;

        [[nodiscard]] static ImGuiViewport* FindViewport(const Window* _window);
        [[nodiscard]] static ViewportData* Data(const ImGuiViewport* _viewport);

        void CreateRendererWindow(const ImGuiViewport* _viewport) const;
        void DestroyRendererWindow(const ImGuiViewport* _viewport) const;
        void RenderRendererWindow(
            const ImGuiViewport* _viewport, GraphicsContext* _graphicsContext, CommandListHandle _commandList) const;

        friend struct ViewportCallbacks;
    };
} // namespace KryneEngine::Modules::ImGui
