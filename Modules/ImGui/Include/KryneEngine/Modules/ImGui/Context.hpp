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
}

namespace KryneEngine::Modules::ImGui
{
    class Input;

    /**
     * @class Context
     *
     * This class represents the rendering and input handling context for ImGui (Immediate Mode GUI).
     */
    class Context
    {
    public:
        /**
         * @brief Constructs a Context object.
         *
         * This constructor initializes the ImGui context by calling ImGui::CreateContext().
         * It also sets up the ImGuiIO data structure and initializes the vertex and index dynamic buffers.
         *
         * @param _window The Window object associated with the Context.
         * @param _renderPass The RenderPassHandle object used for building the ImGui PSO.
         * @param _allocator The memory allocator instance for this context
         * @param _vsBytecode An optional argument to use if you load the vertex shader bytecode externally
         * @param _fsBytecode An optional argument to use if you load the fragment shader bytecode externally
         */
        Context(
            Window* _window,
            RenderPassHandle _renderPass,
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
        void Shutdown(Window* _window);

        /**
         * Sets up the ImGui context for a new frame.
         * Updates input and window data.
         *
         * @param _window The Window object.
         */
        void NewFrame(Window* _window);

        /**
         * @brief Prepares the rendering context for a new frame by updating the vertex and index buffers.
         *
         * @param _graphicsContext The graphics context used for rendering.
         * @param _commandList The command list used for uploading the buffers and texture regions.
         */
        void PrepareToRenderFrame(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

        /**
         * @brief Renders a frame using the provided graphics context and command list.
         *
         * This function is responsible for rendering the ImGui UI for a single frame.
         *
         * @param _graphicsContext The graphics context used for rendering.
         * @param _commandList The command list used for rendering.
         */
        void RenderFrame(GraphicsContext* _graphicsContext, CommandListHandle _commandList);

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

        Input* m_input;

        void InitPso(
            GraphicsContext* _graphicsContext,
            RenderPassHandle _renderPass,
            eastl::span<char> _externalVsBytecode,
            eastl::span<char> _externalFsBytecode);
    };
}// namespace KryneEngine