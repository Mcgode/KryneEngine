/**
 * @file
 * @author Max Godefroy
 * @date 21/07/2024.
 */

#include "KryneEngine/Modules/ImGui/Context.hpp"

#include <fstream>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <KryneEngine/Core/Common/Utils/Alignment.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

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

    Context::Context(
        Window* _window,
        const RenderPassHandle _renderPass,
        AllocatorInstance _allocator,
        const eastl::span<char> _vsBytecode,
        const eastl::span<char> _fsBytecode)
            : m_systemsTexturesStagingBuffers(_allocator)
            , m_systemTextures(_allocator)
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
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        const float2 dpiScale = _window->GetDpiScale();
        io.DisplayFramebufferScale = { dpiScale.x, dpiScale.y };

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

        InitPso(graphicsContext, _renderPass, _vsBytecode, _fsBytecode);

        m_timePoint = eastl::chrono::steady_clock::now();

        m_systemsTexturesStagingBuffers.Resize(graphicsContext->GetFrameContextCount());
        m_systemsTexturesStagingBuffers.InitAll(SystemTextureStagingBuffer {});

        m_defaultSampler = graphicsContext->CreateSampler({
#if !defined(KE_FINAL)
            .m_debugName = eastl::string { "ImGui Default Sampler", _allocator },
#endif
        });
    }

    Context::~Context()
    {
        KE_ASSERT_MSG(m_context == nullptr, "ImGui module was not shut down");
    }

    void Context::Shutdown(Window* _window)
    {
        KE_ZoneScopedFunction("Modules::ImGui::ContextShutdown");

        GraphicsContext* graphicsContext = _window->GetGraphicsContext();

        for (const auto& stagingBuffer: m_systemsTexturesStagingBuffers)
        {
            if (stagingBuffer.m_buffer != GenPool::kInvalidHandle)
            {
                graphicsContext->DestroyBuffer(stagingBuffer.m_buffer);
            }
        }

        m_dynamicIndexBuffer.Destroy(graphicsContext);
        m_dynamicVertexBuffer.Destroy(graphicsContext);

        if (m_defaultSampler != GenPool::kInvalidHandle)
        {
            graphicsContext->DestroySampler(m_defaultSampler);
        }

        for (const auto* texture: ::ImGui::GetPlatformIO().Textures)
        {
            if (texture->Status == ImTextureStatus_Destroyed)
                continue;

            const TextureViewHandle textureView = FromImTextureID(texture->GetTexID()).first;

            const auto it = m_systemTextures.find(textureView);
            KE_ASSERT(it != m_systemTextures.end());

            graphicsContext->DestroyTextureView(it->first);
            graphicsContext->DestroyTexture(it->second.m_texture);
            m_systemTextures.erase(it);
        }
        KE_ASSERT(m_systemTextures.empty());

        {
            graphicsContext->DestroyGraphicsPipeline(m_pso);
            graphicsContext->DestroyPipelineLayout(m_pipelineLayout);
            for (auto descriptorSet : m_descriptorSets)
                graphicsContext->DestroyDescriptorSet(descriptorSet);
            graphicsContext->DestroyDescriptorSetLayout(m_descriptorSetLayout);
        }

        // Unregister input callbacks.
        m_input->Shutdown(_window);
        m_setIndices.get_allocator().Delete(m_input);

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

        ImDrawData* drawData = ::ImGui::GetDrawData();

        eastl::vector<TextureMemoryBarrier> textureMemoryBarriers(m_setIndices.get_allocator());
        size_t stagingBufferSizeRequired = 0;
        for (auto* texture: *drawData->Textures)
        {
            switch (texture->Status)
            {
            case ImTextureStatus_OK:
            case ImTextureStatus_Destroyed:
                break;
            case ImTextureStatus_WantCreate:
            {
                const TextureDesc textureDesc {
                    .m_dimensions = { texture->Width, texture->Height, 1 },
                    .m_format = texture->Format == ImTextureFormat_Alpha8 ? TextureFormat::R8_UNorm : TextureFormat::RGBA8_UNorm,
#if !defined(KE_FINAL)
                    .m_debugName = eastl::string { m_setIndices.get_allocator() }.sprintf("ImGuiSystemTexture [%8X]", texture->UniqueID)
#endif
                };
                auto footprints = _graphicsContext->FetchTextureSubResourcesMemoryFootprints(textureDesc);

                TextureHandle textureHandle = _graphicsContext->CreateTexture({
                    .m_desc = textureDesc,
                    .m_footprintPerSubResource = footprints,
                    .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage | MemoryUsage::TransferDstImage,
                });

                TextureViewDesc textureViewDesc {
                    .m_texture = textureHandle,
                    .m_format = textureDesc.m_format,
                };
                if (textureDesc.m_format == TextureFormat::R8_UNorm)
                {
                    textureViewDesc.m_componentsMapping[0] = TextureComponentMapping::Red;
                    textureViewDesc.m_componentsMapping[1] = TextureComponentMapping::Red;
                    textureViewDesc.m_componentsMapping[2] = TextureComponentMapping::Red;
                    textureViewDesc.m_componentsMapping[3] = TextureComponentMapping::Red;
                }
                TextureViewHandle textureView = _graphicsContext->CreateTextureView(textureViewDesc);

                textureMemoryBarriers.push_back({
                    .m_stagesSrc = BarrierSyncStageFlags::All,
                    .m_stagesDst = BarrierSyncStageFlags::Transfer,
                    .m_accessSrc = BarrierAccessFlags::None,
                    .m_accessDst = BarrierAccessFlags::TransferDst,
                    .m_texture = textureHandle,
                    .m_layoutSrc = TextureLayout::Unknown,
                    .m_layoutDst = TextureLayout::TransferDst,
                });

                KE_ASSERT(texture->Updates.empty());
                stagingBufferSizeRequired +=
                    Alignment::AlignUp<u32>(texture->Width * texture->BytesPerPixel, footprints.front().m_rowPitchAlignment) *
                        texture->Height;

                m_systemTextures.emplace(textureView, SystemTexture { textureHandle, footprints.front() });
                texture->SetTexID(ToImTextureID(textureView));
                break;
            }
            case ImTextureStatus_WantUpdates:
            {
                const auto it = m_systemTextures.find(FromImTextureID(texture->GetTexID()).first);
                KE_ASSERT(it != m_systemTextures.end());
                const TextureMemoryFootprint& footprint = it->second.m_footprint;
                textureMemoryBarriers.push_back({
                    .m_stagesSrc = BarrierSyncStageFlags::FragmentShading,
                    .m_stagesDst = BarrierSyncStageFlags::Transfer,
                    .m_accessSrc = BarrierAccessFlags::ShaderResource,
                    .m_accessDst = BarrierAccessFlags::TransferDst,
                    .m_texture = it->second.m_texture,
                    .m_layoutSrc = TextureLayout::ShaderResource,
                    .m_layoutDst = TextureLayout::TransferDst,
                });
                for (const auto& update: texture->Updates)
                {
                    stagingBufferSizeRequired += Alignment::AlignUp<u32>(update.w * texture->BytesPerPixel, footprint.m_rowPitchAlignment) * update.h;
                }
                break;
            }
            case ImTextureStatus_WantDestroy:
                if (texture->UnusedFrames >= _graphicsContext->GetFrameContextCount())
                {
                    const TextureViewHandle textureView = FromImTextureID(texture->GetTexID()).first;
                    const auto it = m_systemTextures.find(textureView);
                    KE_ASSERT(it != m_systemTextures.end());
                    _graphicsContext->DestroyTextureView(it->first);
                    _graphicsContext->DestroyTexture(it->second.m_texture);
                    m_systemTextures.erase(it);
                    texture->SetTexID(ImTextureID_Invalid);
                    texture->SetStatus(ImTextureStatus_Destroyed);
                }
                break;
            }
        }

        if (GraphicsContext::SupportsNonGlobalBarriers())
        {
            _graphicsContext->PlaceMemoryBarriers(
                _commandList,
                {},
                {},
                textureMemoryBarriers);
        }

        SystemTextureStagingBuffer& stagingBuffer = m_systemsTexturesStagingBuffers[_graphicsContext->GetCurrentFrameContextIndex()];
        if (stagingBufferSizeRequired > stagingBuffer.m_size)
        {
            if (stagingBuffer.m_buffer != GenPool::kInvalidHandle)
            {
                _graphicsContext->DestroyBuffer(stagingBuffer.m_buffer);
            }

            const BufferCreateDesc stagingBufferDesc {
                .m_desc = {
                    .m_size = stagingBufferSizeRequired,
                },
                .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
            };

            stagingBuffer.m_buffer = _graphicsContext->CreateBuffer(stagingBufferDesc);
            stagingBuffer.m_size = stagingBufferSizeRequired;

            if (GraphicsContext::SupportsNonGlobalBarriers())
            {
                const BufferMemoryBarrier stagingBufferBarrier[1] = {
                    {
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_offset = 0,
                        .m_size = stagingBufferSizeRequired,
                        .m_buffer = stagingBuffer.m_buffer,
                    }
                };
                _graphicsContext->PlaceMemoryBarriers(
                    _commandList,
                    {},
                    stagingBufferBarrier,
                    {});
            }
        }

        if (stagingBufferSizeRequired > 0)
        {
            BufferMapping mapping { stagingBuffer.m_buffer, stagingBufferSizeRequired };
            _graphicsContext->MapBuffer(mapping);

            std::byte* stagingBufferPtr = mapping.m_ptr;

            for (auto* texture: *drawData->Textures)
            {
                if (texture->Status != ImTextureStatus_WantCreate && texture->Status != ImTextureStatus_WantUpdates)
                    continue;

                const auto it = m_systemTextures.find(FromImTextureID(texture->GetTexID()).first);
                KE_ASSERT(it != m_systemTextures.end());
                const TextureMemoryFootprint& footprint = it->second.m_footprint;

                if (texture->Status == ImTextureStatus_WantCreate)
                {
                    const size_t rowSize = Alignment::AlignUp<u32>(texture->Width * texture->BytesPerPixel, footprint.m_rowPitchAlignment);
                    for (auto y = 0; y < texture->Height; y++)
                    {
                        memcpy(stagingBufferPtr + y * rowSize, texture->GetPixelsAt(0, y), texture->Width * texture->BytesPerPixel);
                        memset(
                            stagingBufferPtr + y * rowSize + texture->Width * texture->BytesPerPixel,
                            0,
                            rowSize - texture->Width * texture->BytesPerPixel);
                    }
                    _graphicsContext->SetTextureRegionData(
                        _commandList,
                        BufferSpan {
                            .m_size = rowSize * texture->Height,
                            .m_offset = static_cast<u64>(eastl::distance(mapping.m_ptr, stagingBufferPtr)),
                            .m_buffer = stagingBuffer.m_buffer,
                        },
                        it->second.m_texture,
                        footprint,
                        {},
                        {0, 0, 0},
                        {texture->Width, texture->Height, 1}
                    );
                    stagingBufferPtr += rowSize * texture->Height;
                }
                else
                {
                    for (const auto& update: texture->Updates)
                    {
                        const size_t rowSize = Alignment::AlignUp<u32>(update.w * texture->BytesPerPixel, footprint.m_rowPitchAlignment);
                        for (auto y = 0; y < update.h; y++)
                        {
                            memcpy(
                                stagingBufferPtr + y * rowSize,
                                texture->GetPixelsAt(update.x, y + update.y),
                                update.w * texture->BytesPerPixel);
                            memset(stagingBufferPtr + y * rowSize + update.w * texture->BytesPerPixel, 0, rowSize - update.w * texture->BytesPerPixel);
                        }

                        TextureMemoryFootprint regionFootprint = footprint;
                        regionFootprint.m_lineByteAlignedSize = rowSize;
                        regionFootprint.m_width = update.w,
                        regionFootprint.m_height = update.h;
                        _graphicsContext->SetTextureRegionData(
                            _commandList,
                            BufferSpan {
                                .m_size = rowSize * update.h,
                                .m_offset = static_cast<u64>(eastl::distance(mapping.m_ptr, stagingBufferPtr)),
                                .m_buffer = stagingBuffer.m_buffer,
                            },
                            it->second.m_texture,
                            regionFootprint,
                            {},
                            { update.x, update.y, 0 },
                            { update.w, update.h, 1 });

                        stagingBufferPtr += rowSize * update.h;
                    }
                }
            }

            _graphicsContext->UnmapBuffer(mapping);
        }

        textureMemoryBarriers.clear();
        for (auto* texture: *drawData->Textures)
        {
            if (texture->Status != ImTextureStatus_WantCreate && texture->Status != ImTextureStatus_WantUpdates)
                continue;

            const auto it = m_systemTextures.find(FromImTextureID(texture->GetTexID()).first);
            KE_ASSERT(it != m_systemTextures.end());

            textureMemoryBarriers.push_back({
                    .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                    .m_stagesDst = BarrierSyncStageFlags::FragmentShading,
                    .m_accessSrc = BarrierAccessFlags::TransferDst,
                    .m_accessDst = BarrierAccessFlags::ShaderResource,
                    .m_texture = it->second.m_texture,
                    .m_layoutSrc = TextureLayout::TransferDst,
                    .m_layoutDst = TextureLayout::ShaderResource,
                });

            texture->SetStatus(ImTextureStatus_OK);
        }
        if (GraphicsContext::SupportsNonGlobalBarriers())
        {
            _graphicsContext->PlaceMemoryBarriers(_commandList, {}, {}, textureMemoryBarriers);
        }

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
                .m_topLeftX = static_cast<s32>(drawData->DisplayPos.x * drawData->FramebufferScale.x),
                .m_topLeftY = static_cast<s32>(drawData->DisplayPos.y * drawData->FramebufferScale.y),
                .m_width = static_cast<s32>(drawData->DisplaySize.x * drawData->FramebufferScale.x),
                .m_height = static_cast<s32>(drawData->DisplaySize.y * drawData->FramebufferScale.y),
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

        eastl::vector_map<ImTextureID, DescriptorSetHandle> textureDescriptorSets(m_setIndices.get_allocator());

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

                // Handle descriptor set caching and selection
                DescriptorSetHandle descriptorSet;
                {
                    auto it = textureDescriptorSets.find(drawCmd.GetTexID());

                    if (it != textureDescriptorSets.end())
                    {
                        descriptorSet = it->second;
                    }
                    else
                    {
                        const size_t index = textureDescriptorSets.size();
                        KE_ASSERT(index <= m_descriptorSets.size());
                        if (index == m_descriptorSets.size())
                        {
                            m_descriptorSets.push_back(_graphicsContext->CreateDescriptorSet(m_descriptorSetLayout));
                        }
                        descriptorSet = m_descriptorSets[index];
                        textureDescriptorSets[drawCmd.GetTexID()] = descriptorSet;

                        const auto pair = FromImTextureID(drawCmd.GetTexID());

                        const DescriptorSetWriteInfo::DescriptorData textureData[1] = {
                            {
                                .m_textureLayout = TextureLayout::ShaderResource,
                                .m_handle = pair.first.m_handle,
                            }
                        };
                        const DescriptorSetWriteInfo::DescriptorData samplerData[1] = {
                            {
                                .m_handle = pair.second == GenPool::kInvalidHandle ? m_defaultSampler.m_handle : pair.second.m_handle,
                            }
                        };
                        const DescriptorSetWriteInfo writeInfo[2] = {
                            {
                                .m_index = m_setIndices[0],
                                .m_descriptorData = textureData,
                            },
                            {
                                .m_index = m_setIndices[1],
                                .m_descriptorData = samplerData,
                            }
                        };

                        _graphicsContext->UpdateDescriptorSet(descriptorSet, writeInfo, true);

                        _graphicsContext->DeclarePassTextureViewUsage(_commandList, { &pair.first, 1 }, TextureViewAccessType::Read);
                    }
                }

                // Set up scissor rect
                {
                    const ImVec2 clipOffset = drawData->DisplayPos;
                    const ImVec2 clipMin{drawCmd.ClipRect.x - clipOffset.x, drawCmd.ClipRect.y - clipOffset.y};
                    const ImVec2 clipMax{drawCmd.ClipRect.z - clipOffset.x, drawCmd.ClipRect.w - clipOffset.y};

                    const Rect rect{
                        .m_left = static_cast<u32>(clipMin.x * drawData->FramebufferScale.x),
                        .m_top = static_cast<u32>(clipMin.y * drawData->FramebufferScale.y),
                        .m_right = static_cast<u32>(clipMax.x * drawData->FramebufferScale.x),
                        .m_bottom = static_cast<u32>(clipMax.y * drawData->FramebufferScale.y),
                    };
                    _graphicsContext->SetScissorsRect(_commandList, rect);
                }

                // Draw
                {
                    _graphicsContext->SetGraphicsPipeline(_commandList, m_pso);

                    _graphicsContext->SetGraphicsDescriptorSets(
                        _commandList,
                        m_pipelineLayout,
                        { &descriptorSet, 1 });

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

    ImTextureID Context::ToImTextureID(TextureViewHandle _texture, SamplerHandle _sampler)
    {
        const u64 id = static_cast<u64>(static_cast<u32>(_texture.m_handle))
        | static_cast<u64>(static_cast<u32>(_sampler.m_handle)) << 32;
        return id;
    }

    eastl::pair<TextureViewHandle, SamplerHandle> Context::FromImTextureID(ImTextureID _textureId)
    {
        return {
            TextureViewHandle(GenPool::Handle::FromU32(static_cast<u32>(_textureId))),
            SamplerHandle(GenPool::Handle::FromU32(static_cast<u32>(_textureId >> 32)))
        };
    }

    void Context::InitPso(
        GraphicsContext* _graphicsContext,
        RenderPassHandle _renderPass,
        eastl::span<char> _externalVsBytecode,
        eastl::span<char> _externalFsBytecode)
    {
        KE_ZoneScopedFunction("Modules::ImGui::Context_InitPso");

        eastl::span<char> vsBytecode = _externalVsBytecode;
        eastl::span<char> fsBytecode = _externalFsBytecode;

        ShaderModuleHandle vsModule;
        ShaderModuleHandle fsModule;

        // Read shader files
        {
            constexpr auto readShaderFile = [](const auto& _path, eastl::span<char>& _span, const AllocatorInstance _allocator)
            {
                std::ifstream file(_path.c_str(), std::ios::binary);
                VERIFY_OR_RETURN_VOID(file);

                file.seekg(0, std::ios::end);
                const auto size = static_cast<size_t>(file.tellg());
                char* data = _allocator.Allocate<char>(size);
                file.seekg(0, std::ios::beg);

                KE_VERIFY(file.read(data, size));
                _span = { data, size };
            };

            AllocatorInstance allocator = m_setIndices.get_allocator();

            if (_externalVsBytecode.empty())
            {
                readShaderFile(
                   eastl::string("Shaders/ImGui/ImGui_vs_MainVS.", allocator) + GraphicsContext::GetShaderFileExtension(),
                   vsBytecode,
                   allocator);
            }
            if (_externalFsBytecode.empty())
            {
                readShaderFile(
                   eastl::string("Shaders/ImGui/ImGui_ps_MainPS.", allocator) + GraphicsContext::GetShaderFileExtension(),
                   fsBytecode,
                   allocator);
            }

            vsModule = _graphicsContext->RegisterShaderModule(vsBytecode.data(), vsBytecode.size());
            fsModule = _graphicsContext->RegisterShaderModule(fsBytecode.data(), fsBytecode.size());
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
            m_descriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                descriptorSetDesc,
                m_setIndices.data());
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
                .m_descriptorSets = { &m_descriptorSetLayout, 1 },
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
                    .m_shaderModule = vsModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "MainVS",
                },
                {
                    .m_shaderModule = fsModule,
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

        // Cleanup
        {
            _graphicsContext->FreeShaderModule(fsModule);
            _graphicsContext->FreeShaderModule(vsModule);
            if (_externalFsBytecode.empty())
                m_setIndices.get_allocator().deallocate(fsBytecode.data(), fsBytecode.size());
            if (_externalVsBytecode.empty())
                m_setIndices.get_allocator().deallocate(vsBytecode.data(), vsBytecode.size());
        }
    }
} // namespace KryneEngine