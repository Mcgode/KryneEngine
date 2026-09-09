/**
 * @file
 * @author Max Godefroy
 * @date 09/09/2026.
 */

#include "ViewportBackend.hpp"

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/RenderPass.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Window/Window.hpp>

#include "KryneEngine/Modules/ImGui/Context.hpp"

namespace KryneEngine::Modules::ImGui
{
    namespace
    {
        ViewportBackend* Backend()
        {
            return static_cast<ViewportBackend*>(::ImGui::GetIO().BackendPlatformUserData);
        }
    }

    // C-style ImGui platform / renderer callbacks. Friend of ViewportBackend.
    struct ViewportCallbacks
    {
        using VD = ViewportBackend::ViewportData;

        static VD* Data(const ImGuiViewport* _vp) { return static_cast<VD*>(_vp->PlatformUserData); }

        // -------- platform --------
        static void Platform_CreateWindow(ImGuiViewport* _vp)
        {
            const ViewportBackend* backend = Backend();

            auto* vd = backend->m_allocator.New<VD>();
            vd->m_ownedByBackend = true;

            GraphicsCommon::DisplayOptions opts {};
            opts.m_width = static_cast<u16>(eastl::max(1.f, _vp->Size.x));
            opts.m_height = static_cast<u16>(eastl::max(1.f, _vp->Size.y));
            opts.m_resizableWindow = true;
            opts.m_decorated = (_vp->Flags & ImGuiViewportFlags_NoDecoration) == 0;

            vd->m_window = backend->m_windowManager->CreateWindow("ImGui viewport", opts, false);

            _vp->PlatformUserData = vd;
            _vp->PlatformHandle = vd->m_window;
            _vp->PlatformHandleRaw = vd->m_window->GetNativeHandle().m_windowHandle;
        }

        static void Platform_DestroyWindow(ImGuiViewport* _vp)
        {
            const ViewportBackend* backend = Backend();
            VD* vd = Data(_vp);
            if (vd == nullptr)
                return;

            if (vd->m_ownedByBackend && vd->m_window != nullptr)
                backend->m_windowManager->DestroyWindow(vd->m_window);

            backend->m_allocator.Delete(vd);
            _vp->PlatformUserData = nullptr;
            _vp->PlatformHandle = nullptr;
            _vp->PlatformHandleRaw = nullptr;
        }

        static void Platform_ShowWindow(ImGuiViewport* _vp) { Data(_vp)->m_window->Show(); }

        static void Platform_SetWindowPos(ImGuiViewport* _vp, const ImVec2 _pos)
        {
            Data(_vp)->m_window->SetPosition({ static_cast<s32>(_pos.x), static_cast<s32>(_pos.y) });
        }

        static ImVec2 Platform_GetWindowPos(ImGuiViewport* _vp)
        {
            const int2 p = Data(_vp)->m_window->GetPosition();
            return { static_cast<float>(p.x), static_cast<float>(p.y) };
        }

        static void Platform_SetWindowSize(ImGuiViewport* _vp, const ImVec2 _size)
        {
            Data(_vp)->m_window->SetSize({ static_cast<u32>(_size.x), static_cast<u32>(_size.y) });
        }

        static ImVec2 Platform_GetWindowSize(ImGuiViewport* _vp)
        {
            const uint2 s = Data(_vp)->m_window->GetSize();
            return { static_cast<float>(s.x), static_cast<float>(s.y) };
        }

        static void Platform_SetWindowFocus(ImGuiViewport* _vp) { Data(_vp)->m_window->Focus(); }
        static bool Platform_GetWindowFocus(ImGuiViewport* _vp) { return Data(_vp)->m_window->IsFocused(); }
        static bool Platform_GetWindowMinimized(ImGuiViewport* _vp) { return Data(_vp)->m_window->IsMinimized(); }

