/**
 * @file
 * @author Max Godefroy
 * @date 14/01/2026.
 */

#include "UiCube.hpp"

#include <KryneEngine/Core/Graphics/Drawing.hpp>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <KryneEngine/Core/Math/Transform.hpp>
#include <fstream>

static const float3 positions[8] = {
    { -1, -1, -1 },
    {  1, -1, -1 },
    { -1,  1, -1 },
    {  1,  1, -1 },
    { -1, -1,  1 },
    {  1, -1,  1 },
    { -1,  1,  1 },
    {  1,  1,  1 },
};

const u16 indices[36] = {
    0, 2, 1, 2, 3, 1,
    1, 3, 5, 5, 3, 7,
    0, 1, 4, 1, 5, 4,
    4, 5, 6, 6, 5, 7,
    0, 4, 2, 4, 6, 2,
    2, 6, 3, 3, 6, 7,
};

struct UiCubeData
{
    float4x4 m_mvpMatrix;
};

UiCube::UiCube(
    AllocatorInstance _allocator,
    GraphicsContext& _graphicsContext,
    Modules::TextRendering::FontManager* _fontManager,
    RenderPassHandle _renderPass,
    Modules::TextRendering::MsdfAtlasManager* _atlasManager)
        : m_allocator(_allocator)
        , m_guiContexts(_allocator, 6)
        , m_guiRenderers(_allocator, 6)
        , m_constantBuffer(_allocator)
        , m_constantBufferViews(_allocator)
{
    m_vertexBuffer = _graphicsContext.CreateBuffer({
        .m_desc = {
            .m_size = sizeof(positions),
#if !defined(KE_FINAL)
            .m_debugName = "UI Cube vertex buffer"
#endif
        },
        .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::VertexBuffer | MemoryUsage::TransferDstBuffer,
    });

    m_indexBuffer = _graphicsContext.CreateBuffer({
        .m_desc = {
            .m_size = sizeof(indices),
#if !defined(KE_FINAL)
            .m_debugName = "UI Cube index buffer"
#endif
        },
        .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::IndexBuffer | MemoryUsage::TransferDstBuffer,
    });

    m_transferBuffer = _graphicsContext.CreateBuffer({
        .m_desc = {
            .m_size = sizeof(positions) + sizeof(indices),
#if !defined(KE_FINAL)
            .m_debugName = "UI Cube transfer buffer"
#endif
        },
        .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
    });

    BufferMapping mapping { m_transferBuffer };
    _graphicsContext.MapBuffer(mapping);
    memcpy(mapping.m_ptr, positions, sizeof(positions));
    memcpy(mapping.m_ptr + sizeof(positions), indices, sizeof(indices));
    _graphicsContext.UnmapBuffer(mapping);

    const u8 frameContextCount = _graphicsContext.GetFrameContextCount();
    {
        m_constantBuffer.Init(
            &_graphicsContext,
            BufferCreateDesc {
                .m_desc = {
                    .m_size = sizeof(UiCubeData),
#if !defined(KE_FINAL)
                    .m_debugName = "UiCube constant buffer",
#endif
                },
                .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::ConstantBuffer
            },
            frameContextCount);

        m_constantBufferViews.Resize(frameContextCount);
        for (auto i = 0u; i < frameContextCount; ++i)
        {
            m_constantBufferViews[i] = _graphicsContext.CreateBufferView(
                BufferViewDesc {
                    .m_buffer = m_constantBuffer.GetBuffer(i),
                    .m_size = sizeof(UiCubeData),
                    .m_offset = 0,
                    .m_stride = sizeof(UiCubeData),
                    .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                    .m_debugName = "UiCube constant buffer view",
#endif
                });
        }
    }

    {
        constexpr DescriptorBindingDesc bindings[] = {
            {
                .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                .m_visibility = ShaderVisibility::Vertex
            }
        };
        DescriptorSetLayoutHandle descriptorSetLayout = _graphicsContext.CreateDescriptorSetLayout(
            { .m_bindings = bindings },
            &m_descriptorSetIndex);

        m_pipelineLayout = _graphicsContext.CreatePipelineLayout({
            .m_descriptorSets = { &descriptorSetLayout, 1 },
        });

        m_descriptorSet = _graphicsContext.CreateDescriptorSet(descriptorSetLayout);
    }

    {
        const auto readShaderFile = [_allocator](const eastl::string_view _filePath) -> eastl::span<char>
        {
            std::ifstream file(_filePath.data(), std::ios::binary);
            KE_ASSERT(file);

            file.seekg(0, std::ios::end);
            const std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            auto* buffer = static_cast<char*>(_allocator.allocate(size));
            if (!file.read(buffer, size)) return {};

            file.close();

            return { buffer, static_cast<size_t>(size) };
        };

        const eastl::span<char> vertexShaderSource = readShaderFile(
            eastl::string("Shaders/UiCube_MainVS.", _allocator) + GraphicsContext::GetShaderFileExtension());
        const eastl::span<char> fragmentShaderSource = readShaderFile(
            eastl::string("Shaders/UiCube_MainFS.", _allocator) + GraphicsContext::GetShaderFileExtension());

        const ShaderModuleHandle vertexShaderModule = _graphicsContext.RegisterShaderModule(vertexShaderSource.data(), vertexShaderSource.size());
        const ShaderModuleHandle fragmentShaderModule = _graphicsContext.RegisterShaderModule(fragmentShaderSource.data(), fragmentShaderSource.size());

        const ShaderStage stages[] = {
            {
                .m_shaderModule = vertexShaderModule,
                .m_stage = ShaderStage::Stage::Vertex,
                .m_entryPoint = "MainVS",
            },
            {
                .m_shaderModule = fragmentShaderModule,
                .m_stage = ShaderStage::Stage::Fragment,
                .m_entryPoint = "MainFS",
            },
        };

        constexpr VertexLayoutElement vertexElements[] = {
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Position,
                .m_semanticIndex = 0,
                .m_bindingIndex = 0,
                .m_format = TextureFormat::RGB32_Float,
                .m_offset = 0,
                .m_location = 0,
            },
        };

        constexpr VertexBindingDesc vertexBindings[] = {
            {
                .m_stride = sizeof(float3),
                .m_inputRate = VertexInputRate::Vertex,
            }
        };

        GraphicsPipelineDesc pipelineDesc {
            .m_stages = stages,
            .m_vertexInput = {
                .m_elements = vertexElements,
                .m_bindings = vertexBindings
            },
            .m_rasterState = {
                // .m_cullMode = RasterStateDesc::CullMode::None,
            },
            .m_colorBlending = {
                .m_attachments = { kDefaultColorAttachmentAlphaBlendDesc }
            },
            .m_depthStencil = {
                .m_depthTest = false,
                .m_depthWrite = false,
            },
            .m_renderPass =  _renderPass,
            .m_pipelineLayout = m_pipelineLayout,
        };

        m_pso = _graphicsContext.CreateGraphicsPipeline(pipelineDesc);

        _graphicsContext.FreeShaderModule(fragmentShaderModule);
        _graphicsContext.FreeShaderModule(vertexShaderModule);
        _allocator.deallocate(fragmentShaderSource.data());
        _allocator.deallocate(vertexShaderSource.data());
    }

    for (u32 i = 0; i < 6; ++i)
    {
        m_guiContexts.Init(i, _allocator, _fontManager);
        new (m_guiRenderers.Data() + i) Modules::GuiLib::BasicGuiRenderer(_allocator, &_graphicsContext, _renderPass);
        m_guiContexts[i].Initialize(&m_guiRenderers[i], m_uiViewportSize);
        m_guiRenderers[i].SetAtlasManager(_atlasManager);
    }
}


