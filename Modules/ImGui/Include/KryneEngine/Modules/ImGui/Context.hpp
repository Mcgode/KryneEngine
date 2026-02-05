/**
 * @file
 * @author Max Godefroy
 * @date 21/07/2024.
 */

#pragma once

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include <EASTL/chrono.h>
#include <EASTL/unique_ptr.h>
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
         */
        Context(Window* _window, RenderPassHandle _renderPass, AllocatorInstance _allocator);

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
         * @param _commandList The command list.
         */
        void NewFrame(Window* _window);

        /**
         * @brief Prepares the rendering context for a new frame by updating the vertex and index buffers.
         *
         * @param _graphicsContext The graphics context used for rendering.
         * @param _commandList The command list used for uploading the buffers.
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

    private:
        struct StagingData {
            u8* m_data = nullptr;
            TextureCreateDesc m_fontsTextureDesc {};
            u64 m_stagingFrame = 0;
        };

        TextureMemoryFootprint m_fontsMemoryFootprint;

        ImGuiContext* m_context;
        StagingData* m_stagingData = nullptr;
        BufferHandle m_fontsStagingHandle { GenPool::kInvalidHandle };
        TextureHandle m_fontsTextureHandle { GenPool::kInvalidHandle };
        TextureViewHandle m_fontTextureViewHandle{ GenPool::kInvalidHandle };
        SamplerHandle m_fontSamplerHandle { GenPool::kInvalidHandle };

        DescriptorSetLayoutHandle m_fontDescriptorSetLayout { GenPool::kInvalidHandle };
        DescriptorSetHandle m_fontDescriptorSet { GenPool::kInvalidHandle };

        eastl::vector<char> m_vsBytecode {};
        eastl::vector<char> m_fsBytecode {};
        ShaderModuleHandle m_vsModule { GenPool::kInvalidHandle };
        ShaderModuleHandle m_fsModule { GenPool::kInvalidHandle };
        eastl::vector<u32> m_setIndices;
        PipelineLayoutHandle m_pipelineLayout { GenPool::kInvalidHandle };
        GraphicsPipelineHandle m_pso { GenPool::kInvalidHandle };

        static constexpr u64 kInitialSize = 1024;
        GraphicsUtils::DynamicBuffer m_dynamicVertexBuffer;
        GraphicsUtils::DynamicBuffer m_dynamicIndexBuffer;

        eastl::chrono::time_point<eastl::chrono::steady_clock> m_timePoint;

        Input* m_input;

        void _InitPso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass);
    };
}// namespace KryneEngine