        static void Platform_SetWindowTitle(ImGuiViewport* _vp, const char* _title)
        {
            Data(_vp)->m_window->SetTitle(_title);
        }

        static float Platform_GetWindowDpiScale(ImGuiViewport* _vp)
        {
            return Data(_vp)->m_window->GetDpiScale().x;
        }

        // -------- renderer --------
        static void Renderer_CreateWindow(ImGuiViewport* _vp) { Backend()->CreateRendererWindow(_vp); }
        static void Renderer_DestroyWindow(ImGuiViewport* _vp) { Backend()->DestroyRendererWindow(_vp); }

        static void Renderer_SetWindowSize(ImGuiViewport* _vp, ImVec2)
        {
            const ViewportBackend* backend = Backend();
            const VD* vd = Data(_vp);
            if (vd != nullptr && vd->m_swapChain != GenPool::kInvalidHandle)
                backend->m_graphicsContext->ResizeSwapChain(vd->m_swapChain, vd->m_window->GetFramebufferSize());
        }

        struct RenderArgs
        {
            GraphicsContext* m_graphicsContext;
            CommandListHandle m_commandList;
        };

        static void Renderer_RenderWindow(ImGuiViewport* _vp, void* _renderArg)
        {
            const auto* renderArgs = static_cast<const RenderArgs*>(_renderArg);
            Backend()->RenderRendererWindow(_vp, renderArgs->m_graphicsContext, renderArgs->m_commandList);
        }

        static void Renderer_SwapBuffers(ImGuiViewport* _vp, void*)
        {
            VD* vd = Data(_vp);
            if (vd != nullptr && vd->m_swapChain != GenPool::kInvalidHandle)
                Backend()->m_secondarySwapChains.push_back(vd->m_swapChain);
        }
    };

    ViewportBackend::ViewportData* ViewportBackend::Data(const ImGuiViewport* _viewport)
    {
        return static_cast<ViewportData*>(_viewport->PlatformUserData);
    }

    ViewportBackend::ViewportBackend(
        Context* _context,
        Window* _mainWindow,
        WindowManager* _windowManager,
        GraphicsContext* _graphicsContext,
        const TextureFormat _targetFormat,
        const AllocatorInstance _allocator)
            : m_context(_context)
            , m_mainWindow(_mainWindow)
            , m_windowManager(_windowManager)
            , m_graphicsContext(_graphicsContext)
            , m_targetFormat(_targetFormat)
            , m_allocator(_allocator)
            , m_secondarySwapChains(_allocator)
    {
        ImGuiIO& io = ::ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_RendererHasViewports;
        io.BackendPlatformUserData = this;

        ImGuiPlatformIO& pio = ::ImGui::GetPlatformIO();
        pio.Platform_CreateWindow = &ViewportCallbacks::Platform_CreateWindow;
        pio.Platform_DestroyWindow = &ViewportCallbacks::Platform_DestroyWindow;
        pio.Platform_ShowWindow = &ViewportCallbacks::Platform_ShowWindow;
        pio.Platform_SetWindowPos = &ViewportCallbacks::Platform_SetWindowPos;
        pio.Platform_GetWindowPos = &ViewportCallbacks::Platform_GetWindowPos;
        pio.Platform_SetWindowSize = &ViewportCallbacks::Platform_SetWindowSize;
        pio.Platform_GetWindowSize = &ViewportCallbacks::Platform_GetWindowSize;
        pio.Platform_SetWindowFocus = &ViewportCallbacks::Platform_SetWindowFocus;
        pio.Platform_GetWindowFocus = &ViewportCallbacks::Platform_GetWindowFocus;
        pio.Platform_GetWindowMinimized = &ViewportCallbacks::Platform_GetWindowMinimized;
        pio.Platform_SetWindowTitle = &ViewportCallbacks::Platform_SetWindowTitle;
        pio.Platform_GetWindowDpiScale = &ViewportCallbacks::Platform_GetWindowDpiScale;

        pio.Renderer_CreateWindow = &ViewportCallbacks::Renderer_CreateWindow;
        pio.Renderer_DestroyWindow = &ViewportCallbacks::Renderer_DestroyWindow;
        pio.Renderer_SetWindowSize = &ViewportCallbacks::Renderer_SetWindowSize;
        pio.Renderer_RenderWindow = &ViewportCallbacks::Renderer_RenderWindow;
        pio.Renderer_SwapBuffers = &ViewportCallbacks::Renderer_SwapBuffers;

        ImGuiViewport* main = ::ImGui::GetMainViewport();
        auto* mainData = m_allocator.New<ViewportData>();
        mainData->m_window = m_mainWindow;
        mainData->m_ownedByBackend = false;
        main->PlatformUserData = mainData;
        main->PlatformHandle = m_mainWindow;
        main->PlatformHandleRaw = m_mainWindow->GetNativeHandle().m_windowHandle;

        NewFrame();

        m_windowManager->SetWindowEventCallbacks({
            .m_onFocus = [this](Window* _w, const bool _focused) { OnWindowFocus(_w, _focused); },
            .m_onMove = [this](Window* _w, const int2 _pos) { OnWindowMove(_w, _pos); },
            .m_onResize = [this](Window* _w, const uint2 _size) { OnWindowResize(_w, _size); },
            .m_onCloseRequest = [this](Window* _w) { OnWindowCloseRequest(_w); },
            .m_onCursorEnter = [this](Window* _w, const bool _entered) { OnCursorEnter(_w, _entered); },
            .m_onDpiChange = [this](Window* _w, const float2 _scale) { OnDpiChange(_w, _scale); },
        });
    }