void UiCube::Render(
    GraphicsContext& _graphicsContext,
    CommandListHandle _transferCommandList,
    CommandListHandle _renderCommandList,
    const float _contentScale)
{
    if (m_transferBuffer != GenPool::kInvalidHandle)
    {
        if (m_transferFrameId == ~0u)
        {
            _graphicsContext.CopyBuffer(_transferCommandList, {
                .m_copySize = sizeof(positions),
                .m_bufferSrc = m_transferBuffer,
                .m_bufferDst = m_vertexBuffer,
                .m_offsetSrc = 0,
                .m_offsetDst = 0,
            });

            _graphicsContext.CopyBuffer(_transferCommandList, {
                .m_copySize = sizeof(indices),
                .m_bufferSrc = m_transferBuffer,
                .m_bufferDst = m_indexBuffer,
                .m_offsetSrc = sizeof(positions),
                .m_offsetDst = 0,
            });

            m_transferFrameId = _graphicsContext.GetFrameId();
        }
        else if (_graphicsContext.IsFrameExecuted(m_transferFrameId))
        {
            _graphicsContext.DestroyBuffer(m_transferBuffer);
            m_transferBuffer = GenPool::kInvalidHandle;
        }
    }

    const u8 frameIndex = _graphicsContext.GetCurrentFrameContextIndex();

    const GraphicsCommon::ApplicationInfo& appInfo = _graphicsContext.GetApplicationInfo();
    const u32 viewportSize = static_cast<u32>(332.f * _contentScale);
    const uint2 frameBufferSize = _graphicsContext.GetPresentFrameBufferSize();
    const Rect cubeViewport {
        .m_left = 0,
        .m_top = frameBufferSize.y - viewportSize,
        .m_right = viewportSize,
        .m_bottom = frameBufferSize.y,
    };

    const float4x4 projection = Math::PerspectiveProjection(60.f * M_PI / 180.f, 1, 0.1f, 100.f, false);

    const float time = static_cast<float>(_graphicsContext.GetFrameId()) / 60.f;
    Math::Quaternion rotation;
    rotation.FromAxisAngle(float3(1, 0, 0).Normalized(), -M_PI_4);
    rotation = rotation * Math::Quaternion().FromAxisAngle(float3(0, 0, 1), -M_PI_2 * 1.5) * rotation.Conjugate();
    rotation = rotation * Math::Quaternion().FromAxisAngle(float3(0, 1, 0), time * 2.f) * rotation.Conjugate();

    // Move the cube to the bottom left third of the screen
    const float3 position = { 0, 5, 0 };
    const auto model = Math::ComputeTransformMatrix<float4x4>(position, rotation, float3(1));

    {
        m_constantBuffer.PrepareBuffers(&_graphicsContext, _transferCommandList, BarrierAccessFlags::ConstantBuffer, frameIndex);

        auto* data = static_cast<UiCubeData*>(m_constantBuffer.Map(&_graphicsContext, frameIndex));

        data->m_mvpMatrix = projection * model;

        m_constantBuffer.Unmap(&_graphicsContext);
        m_constantBuffer.PrepareBuffers(&_graphicsContext, _transferCommandList, BarrierAccessFlags::ConstantBuffer, frameIndex);
    }

    {
        const DescriptorSetWriteInfo::DescriptorData descriptorData {
            .m_handle = m_constantBufferViews[frameIndex].m_handle,
        };
        const DescriptorSetWriteInfo writeInfo {
            .m_index = m_descriptorSetIndex,
            .m_descriptorData = { &descriptorData, 1 },
        };
        _graphicsContext.DeclarePassBufferViewUsage(_renderCommandList, { &m_constantBufferViews[frameIndex], 1 }, BufferViewAccessType::Read);
        _graphicsContext.UpdateDescriptorSet(m_descriptorSet, { &writeInfo, 1 }, true);
    }


    const BufferSpan vbSpan { .m_size = sizeof(positions), .m_offset = 0, .m_stride = sizeof(float3), .m_buffer = m_vertexBuffer };
    _graphicsContext.SetVertexBuffers(_renderCommandList, { &vbSpan, 1 });

    const BufferSpan ibSpan { .m_size = sizeof(indices), .m_offset = 0, .m_stride = sizeof(u16), .m_buffer = m_indexBuffer };
    _graphicsContext.SetIndexBuffer(_renderCommandList, ibSpan, true);

    _graphicsContext.SetViewport(_renderCommandList, {
        .m_topLeftX = static_cast<s32>(cubeViewport.m_left),
        .m_topLeftY = static_cast<s32>(cubeViewport.m_top),
        .m_width = static_cast<s32>(cubeViewport.m_right - cubeViewport.m_left),
        .m_height = static_cast<s32>(cubeViewport.m_bottom - cubeViewport.m_top),
    });

    _graphicsContext.SetGraphicsPipeline(_renderCommandList, m_pso);
    _graphicsContext.SetGraphicsDescriptorSets(_renderCommandList, m_pipelineLayout, { &m_descriptorSet, 1 });

    _graphicsContext.DrawIndexedInstanced(_renderCommandList, { .m_elementCount = 36, });

    for (u32 i = 0; i < 6; ++i)
    {
        float4x4 localTransform;
        switch (i)
        {
        case 0:
            localTransform= Math::ComputeTransformMatrix<float4x4>(
                float3(0, 0, -1),
                Math::Quaternion().FromAxisAngle(float3(0, 1, 0), M_PI),
                float3(1.f));
            break;
        case 1:
            localTransform = Math::ComputeTransformMatrix<float4x4>(
                float3(1, 0, 0),
                Math::Quaternion().FromAxisAngle(float3(0, 1, 0), M_PI_2),
                float3(1.f));
            break;
        case 2:
            localTransform = Math::ComputeTransformMatrix<float4x4>(
                float3(0, -1, 0),
                Math::Quaternion().FromAxisAngle(float3(1, 0, 0), M_PI_2),
                float3(1.f));
            break;
        case 3:
            localTransform = Math::ComputeTransformMatrix<float4x4>(
                float3(0, 0, 1),
                Math::Quaternion(),
                float3(1.f));
            break;
        case 4:
            localTransform = Math::ComputeTransformMatrix<float4x4>(
                float3(-1, 0, 0),
                Math::Quaternion().FromAxisAngle(float3(0, 1, 0), -M_PI_2),
                float3(1.f));
            break;
        case 5:
            localTransform = Math::ComputeTransformMatrix<float4x4>(
                float3(0, 1, 0),
                Math::Quaternion().FromAxisAngle(float3(1, 0, 0), -M_PI_2),
                float3(1.f));
            break;
        default: KE_ASSERT_FATAL(false);
        }
        const float4x4 uiProjection = projection * model * localTransform;

        m_guiContexts[i].BeginLayout(m_uiViewportSize, uiProjection);
        CLAY({
            .layout = {
                .sizing =  { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .border = { .color = { 0, 0, 0, 255 }, .width = {4, 4, 4, 4 } }
        })
        {
            const auto string = eastl::string().sprintf("Face %u", i);
            const Clay_String clayString { false, static_cast<s32>(string.size()), string.data() };
            CLAY_TEXT(clayString, CLAY_TEXT_CONFIG({
                .textColor = { 0, 0, 0, 255 },
                .fontId = 0,
                .fontSize = 32,
            }));
        }
        m_guiContexts[i].EndLayout(_graphicsContext, _transferCommandList, _renderCommandList);
    }
}
