/**
 * @file
 * @author Max Godefroy
 * @date 21/07/2024.
 */

#pragma once

#include <EASTL/chrono.h>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Modules/GraphicsUtils/DynamicBuffer.hpp>
#include <imgui.h>

namespace KryneEngine
{
    class Window;
    class WindowManager;
}

namespace KryneEngine::Modules::ImGui
{
    class Input;
    class ViewportBackend;

    /**
     * @class Context
     *
     * This class represents the rendering and input handling context for ImGui (Immediate Mode GUI).
     */
    class Context
    {
        friend class ViewportBackend;

    public:
        /**
         * @brief Constructs a Context object.
         *
         * This constructor initializes the ImGui context by calling ImGui::CreateContext().
         * It also sets up the ImGuiIO data structure and initializes the vertex and index dynamic buffers.
         *
         * @param _window The Window object associated with the Context.
         * @param _targetFormat The render target texture format, used for building the ImGui PSO.
         * @param _allocator The memory allocator instance for this context
         * @param _vsBytecode An optional argument to use if you load the vertex shader bytecode externally
         * @param _fsBytecode An optional argument to use if you load the fragment shader bytecode externally
         */
        Context(
            Window* _window,
            WindowManager* _windowManager,
            GraphicsContext* _graphicsContext,
            TextureFormat _targetFormat,
            AllocatorInstance _allocator,
            eastl::span<char> _vsBytecode = {},
            eastl::span<char> _fsBytecode = {});

        ~Context();

        /**
         * @brief Shuts down the Context by releasing all allocated resources.
         *
         * This function is responsible for releasing all allocated resources such as dynamic buffers,
         * samplers, textures, descriptor sets, pipeline layout, graphics pipeline, shader modules, and the ImGui context.
         * It also unregisters input event callbacks from the Window's InputManager.
         *
         * @param _window The Window object associated with this Context, which indirectly owns the objects.
         */
        void Shutdown(WindowManager* _windowManager, GraphicsContext* _graphicsContext);

        /**
         * Sets up the ImGui context for a new frame.
         * Updates input and window data.
         *
         * @param _window The main OS window.
         * @param _graphicsContext The graphics context.
         * @param _mainSwapChain The main window's swap chain — presented alongside any viewport
         *        swap chains by #GetSwapChainsToPresent.
         */
        void NewFrame(Window* _window, GraphicsContext* _graphicsContext, SwapChainHandle _mainSwapChain);

        /**
         * @brief Prepares the rendering context for a new frame by updating the vertex and index buffers.
         *
         * @param _graphicsContext The graphics context used for rendering.
         * @param _transferEncoder The command list used for uploading the buffers and texture regions.
         */
        void PrepareToRenderFrame(GraphicsContext* _graphicsContext, TransferCommandEncoderHandle _transferEncoder);

        /**
         * @brief Renders a frame using the provided graphics context and command list.
         *
         * This function is responsible for rendering the ImGui UI for a single frame.
         *
         * @param _graphicsContext The graphics context used for rendering.
         * @param _renderEncoder The command list used for rendering.
         */
        void RenderFrame(GraphicsContext* _graphicsContext, RenderCommandEncoderHandle _renderEncoder);

        /**
         * @brief Creates / resizes / destroys the OS windows and swap chains backing Dear ImGui's
         *        secondary viewports, and records their draw commands.
         *
         * @details No-op unless `ImGuiConfigFlags_ViewportsEnable` is set. Call once per frame after the
         * main viewport's render pass has been recorded and its command list ended, then present with
         * @ref GetSwapChainsToPresent. Only records — the actual submit/present happens in
         * `GraphicsContext::EndFrame`.
         */
        void UpdateAndRenderPlatformWindows(GraphicsContext* _graphicsContext);

        /// @brief The swap chains to hand to `GraphicsContext::EndFrame` this frame — the main window's
        ///        plus one per visible secondary viewport. Valid until the next #NewFrame.
        [[nodiscard]] eastl::span<const SwapChainHandle> GetSwapChainsToPresent() const { return m_presentSwapChains; }

        /**
         * @brief A helper function to convert a texture view / sampler set into an ImTextureID.
         *
         * @param _texture The texture view handle to save
         * @param _sampler The optional sampler handle. If not provided, a default sampler will be used.
         */
        static ImTextureID ToImTextureID(TextureViewHandle _texture, SamplerHandle _sampler = {});

        /**
         * @brief A function to convert back an ImTextureID to a texture view and sampler pair.
         *
         * @param _textureId The ImTextureID to convert back to a texture view and sampler pair.
         */
        static eastl::pair<TextureViewHandle, SamplerHandle> FromImTextureID(ImTextureID _textureId);

    private:
        struct SystemTexture
        {
            TextureHandle m_texture {};
            TextureMemoryFootprint m_footprint {};
        };

        struct SystemTextureStagingBuffer
        {
            BufferHandle m_buffer {};
            size_t m_size = 0;
        };

        ImGuiContext* m_context;

        DynamicArray<SystemTextureStagingBuffer> m_systemsTexturesStagingBuffers;
        eastl::vector_map<TextureViewHandle, SystemTexture> m_systemTextures;
        SamplerHandle m_defaultSampler { GenPool::kInvalidHandle };

        DescriptorSetLayoutHandle m_descriptorSetLayout { GenPool::kInvalidHandle };
        eastl::vector<DescriptorSetHandle> m_descriptorSets;

        eastl::vector<u32> m_setIndices;
        PipelineLayoutHandle m_pipelineLayout { GenPool::kInvalidHandle };
        GraphicsPipelineHandle m_pso { GenPool::kInvalidHandle };

        static constexpr u64 kInitialSize = 1024;
        GraphicsUtils::DynamicBuffer m_dynamicVertexBuffer;
        GraphicsUtils::DynamicBuffer m_dynamicIndexBuffer;

        eastl::chrono::time_point<eastl::chrono::steady_clock> m_timePoint;

        Input* m_input = nullptr;
        ViewportBackend* m_viewportBackend = nullptr;

        TextureFormat m_targetFormat = TextureFormat::NoFormat;
        SwapChainHandle m_mainSwapChain { GenPool::kInvalidHandle };
        eastl::vector<SwapChainHandle> m_presentSwapChains;

        void InitPso(
            GraphicsContext* _graphicsContext,
            TextureFormat _targetFormat,
            eastl::span<char> _externalVsBytecode,
            eastl::span<char> _externalFsBytecode);

        /// @brief Records the draw commands of one ImDrawData into an already-open render pass.
        void RenderDrawData(
            GraphicsContext* _graphicsContext,
            RenderCommandEncoderHandle _renderEncoder,
            const ImDrawData* _drawData,
            u32 _firstVertex,
            u32 _firstIndex);

        /// @brief Base offsets of a viewport's vertices/indices in the shared dynamic buffers,
        ///        as laid out by the last #PrepareToRenderFrame.
        void GetViewportDrawOffsets(ImGuiID _viewportId, u32& _firstVertex, u32& _firstIndex) const;

        eastl::vector_map<ImGuiID, eastl::pair<u32, u32>> m_viewportDrawOffsets;
    };
}// namespace KryneEngine