    ViewportBackend::~ViewportBackend()
    {
        m_windowManager->SetWindowEventCallbacks({});

        ::ImGui::DestroyPlatformWindows();

        ImGuiViewport* main = ::ImGui::GetMainViewport();
        if (main->PlatformUserData != nullptr)
        {
            m_allocator.Delete(static_cast<ViewportData*>(main->PlatformUserData));
            main->PlatformUserData = nullptr;
            main->PlatformHandle = nullptr;
            main->PlatformHandleRaw = nullptr;
        }

        ImGuiIO& io = ::ImGui::GetIO();
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_RendererHasViewports);
    }

    void ViewportBackend::NewFrame() const
    {
        ImGuiPlatformIO& pio = ::ImGui::GetPlatformIO();
        const eastl::span<const MonitorInfo> monitors = m_windowManager->GetMonitors();

        pio.Monitors.resize(0);
        for (const MonitorInfo& monitor : monitors)
        {
            ImGuiPlatformMonitor imMonitor;
            imMonitor.MainPos = ImVec2(static_cast<float>(monitor.m_position.x), static_cast<float>(monitor.m_position.y));
            imMonitor.MainSize = ImVec2(static_cast<float>(monitor.m_size.x), static_cast<float>(monitor.m_size.y));
            imMonitor.WorkPos = ImVec2(static_cast<float>(monitor.m_workAreaPosition.x), static_cast<float>(monitor.m_workAreaPosition.y));
            imMonitor.WorkSize = ImVec2(static_cast<float>(monitor.m_workAreaSize.x), static_cast<float>(monitor.m_workAreaSize.y));
            imMonitor.DpiScale = monitor.m_dpiScale;
            imMonitor.PlatformHandle = nullptr;
            pio.Monitors.push_back(imMonitor);
        }

        ImGuiViewport* main = ::ImGui::GetMainViewport();
        const int2 pos = m_mainWindow->GetPosition();
        const uint2 size = m_mainWindow->GetSize();
        main->Pos = ImVec2(static_cast<float>(pos.x), static_cast<float>(pos.y));
        main->Size = ImVec2(static_cast<float>(size.x), static_cast<float>(size.y));
    }

    void ViewportBackend::UpdateAndRenderPlatformWindows(
        GraphicsContext* _graphicsContext,
        CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("ImGui::ViewportBackend::UpdateAndRenderPlatformWindows");

        m_secondarySwapChains.clear();

        const ImGuiIO& io = ::ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
            return;

        ::ImGui::UpdatePlatformWindows();
        ViewportCallbacks::RenderArgs renderArgs { .m_graphicsContext = _graphicsContext, .m_commandList = _commandList };
        ::ImGui::RenderPlatformWindowsDefault(nullptr, &renderArgs);
    }

    ImGuiViewport* ViewportBackend::FindViewport(const Window* _window)
    {
        for (ImGuiViewport* vp : ::ImGui::GetPlatformIO().Viewports)
        {
            const auto* vd = static_cast<const ViewportData*>(vp->PlatformUserData);
            if (vd != nullptr && vd->m_window == _window)
                return vp;
        }
        return nullptr;
    }

    void ViewportBackend::CreateRendererWindow(const ImGuiViewport* _viewport) const
    {
        KE_ZoneScopedFunction("ImGui::ViewportBackend::CreateRendererWindow");

        ViewportData* vd = Data(_viewport);
        const Window* window = vd->m_window;

        vd->m_swapChain = m_graphicsContext->CreateSwapChain({
            .m_nativeWindow = window->GetNativeHandle(),
            .m_dimensions = window->GetFramebufferSize(),
            .m_displayOptions = {},
        });

        KE_ASSERT_MSG(
            m_graphicsContext->GetSwapChainFormat(vd->m_swapChain) == m_targetFormat,
            "ImGui viewport swap chain format differs from the main context format");

        const bool clear = (_viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0;
        const u8 imageCount = m_graphicsContext->GetFrameContextCount();
        vd->m_renderTargetViews.Resize(imageCount);
        vd->m_renderPasses.Resize(imageCount);
        for (u8 i = 0; i < imageCount; i++)
        {
            vd->m_renderTargetViews.Init(i, m_graphicsContext->GetSwapChainRenderTargetView(vd->m_swapChain, i));

            RenderPassDesc desc;
            desc.m_colorAttachments.push_back(RenderPassDesc::Attachment {
                .m_loadOperation = clear
                    ? RenderPassDesc::Attachment::LoadOperation::Clear
                    : RenderPassDesc::Attachment::LoadOperation::DontCare,
                .m_storeOperation = RenderPassDesc::Attachment::StoreOperation::Store,
                .m_initialLayout = TextureLayout::Unknown,
                .m_finalLayout = TextureLayout::Present,
                .m_rtv = vd->m_renderTargetViews[i],
                .m_clearColor = float4(0.f, 0.f, 0.f, 1.f),
            });
            vd->m_renderPasses.Init(i, m_graphicsContext->CreateRenderPass(desc));
        }
    }

    void ViewportBackend::DestroyRendererWindow(const ImGuiViewport* _viewport) const
    {
        KE_ZoneScopedFunction("ImGui::ViewportBackend::DestroyRendererWindow");

        ViewportData* vd = Data(_viewport);
        if (vd == nullptr)
            return;

        m_graphicsContext->WaitForLastFrame();

        for (const RenderPassHandle pass : vd->m_renderPasses)
            m_graphicsContext->DestroyRenderPass(pass);
        vd->m_renderPasses.Clear();
        vd->m_renderTargetViews.Clear();

        if (vd->m_swapChain != GenPool::kInvalidHandle)
            m_graphicsContext->DestroySwapChain(vd->m_swapChain);
        vd->m_swapChain = { GenPool::kInvalidHandle };
    }

    void ViewportBackend::RenderRendererWindow(
        const ImGuiViewport* _viewport,
        GraphicsContext* _graphicsContext,
        CommandListHandle _commandList) const
    {
        KE_ZoneScopedFunction("ImGui::ViewportBackend::RenderRendererWindow");

        const ViewportData* vd = Data(_viewport);
        if (vd == nullptr || vd->m_swapChain == GenPool::kInvalidHandle)
            return;

        // The swap chain recreation rotates fresh render target views in over the next few frames.
        bool anyRtvChanged = false;
        for (size_t i = 0; i < vd->m_renderTargetViews.Size(); i++)
            anyRtvChanged |= vd->m_renderTargetViews[i] != _graphicsContext->GetSwapChainRenderTargetView(vd->m_swapChain, i);

        if (anyRtvChanged)
        {
            _graphicsContext->WaitForLastFrame();
            const bool clear = (_viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0;
            for (size_t i = 0; i < vd->m_renderTargetViews.Size(); i++)
            {
                const RenderTargetViewHandle rtv = _graphicsContext->GetSwapChainRenderTargetView(vd->m_swapChain, i);
                if (vd->m_renderTargetViews[i] == rtv)
                    continue;
                vd->m_renderTargetViews[i] = rtv;
                _graphicsContext->DestroyRenderPass(vd->m_renderPasses[i]);

                RenderPassDesc desc;
                desc.m_colorAttachments.push_back(RenderPassDesc::Attachment {
                    clear ? RenderPassDesc::Attachment::LoadOperation::Clear
                          : RenderPassDesc::Attachment::LoadOperation::DontCare,
                    RenderPassDesc::Attachment::StoreOperation::Store,
                    TextureLayout::Unknown,
                    TextureLayout::Present,
                    rtv,
                    float4(0.f, 0.f, 0.f, 1.f),
                });
                vd->m_renderPasses[i] = _graphicsContext->CreateRenderPass(desc);
            }
        }

        u32 firstVertex = 0;
        u32 firstIndex = 0;
        m_context->GetViewportDrawOffsets(_viewport->ID, firstVertex, firstIndex);

        const u8 imageIndex = static_cast<u8>(_graphicsContext->GetSwapChainCurrentImageIndex(vd->m_swapChain));

        const RenderCommandEncoderHandle encoder = _graphicsContext->BeginRenderPass(
            _commandList,
            vd->m_renderPasses[imageIndex],
            {},
            "ImGui viewport");
        m_context->RenderDrawData(_graphicsContext, encoder, _viewport->DrawData, firstVertex, firstIndex);
        _graphicsContext->EndRenderPass(encoder);
    }

    // ---- WindowManager per-window OS events ----

    void ViewportBackend::OnWindowFocus(Window*, bool _focused)
    {
        ::ImGui::GetIO().AddFocusEvent(_focused);
    }

    void ViewportBackend::OnWindowMove(Window* _window, int2)
    {
        if (ImGuiViewport* vp = FindViewport(_window))
            vp->PlatformRequestMove = true;
    }

    void ViewportBackend::OnWindowResize(Window* _window, uint2)
    {
        if (ImGuiViewport* vp = FindViewport(_window))
            vp->PlatformRequestResize = true;
    }

    void ViewportBackend::OnWindowCloseRequest(Window* _window)
    {
        if (ImGuiViewport* vp = FindViewport(_window))
            vp->PlatformRequestClose = true;
    }

    void ViewportBackend::OnCursorEnter(Window* _window, bool _entered)
    {
        ImGuiIO& io = ::ImGui::GetIO();
        if ((io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport) == 0)
            return;

        if (_entered)
        {
            if (const ImGuiViewport* vp = FindViewport(_window))
                io.AddMouseViewportEvent(vp->ID);
        }
        else
        {
            io.AddMouseViewportEvent(0);
        }
    }

    void ViewportBackend::OnDpiChange(Window* _window, float2 _dpiScale)
    {
        if (_window == m_mainWindow)
            ::ImGui::GetIO().DisplayFramebufferScale = { _dpiScale.x, _dpiScale.y };
    }
} // namespace KryneEngine::Modules::ImGui
