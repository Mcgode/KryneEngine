/**
 * @file
 * @author Max Godefroy
 * @date 21/07/2024.
 */

#include "KryneEngine/Modules/ImGui/Context.hpp"

#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include <KryneEngine/Core/Common/Utils/Alignment.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <fstream>
#include <imgui_internal.h>

#include "Input.hpp"

namespace KryneEngine::Modules::ImGui
{
    struct VertexEntry
    {
        float2 m_position;
        float2 m_uv;
        u32 m_color;
    };

    struct PushConstants
    {
        float2 m_scale;
        float2 m_translate;
    };

    Context::Context(Window* _window, RenderPassHandle _renderPass, AllocatorInstance _allocator)
        : m_vsBytecode(_allocator)
        , m_fsBytecode(_allocator)
        , m_setIndices(_allocator)
        , m_dynamicVertexBuffer(_allocator)
        , m_dynamicIndexBuffer(_allocator)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextContext");

        m_context = ::ImGui::CreateContext();

        GraphicsContext* graphicsContext = _window->GetGraphicsContext();

        ImGuiIO& io = ::ImGui::GetIO();
        io.BackendRendererUserData = nullptr;
        io.BackendRendererName = "KryneEngineGraphics";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        {
            const BufferCreateDesc bufferCreateDesc{
                .m_desc = {
                    .m_size = kInitialSize * sizeof(VertexEntry),
#if !defined(KE_FINAL)
                    .m_debugName { "ImGuiContext/DynamicVertexBuffer", _allocator }
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType
                           | MemoryUsage::VertexBuffer
                           | MemoryUsage::TransferDstBuffer,
            };
            m_dynamicVertexBuffer.Init(
                graphicsContext,
                bufferCreateDesc,
                graphicsContext->GetFrameContextCount());
        }

        {
            const BufferCreateDesc bufferCreateDesc{
                .m_desc = {
                    .m_size = kInitialSize * sizeof(u32),
#if !defined(KE_FINAL)
                    .m_debugName { "ImGuiContext/DynamicIndexBuffer", _allocator }
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType
                           | MemoryUsage::IndexBuffer
                           | MemoryUsage::TransferDstBuffer,
            };
            m_dynamicIndexBuffer.Init(
                graphicsContext,
                bufferCreateDesc,
                graphicsContext->GetFrameContextCount());
        }

        m_input = _allocator.New<Input>(_window);

        _InitPso(graphicsContext, _renderPass);

        m_timePoint = eastl::chrono::steady_clock::now();
    }

    Context::~Context()
    {
        KE_ASSERT_MSG(m_context == nullptr, "ImGui module was not shut down");
    }

    void Context::Shutdown(Window* _window)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextShutdown");

        GraphicsContext* graphicsContext = _window->GetGraphicsContext();

        m_dynamicIndexBuffer.Destroy(graphicsContext);
        m_dynamicVertexBuffer.Destroy(graphicsContext);

        if (m_fontSamplerHandle != GenPool::kInvalidHandle)
        {
            graphicsContext->DestroySampler(m_fontSamplerHandle);
        }

        if (m_fontTextureViewHandle != GenPool::kInvalidHandle)
        {
            graphicsContext->DestroyTextureView(m_fontTextureViewHandle);
        }

        if (m_fontsTextureHandle != GenPool::kInvalidHandle)
        {
            graphicsContext->DestroyTexture(m_fontsTextureHandle);
        }

        if (m_fontsStagingHandle != GenPool::kInvalidHandle)
        {
            graphicsContext->DestroyBuffer(m_fontsStagingHandle);
        }

        {
            graphicsContext->DestroyGraphicsPipeline(m_pso);
            graphicsContext->DestroyPipelineLayout(m_pipelineLayout);
            graphicsContext->DestroyDescriptorSet(m_fontDescriptorSet);
            graphicsContext->DestroyDescriptorSetLayout(m_fontDescriptorSetLayout);
            graphicsContext->FreeShaderModule(m_fsModule);
            graphicsContext->FreeShaderModule(m_vsModule);
        }

        // Unregister input callbacks.
        m_input->Shutdown(_window);
        m_vsBytecode.get_allocator().Delete(m_input);

        ::ImGui::DestroyContext(m_context);
        m_context = nullptr;
    }

    void Context::NewFrame(Window* _window)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextNewFrame");

        ::ImGui::SetCurrentContext(m_context);

        ImGuiIO& io = ::ImGui::GetIO();

        {
            auto* window = _window->GetGlfwWindow();

            int x, y;
            glfwGetWindowSize(window, &x, &y);
            io.DisplaySize = ImVec2(float(x), float(y));

            if (x > 0 && y > 0)
            {
                int displayW, displayH;
                glfwGetFramebufferSize(window, &displayW, &displayH);
                io.DisplayFramebufferScale =
                    ImVec2(float(displayW) / io.DisplaySize.x, float(displayH) / io.DisplaySize.y);
            }
        }

        GraphicsContext* graphicsContext = _window->GetGraphicsContext();

        if (m_fontsTextureHandle == GenPool::kInvalidHandle)
        {
            u8* data;
            s32 w, h;
            io.Fonts->GetTexDataAsAlpha8(&data, &w, &h);

            TextureDesc fontsTextureDesc {
                .m_dimensions = {w, h, 1},
                .m_format = TextureFormat::R8_UNorm,
                .m_arraySize = 1,
                .m_type = TextureTypes::Single2D,
                .m_mipCount = 1,
#if !defined(KE_FINAL)
                .m_debugName = "ImGui/FontTexture"
#endif
            };

            m_fontsMemoryFootprint = graphicsContext->FetchTextureSubResourcesMemoryFootprints(fontsTextureDesc).front();
            const TextureCreateDesc textureCreateDesc {
                .m_desc = fontsTextureDesc,
                .m_footprintPerSubResource = { &m_fontsMemoryFootprint, 1 },
                .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage | MemoryUsage::SampledImage,
            };

            m_stagingData = m_vsBytecode.get_allocator().New<StagingData>(data, textureCreateDesc, graphicsContext->GetFrameId());

            m_fontsStagingHandle = graphicsContext->CreateStagingBuffer(
                fontsTextureDesc,
                textureCreateDesc.m_footprintPerSubResource);
            m_fontsTextureHandle = graphicsContext->CreateTexture(textureCreateDesc);

            {
                // Set up font srv
                const TextureViewDesc srvDesc {
                    .m_texture = m_fontsTextureHandle,
                    .m_componentsMapping = {
                        TextureComponentMapping::Red,
                        TextureComponentMapping::Red,
                        TextureComponentMapping::Red,
                        TextureComponentMapping::Red,
                    },
                    .m_format = textureCreateDesc.m_desc.m_format,
#if !defined(KE_FINAL)
                    .m_debugName = textureCreateDesc.m_desc.m_debugName + "View",
#endif
                };
                m_fontTextureViewHandle = graphicsContext->CreateTextureView(srvDesc);

                // Set up font sampler
                const SamplerDesc samplerDesc { // Default sampler works great for us
#if !defined(KE_FINAL)
                    .m_debugName = textureCreateDesc.m_desc.m_debugName + "Sampler",
#endif
                };
                m_fontSamplerHandle = graphicsContext->CreateSampler(samplerDesc);

                // Set font descriptor set values
                const DescriptorSetWriteInfo::DescriptorData fontTextureData[] = {
                    {
                        .m_textureLayout = TextureLayout::ShaderResource,
                        .m_handle = m_fontTextureViewHandle.m_handle
                    }
                };
                const DescriptorSetWriteInfo::DescriptorData fontSamplerData[] = {
                    {
                        .m_handle = m_fontSamplerHandle.m_handle
                    }
                };
                DescriptorSetWriteInfo writeInfo[2] = {
                    {
                        .m_index = m_setIndices[0],
                        .m_descriptorData = fontTextureData,
                    },
                    {
                        .m_index = m_setIndices[1],
                        .m_descriptorData = fontSamplerData,
                    }
                };
                graphicsContext->UpdateDescriptorSet(m_fontDescriptorSet, writeInfo, false);
            }

            io.Fonts->SetTexID(reinterpret_cast<void*>(static_cast<u32>(m_fontTextureViewHandle.m_handle)));
        }

        if (m_stagingData != nullptr && graphicsContext->IsFrameExecuted(m_stagingData->m_stagingFrame))
        {
            graphicsContext->DestroyBuffer(m_fontsStagingHandle);
            m_fontsStagingHandle = GenPool::kInvalidHandle;

            m_vsBytecode.get_allocator().Delete(m_stagingData);
            m_stagingData = nullptr;
        }

        const auto currentTimePoint =  eastl::chrono::steady_clock::now();
        const eastl::chrono::duration<double> interval = currentTimePoint - m_timePoint;
        m_timePoint = currentTimePoint;

        io.DeltaTime = static_cast<float>(interval.count());

        ::ImGui::NewFrame();
    }

    void Context::PrepareToRenderFrame(GraphicsContext* _graphicsContext, CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextPrepareToRenderFrame");

        ::ImGui::Render();

        if (m_stagingData != nullptr && m_stagingData->m_stagingFrame == _graphicsContext->GetFrameId())
        {
            {
                BufferMemoryBarrier stagingBufferBarrier {
                    .m_stagesSrc = BarrierSyncStageFlags::None,
                    .m_stagesDst = BarrierSyncStageFlags::Transfer,
                    .m_accessSrc = BarrierAccessFlags::None,
                    .m_accessDst = BarrierAccessFlags::TransferSrc,
                    .m_offset = 0,
                    .m_size = eastl::numeric_limits<u64>::max(),
                    .m_buffer = m_fontsStagingHandle,
                };

                TextureMemoryBarrier textureMemoryBarrier {
                    .m_stagesSrc = BarrierSyncStageFlags::None,
                    .m_stagesDst = BarrierSyncStageFlags::Transfer,
                    .m_accessSrc = BarrierAccessFlags::None,
                    .m_accessDst = BarrierAccessFlags::TransferDst,
                    .m_texture = m_fontsTextureHandle,
                    .m_layoutSrc = TextureLayout::Unknown,
                    .m_layoutDst = TextureLayout::TransferDst,
                };

                _graphicsContext->PlaceMemoryBarriers(
                    _commandList,
                    {},
                    { &stagingBufferBarrier, 1 },
                    { &textureMemoryBarrier, 1 });
            }

            _graphicsContext->SetTextureData(
                _commandList,
                m_fontsStagingHandle,
                m_fontsTextureHandle,
                m_fontsMemoryFootprint,
                { m_stagingData->m_fontsTextureDesc.m_desc, 0 },
                m_stagingData->m_data);

            {
                // Don't care about staging buffer state any more, they've outlived their usefulness

                TextureMemoryBarrier textureMemoryBarrier {
                    .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                    .m_stagesDst = BarrierSyncStageFlags::FragmentShading,
                    .m_accessSrc = BarrierAccessFlags::TransferDst,
                    .m_accessDst = BarrierAccessFlags::ShaderResource,
                    .m_texture = m_fontsTextureHandle,
                    .m_layoutSrc = TextureLayout::TransferDst,
                    .m_layoutDst = TextureLayout::ShaderResource,
                };

                _graphicsContext->PlaceMemoryBarriers(
                    _commandList,
                    {},
                    {},
                    { &textureMemoryBarrier, 1 });
            }
        }

        ImDrawData* drawData = ::ImGui::GetDrawData();

        const u8 frameIndex = _graphicsContext->GetCurrentFrameContextIndex();

        {
            const u64 vertexCount = drawData->TotalVtxCount;

            const u64 desiredSize = sizeof(VertexEntry) * Alignment::NextPowerOfTwo(vertexCount);
            if (m_dynamicVertexBuffer.GetSize(frameIndex) < desiredSize)
            {
                m_dynamicVertexBuffer.RequestResize(desiredSize);
            }

            auto* vertexEntries = static_cast<VertexEntry*>(m_dynamicVertexBuffer.Map(_graphicsContext, frameIndex));
            u64 vertexIndex = 0;
            for (auto i = 0u; i < drawData->CmdListsCount; i++)
            {
                const ImDrawList* drawList = drawData->CmdLists[i];
                for (auto j = 0; j < drawList->VtxBuffer.Size; j++)
                {
                    VertexEntry& entry = vertexEntries[vertexIndex];
                    const ImDrawVert& vert = drawList->VtxBuffer[j];

                    entry.m_position = { vert.pos.x, vert.pos.y };
                    entry.m_uv = { vert.uv.x, vert.uv.y };
                    entry.m_color = vert.col;

                    vertexIndex++;
                }
            }
            m_dynamicVertexBuffer.Unmap(_graphicsContext);

            m_dynamicVertexBuffer.PrepareBuffers(
                _graphicsContext,
                _commandList,
                BarrierAccessFlags::VertexBuffer,
                frameIndex);
        }

        {
            const u64 indexCount = drawData->TotalIdxCount;

            const u64 desiredSize = sizeof(u32) * Alignment::NextPowerOfTwo(indexCount);
            if (m_dynamicIndexBuffer.GetSize(frameIndex) < desiredSize)
            {
                m_dynamicIndexBuffer.RequestResize(desiredSize);
            }

            u32* indexBuffer = static_cast<u32*>(m_dynamicIndexBuffer.Map(_graphicsContext, frameIndex));
            for (auto i = 0u; i < drawData->CmdListsCount; i++)
            {
                const ImDrawList* drawList = drawData->CmdLists[i];
                for (auto j = 0; j < drawList->IdxBuffer.Size; j++)
                {
                    indexBuffer[j] = drawList->IdxBuffer[j];
                }
                indexBuffer += drawList->IdxBuffer.Size;
            }
            m_dynamicIndexBuffer.Unmap(_graphicsContext);

            m_dynamicIndexBuffer.PrepareBuffers(
                _graphicsContext,
                _commandList,
                BarrierAccessFlags::IndexBuffer,
                frameIndex);
        }
    }

    void Context::RenderFrame(GraphicsContext* _graphicsContext, CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextRenderFrame");

        ImDrawData* drawData = ::ImGui::GetDrawData();

        if (drawData == nullptr)
        {
            return;
        }

        // Set viewport
        {
            const Viewport viewport {
                .m_width = static_cast<s32>(drawData->DisplaySize.x),
                .m_height = static_cast<s32>(drawData->DisplaySize.y),
            };
            _graphicsContext->SetViewport(_commandList, viewport);
        }

        const u8 frameIndex = _graphicsContext->GetCurrentFrameContextIndex();

        // Set index buffer
        {
            const BufferSpan bufferView {
                .m_size = m_dynamicIndexBuffer.GetSize(frameIndex),
                .m_buffer = m_dynamicIndexBuffer.GetBuffer(frameIndex),
            };
            _graphicsContext->SetIndexBuffer(_commandList, bufferView, false);
        }

        // Set vertex buffer
        {
            BufferSpan bufferView {
                .m_size = m_dynamicVertexBuffer.GetSize(frameIndex),
                .m_stride = sizeof(VertexEntry),
                .m_buffer = m_dynamicVertexBuffer.GetBuffer(frameIndex),
            };
            _graphicsContext->SetVertexBuffers(_commandList, {&bufferView,1});
        }

        // Declare texture usage
        {
            _graphicsContext->DeclarePassTextureViewUsage(
                _commandList,
                {&m_fontTextureViewHandle, 1},
                TextureViewAccessType::Read);
        }

        u64 vertexOffset = 0;
        u64 indexOffset = 0;

        for (auto i = 0u; i < drawData->CmdListsCount; i++)
        {
            const ImDrawList* drawList = drawData->CmdLists[i];

            for (const auto& drawCmd : drawList->CmdBuffer)
            {
                // If user callback, run it instead
                if (drawCmd.UserCallback != nullptr)
                {
                    drawCmd.UserCallback(drawList, &drawCmd);
                    continue;
                }

                // Set up scissor rect
                {
                    const ImVec2 clipOffset = drawData->DisplayPos;
                    const ImVec2 clipMin{drawCmd.ClipRect.x - clipOffset.x, drawCmd.ClipRect.y - clipOffset.y};
                    const ImVec2 clipMax{drawCmd.ClipRect.z - clipOffset.x, drawCmd.ClipRect.w - clipOffset.y};

                    const Rect rect{
                        .m_left = static_cast<u32>(clipMin.x),
                        .m_top = static_cast<u32>(clipMin.y),
                        .m_right = static_cast<u32>(clipMax.x),
                        .m_bottom = static_cast<u32>(clipMax.y),
                    };
                    _graphicsContext->SetScissorsRect(_commandList, rect);
                }

                // Draw
                {
                    _graphicsContext->SetGraphicsPipeline(_commandList, m_pso);

                    _graphicsContext->SetGraphicsDescriptorSets(
                        _commandList,
                        m_pipelineLayout,
                        { &m_fontDescriptorSet, 1 });

                    PushConstants pushConstants {};
                    pushConstants.m_scale = {
                        2.0f / drawData->DisplaySize.x,
                        -2.0f / drawData->DisplaySize.y
                    };
                    pushConstants.m_translate = {
                        -1.0f - drawData->DisplayPos.x * pushConstants.m_scale.x,
                        1.0f - drawData->DisplayPos.y * pushConstants.m_scale.y,
                    };
                    _graphicsContext->SetGraphicsPushConstant(
                        _commandList,
                        m_pipelineLayout,
                        { reinterpret_cast<u32*>(&pushConstants), 4 },
                        0,
                        0);

                    const DrawIndexedInstancedDesc desc {
                        .m_elementCount = drawCmd.ElemCount,
                        .m_indexOffset = static_cast<u32>(indexOffset + drawCmd.IdxOffset),
                        .m_vertexOffset = static_cast<u32>(vertexOffset + drawCmd.VtxOffset),
                    };
                    _graphicsContext->DrawIndexedInstanced(_commandList, desc);
                }
            }

            vertexOffset += drawList->VtxBuffer.Size;
            indexOffset += drawList->IdxBuffer.Size;
        }
    }

    void Context::_InitPso(GraphicsContext* _graphicsContext, RenderPassHandle _renderPass)
    {
        KE_ZoneScopedFunction("Modules::ImGui::Context_InitPso");

        // Read shader files
        {
            constexpr auto readShaderFile = [](const auto& _path, auto& _vec)
            {
                std::ifstream file(_path.c_str(), std::ios::binary);
                VERIFY_OR_RETURN_VOID(file);

                file.seekg(0, std::ios::end);
                _vec.resize(file.tellg());
                file.seekg(0, std::ios::beg);

                KE_VERIFY(file.read(_vec.data(), _vec.size()));
            };

            AllocatorInstance allocator = m_vsBytecode.get_allocator();

            readShaderFile(
                eastl::string("Shaders/ImGui/ImGui_vs_MainVS.", allocator) + GraphicsContext::GetShaderFileExtension(),
                m_vsBytecode);
            readShaderFile(
                eastl::string("Shaders/ImGui/ImGui_ps_MainPS.", allocator) + GraphicsContext::GetShaderFileExtension(),
                m_fsBytecode);

            m_vsModule = _graphicsContext->RegisterShaderModule(m_vsBytecode.data(), m_vsBytecode.size());
            m_fsModule = _graphicsContext->RegisterShaderModule(m_fsBytecode.data(), m_fsBytecode.size());
        }

        // Set up descriptor set layout
        {
            const DescriptorBindingDesc descriptorSetBindings[] {
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::Sampler,
                    .m_visibility = ShaderVisibility::Fragment,
                }
            };
            const DescriptorSetDesc descriptorSetDesc {
                .m_bindings = descriptorSetBindings
            };
            m_setIndices.resize(descriptorSetDesc.m_bindings.size());
            m_fontDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                descriptorSetDesc,
                m_setIndices.data());
        }

        // Set up descriptor set
        {
            m_fontDescriptorSet = _graphicsContext->CreateDescriptorSet(m_fontDescriptorSetLayout);
        }

        // Pipeline layout creation
        {
            // Scale and translate push constant
            const PushConstantDesc pushConstantDesc[] {
                {
                    .m_sizeInBytes = sizeof(PushConstants),
                    .m_visibility = ShaderVisibility::Vertex,
                },
            };

            PipelineLayoutDesc pipelineLayoutDesc {
                .m_descriptorSets = { &m_fontDescriptorSetLayout, 1 },
                .m_pushConstants = pushConstantDesc,
            };

            m_pipelineLayout = _graphicsContext->CreatePipelineLayout(pipelineLayoutDesc);
        }

        // PSO creation
        {
            GraphicsPipelineDesc desc {
                .m_rasterState = {
                    .m_cullMode = RasterStateDesc::CullMode::None,
                },
                .m_colorBlending = {
                    .m_attachments = { { kDefaultColorAttachmentAlphaBlendDesc } },
                },
                .m_depthStencil = {
                    .m_depthTest = false,
                    .m_depthWrite = false,
                    .m_depthCompare = DepthStencilStateDesc::CompareOp::Always,
                },
                .m_renderPass = _renderPass,
                .m_pipelineLayout = m_pipelineLayout,
#if !defined(KE_FINAL)
                .m_debugName = "ImGui_Render_PSO",
#endif
            };

            const ShaderStage stages[] {
                {
                    .m_shaderModule = m_vsModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "MainVS",
                },
                {
                    .m_shaderModule = m_fsModule,
                    .m_stage = ShaderStage::Stage::Fragment,
                    .m_entryPoint = "MainPS",
                }
            };
            desc.m_stages = stages;

            const VertexLayoutElement vertexLayoutElements[] {
                {
                    .m_semanticName = VertexLayoutElement::SemanticName::Position,
                    .m_semanticIndex = 0,
                    .m_bindingIndex = 0,
                    .m_format = TextureFormat::RG32_Float,
                    .m_offset = offsetof(VertexEntry, m_position),
                    .m_location = 0,
                },
                {
                    .m_semanticName = VertexLayoutElement::SemanticName::Uv,
                    .m_semanticIndex = 0,
                    .m_bindingIndex = 0,
                    .m_format = TextureFormat::RG32_Float,
                    .m_offset = offsetof(VertexEntry, m_uv),
                    .m_location = 1,
                },
                {
                    .m_semanticName = VertexLayoutElement::SemanticName::Color,
                    .m_semanticIndex = 0,
                    .m_bindingIndex = 0,
                    .m_format = TextureFormat::RGBA8_UNorm,
                    .m_offset = offsetof(VertexEntry, m_color),
                    .m_location = 2,
                },
            };
            const VertexBindingDesc vertexBindings[] {
                {
                    .m_stride = sizeof(VertexEntry),
                    .m_binding = 0,
                }
            };
            desc.m_vertexInput = VertexInputDesc {
                .m_elements = vertexLayoutElements,
                .m_bindings = vertexBindings,
            };

            m_pso = _graphicsContext->CreateGraphicsPipeline(desc);
        }
    }
} // namespace KryneEngine