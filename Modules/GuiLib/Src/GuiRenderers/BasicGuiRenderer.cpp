/**
 * @file
 * @author Max Godefroy
 * @date 08/12/2025.
 */

#include "KryneEngine/Modules/GuiLib/GuiRenderers/BasicGuiRenderer.hpp"

#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Graphics/Drawing.hpp>
#include <KryneEngine/Core/Graphics/ShaderPipeline.hpp>
#include <KryneEngine/Core/Math/Color.hpp>
#include <KryneEngine/Core/Math/Float16.hpp>
#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Modules/TextRendering/MsdfAtlasManager.hpp>

#include <EASTL/hash_map.h>
#include <EASTL/vector_set.h>
#include <clay.h>
#include <cmath>
#include <fstream>

#include "KryneEngine/Modules/GuiLib/TextureRegion.hpp"
#include "KryneEngine/Modules/TextRendering/Font.hpp"
#include "KryneEngine/Modules/TextRendering/FontManager.hpp"

namespace KryneEngine
{
    struct PackedInstanceData
    {
        uint2 m_packedRect;
        uint m_packedColor;
        uint4 m_packedData;
    };

    struct SlotData
    {
        u16 m_index;
        u16 m_descriptorSetIndex;
    };

    using TextureDataMap = eastl::hash_map<u32, SlotData>;
    using SamplerArray = eastl::fixed_vector<SamplerHandle, Modules::GuiLib::BasicGuiRenderer::kMaxSamplerSlots, false>;
    using SamplerDataMap = eastl::vector<SamplerArray>;

    static eastl::pair<TextureDataMap, SamplerDataMap> HandleTextureSets(
        const AllocatorInstance _allocator,
        GraphicsContext& _graphicsContext,
        const Clay_RenderCommandArray& _renderCommandArray,
        eastl::vector<DescriptorSetHandle>& _texturesDescriptorSets,
        DescriptorSetLayoutHandle _texturesDescriptorSetLayout,
        SamplerHandle _defaultSampler,
        const u32* _descriptorSetIndices)
    {
        KE_ZoneScopedFunction("Handle texture sets");

        TextureDataMap textureData(_allocator);
        SamplerDataMap samplerData(_allocator);

        eastl::vector<eastl::array<DescriptorSetWriteInfo::DescriptorData, Modules::GuiLib::BasicGuiRenderer::kMaxTextureSlots>> textureSetWrites(_allocator);
        eastl::vector<eastl::array<DescriptorSetWriteInfo::DescriptorData, Modules::GuiLib::BasicGuiRenderer::kMaxSamplerSlots>> samplerSetWrites(_allocator);

        u16 descriptorSetIndex = 0;

        for (u32 i = 0; i < _renderCommandArray.length; i++)
        {
            const Clay_RenderCommand& renderCommand = _renderCommandArray.internalArray[i];

            if (renderCommand.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) continue;

            const auto* textureRegion = static_cast<const Modules::GuiLib::TextureRegion*>(renderCommand.renderData.image.imageData);

            if (!KE_VERIFY_MSG(textureRegion->m_textureType == TextureTypes::Single2D, "Unsupported texture type")) continue;

            const SamplerHandle sampler = textureRegion->m_customSampler != GenPool::kInvalidHandle
                ? textureRegion->m_customSampler
                : _defaultSampler;

            const TextureViewHandle textureView = textureRegion->m_textureView;
            const auto it = textureData.find(static_cast<u32>(textureView.m_handle));

            // Texture is already in a set, add sampler to it if not already done
            if (it != textureData.end())
            {
                SamplerArray& array = samplerData[it->second.m_descriptorSetIndex];
                if (eastl::find(array.begin(), array.end(), sampler) == array.end() && KE_VERIFY(array.size() < array.max_size()))
                {
                    KE_ASSERT(samplerSetWrites.size() > it->second.m_descriptorSetIndex);
                    samplerSetWrites[it->second.m_descriptorSetIndex][array.size()] = {
                        .m_handle = sampler.m_handle,
                    };
                    array.push_back(sampler);
                }
                continue;
            }

            // Push texture to set

            if (descriptorSetIndex >= Modules::GuiLib::BasicGuiRenderer::kMaxTextureSlots || samplerData.empty())
            {
                samplerData.push_back();
                textureSetWrites.push_back();
                samplerSetWrites.push_back();

                if (_texturesDescriptorSets.size() < samplerData.size())
                {
                    _texturesDescriptorSets.push_back(_graphicsContext.CreateDescriptorSet(_texturesDescriptorSetLayout));
                }

                descriptorSetIndex %= Modules::GuiLib::BasicGuiRenderer::kMaxTextureSlots;
            }

            textureData.emplace(static_cast<u32>(textureView.m_handle), SlotData {
                .m_index = descriptorSetIndex,
                .m_descriptorSetIndex = static_cast<u16>(samplerData.size() - 1u),
            });
            textureSetWrites.back()[descriptorSetIndex] = {
                .m_textureLayout = TextureLayout::ShaderResource,
                .m_handle = textureView.m_handle,
            };

            SamplerArray& samplerArray = samplerData.back();
            const auto samplerIt = eastl::find(samplerArray.begin(), samplerArray.end(), sampler);
            if (samplerIt == samplerArray.end() && KE_VERIFY(samplerArray.size() < samplerArray.max_size()))
            {
                samplerSetWrites.back()[samplerArray.size()] = {
                    .m_handle = sampler.m_handle,
                };
                samplerArray.push_back(sampler);
            }

            descriptorSetIndex++;
        }

        for (auto i = 0u; i < textureSetWrites.size(); i++)
        {
            const size_t spanSize = i + 1 == textureSetWrites.size() ? descriptorSetIndex : Modules::GuiLib::BasicGuiRenderer::kMaxTextureSlots;
            const DescriptorSetWriteInfo info[] = {
                {
                    .m_index = _descriptorSetIndices[0],
                    .m_descriptorData = { textureSetWrites[i].begin(), spanSize },
                },
                {
                    .m_index = _descriptorSetIndices[1],
                    .m_descriptorData = { samplerSetWrites[i].begin(), samplerData[i].size() },
                }
            };
            _graphicsContext.UpdateDescriptorSet(_texturesDescriptorSets[i], info, true);
        }

        return { eastl::move(textureData), eastl::move(samplerData) };
    }
}

namespace KryneEngine::Modules::GuiLib
{
    BasicGuiRenderer::BasicGuiRenderer(
        AllocatorInstance _allocator,
        GraphicsContext* _graphicsContext,
        RenderPassHandle _renderPass,
        SamplerHandle _defaultSampler)
        : m_instanceDataBuffer(_allocator)
        , m_commonConstantBuffer(_allocator)
        , m_commonConstantBufferViews(_allocator)
        , m_texturesDescriptorSets(_allocator)
        , m_defaultSampler(_defaultSampler)
    {
        KE_ZoneScoped("BasicGuiRenderer initialization");

        const u8 frameContextCount = _graphicsContext->GetFrameContextCount();

        {
            m_instanceDataBuffer.Init(
               _graphicsContext,
               BufferCreateDesc {
                   .m_desc = {
                       .m_size = 256,
#if !defined(KE_FINAL)
                       .m_debugName = "BasicGuiRenderer instance data buffer",
#endif
                   },
                   .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::VertexBuffer
               },
               frameContextCount);
        }

        {
            m_commonConstantBuffer.Init(
                _graphicsContext,
                BufferCreateDesc {
                    .m_desc = {
                        .m_size = sizeof(ViewportConstants),
#if !defined(KE_FINAL)
                        .m_debugName = "BasicGuiRenderer common constant buffer",
#endif
                    },
                    .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::ConstantBuffer
                },
                frameContextCount);

            m_commonConstantBufferViews.Resize(frameContextCount);
            for (auto i = 0u; i < frameContextCount; ++i)
            {
                m_commonConstantBufferViews[i] = _graphicsContext->CreateBufferView(
                    BufferViewDesc {
                        .m_buffer = m_commonConstantBuffer.GetBuffer(i),
                        .m_size = sizeof(ViewportConstants),
                        .m_offset = 0,
                        .m_stride = sizeof(ViewportConstants),
                        .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                        .m_debugName = "BasicGuiRenderer common constant buffer view",
#endif
                    });
            }
        }

        if (m_defaultSampler == GenPool::kInvalidHandle)
        {
            m_defaultSampler = _graphicsContext->CreateSampler({});
        }

        m_textSampler = _graphicsContext->CreateSampler({});

        DescriptorSetLayoutHandle commonDescriptorSetLayout;
        {
            constexpr DescriptorBindingDesc descriptorSet0Bindings[] = {
                {
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Vertex | ShaderVisibility::Fragment
                }
            };
            commonDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = descriptorSet0Bindings },
                m_commonDescriptorSetIndices.data());

            constexpr DescriptorBindingDesc descriptorSet1Bindings[] = {
                {
                    .m_type = DescriptorBindingDesc::Type::SampledTexture,
                    .m_visibility = ShaderVisibility::Fragment,
                    .m_count = kMaxTextureSlots,
                },
                {
                    .m_type = DescriptorBindingDesc::Type::Sampler,
                    .m_visibility = ShaderVisibility::Fragment,
                    .m_count = kMaxSamplerSlots,
                }
            };
            m_texturesDescriptorSetLayout = _graphicsContext->CreateDescriptorSetLayout(
                { .m_bindings = descriptorSet1Bindings },
                m_texturesDescriptorSetIndices.data());

            const DescriptorSetLayoutHandle descriptorSetLayouts[] = { commonDescriptorSetLayout, m_texturesDescriptorSetLayout };
            m_commonPipelineLayout = _graphicsContext->CreatePipelineLayout({
                .m_descriptorSets = descriptorSetLayouts,
            });

            m_commonDescriptorSet = _graphicsContext->CreateDescriptorSet(commonDescriptorSetLayout);
            m_texturesDescriptorSets.push_back(_graphicsContext->CreateDescriptorSet(m_texturesDescriptorSetLayout));
        }

        constexpr VertexLayoutElement commonVertexElements[] = {
            // Packed rect
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Position,
                .m_semanticIndex = 0,
                .m_bindingIndex = 0,
                .m_format = TextureFormat::RG32_UInt,
                .m_offset = offsetof(PackedInstanceData, m_packedRect),
                .m_location = 0,
            },
            // Packed color
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Color,
                .m_semanticIndex = 0,
                .m_bindingIndex = 0,
                .m_format = TextureFormat::R32_UInt,
                .m_offset = offsetof(PackedInstanceData, m_packedColor),
                .m_location = 1,
            },
            // Packed data
            {
                .m_semanticName = VertexLayoutElement::SemanticName::Uv,
                .m_semanticIndex = 0,
                .m_bindingIndex = 0,
                .m_format = TextureFormat::RGBA32_UInt,
                .m_offset = offsetof(PackedInstanceData, m_packedData),
                .m_location = 2,
            },
        };

        constexpr VertexBindingDesc commonVertexBindings[] = {
            {
                .m_stride = sizeof(PackedInstanceData),
                .m_inputRate = VertexInputRate::Instance,
            }
        };

        const auto readShaderFile = [_allocator](const eastl::string_view _filePath) -> eastl::span<char>
        {
            std::ifstream file(_filePath.data(), std::ios::binary);
            VERIFY_OR_RETURN(file, {});

            file.seekg(0, std::ios::end);
            const std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            auto* buffer = _allocator.Allocate<char>(size);
            KE_VERIFY(file.read(buffer, size));

            file.close();

            return { buffer, static_cast<size_t>(size) };
        };

        GraphicsPipelineDesc pipelineDesc {
            .m_vertexInput = {
                .m_elements = commonVertexElements,
                .m_bindings = commonVertexBindings
            },
            .m_colorBlending = {
                .m_attachments = { kDefaultColorAttachmentAlphaBlendDesc }
            },
            .m_depthStencil = {
                .m_depthTest = false,
                .m_depthWrite = false,
            },
            .m_renderPass =  _renderPass,
            .m_pipelineLayout = m_commonPipelineLayout,
        };

        // Rectangle PSO
        {
            const eastl::span<char> vertexShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Rectangle_RectangleVs.", _allocator) + GraphicsContext::GetShaderFileExtension());
            const eastl::span<char> fragmentShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Rectangle_RectangleFs.", _allocator) + GraphicsContext::GetShaderFileExtension());

            const ShaderModuleHandle vertexShaderModule = _graphicsContext->RegisterShaderModule(vertexShaderSource.data(), vertexShaderSource.size());
            const ShaderModuleHandle fragmentShaderModule = _graphicsContext->RegisterShaderModule(fragmentShaderSource.data(), fragmentShaderSource.size());

            const ShaderStage stages[] = {
                {
                    .m_shaderModule = vertexShaderModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "RectangleVs",
                },
                {
                    .m_shaderModule = fragmentShaderModule,
                    .m_stage = ShaderStage::Stage::Fragment,
                    .m_entryPoint = "RectangleFs",
                },
            };

            pipelineDesc.m_stages = stages;
            m_rectanglePipeline = _graphicsContext->CreateGraphicsPipeline(pipelineDesc);

            _graphicsContext->FreeShaderModule(fragmentShaderModule);
            _graphicsContext->FreeShaderModule(vertexShaderModule);
            _allocator.deallocate(fragmentShaderSource.data(), fragmentShaderSource.size());
            _allocator.deallocate(vertexShaderSource.data(), vertexShaderSource.size());
        }

        // Border PSO
        {
            const eastl::span<char> vertexShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Border_BorderVs.", _allocator) + GraphicsContext::GetShaderFileExtension());
            const eastl::span<char> fragmentShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Border_BorderFs.", _allocator) + GraphicsContext::GetShaderFileExtension());

            const ShaderModuleHandle vertexShaderModule = _graphicsContext->RegisterShaderModule(vertexShaderSource.data(), vertexShaderSource.size());
            const ShaderModuleHandle fragmentShaderModule = _graphicsContext->RegisterShaderModule(fragmentShaderSource.data(), fragmentShaderSource.size());

            const ShaderStage stages[] = {
                {
                    .m_shaderModule = vertexShaderModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "BorderVs",
                },
                {
                    .m_shaderModule = fragmentShaderModule,
                    .m_stage = ShaderStage::Stage::Fragment,
                    .m_entryPoint = "BorderFs",
                },
            };

            pipelineDesc.m_stages = stages;
            m_borderPipeline = _graphicsContext->CreateGraphicsPipeline(pipelineDesc);

            _graphicsContext->FreeShaderModule(fragmentShaderModule);
            _graphicsContext->FreeShaderModule(vertexShaderModule);
            _allocator.deallocate(fragmentShaderSource.data(), fragmentShaderSource.size());
            _allocator.deallocate(vertexShaderSource.data(), vertexShaderSource.size());
        }

        // Image PSO
        {
            const eastl::span<char> vertexShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Image_ImageVs.", _allocator) + GraphicsContext::GetShaderFileExtension());
            const eastl::span<char> fragmentShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Image_ImageFs.", _allocator) + GraphicsContext::GetShaderFileExtension());

            const ShaderModuleHandle vertexShaderModule = _graphicsContext->RegisterShaderModule(vertexShaderSource.data(), vertexShaderSource.size());
            const ShaderModuleHandle fragmentShaderModule = _graphicsContext->RegisterShaderModule(fragmentShaderSource.data(), fragmentShaderSource.size());

            const ShaderStage stages[] = {
                {
                    .m_shaderModule = vertexShaderModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "ImageVs",
                },
                {
                    .m_shaderModule = fragmentShaderModule,
                    .m_stage = ShaderStage::Stage::Fragment,
                    .m_entryPoint = "ImageFs",
                },
            };

            pipelineDesc.m_stages = stages;
            m_imagePipeline = _graphicsContext->CreateGraphicsPipeline(pipelineDesc);

            _graphicsContext->FreeShaderModule(fragmentShaderModule);
            _graphicsContext->FreeShaderModule(vertexShaderModule);
            _allocator.deallocate(fragmentShaderSource.data(), fragmentShaderSource.size());
            _allocator.deallocate(vertexShaderSource.data(), vertexShaderSource.size());
        }

        // Text PSO
        {
            const eastl::span<char> vertexShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Text_TextVs.", _allocator) + GraphicsContext::GetShaderFileExtension());
            const eastl::span<char> fragmentShaderSource = readShaderFile(
                eastl::string("Shaders/BasicGuiRenderer/Text_TextFs.", _allocator) + GraphicsContext::GetShaderFileExtension());

            const ShaderModuleHandle vertexShaderModule = _graphicsContext->RegisterShaderModule(vertexShaderSource.data(), vertexShaderSource.size());
            const ShaderModuleHandle fragmentShaderModule = _graphicsContext->RegisterShaderModule(fragmentShaderSource.data(), fragmentShaderSource.size());

            const ShaderStage stages[] = {
                {
                    .m_shaderModule = vertexShaderModule,
                    .m_stage = ShaderStage::Stage::Vertex,
                    .m_entryPoint = "TextVs",
                },
                {
                    .m_shaderModule = fragmentShaderModule,
                    .m_stage = ShaderStage::Stage::Fragment,
                    .m_entryPoint = "TextFs",
                },
            };

            pipelineDesc.m_stages = stages;
            m_textPipeline = _graphicsContext->CreateGraphicsPipeline(pipelineDesc);

            _graphicsContext->FreeShaderModule(fragmentShaderModule);
            _graphicsContext->FreeShaderModule(vertexShaderModule);
            _allocator.deallocate(fragmentShaderSource.data(), fragmentShaderSource.size());
            _allocator.deallocate(vertexShaderSource.data(), vertexShaderSource.size());
        }

        _graphicsContext->DestroyDescriptorSetLayout(commonDescriptorSetLayout);
    }

    void BasicGuiRenderer::BeginLayout(const float4x4& _viewportTransform, const uint2& _viewportSize)
    {
        Clay_BeginLayout();

        m_viewportConstants.ndcProjectionMatrix = _viewportTransform;
        m_viewportConstants.viewportSize = float2(_viewportSize);
    }

    void BasicGuiRenderer::EndLayoutAndRender(
        GraphicsContext& _graphicsContext,
        CommandListHandle _transferCommandList,
        CommandListHandle _renderCommandList)
    {
        KE_ZoneScopedFunction("BasicGuiRenderer::EndLayoutAndRender");

        const Clay_RenderCommandArray renderCommandArray = Clay_EndLayout();

        const u8 frameIndex = _graphicsContext.GetCurrentFrameContextIndex();

        {
            KE_ASSERT(m_commonConstantBuffer.GetSize(frameIndex) == sizeof(ViewportConstants));

            memcpy(m_commonConstantBuffer.Map(&_graphicsContext, frameIndex), &m_viewportConstants, sizeof(ViewportConstants));
            m_commonConstantBuffer.Unmap(&_graphicsContext);
            m_commonConstantBuffer.PrepareBuffers(&_graphicsContext, _transferCommandList, BarrierAccessFlags::ConstantBuffer, frameIndex);

            const DescriptorSetWriteInfo::DescriptorData descriptorData { .m_handle = m_commonConstantBufferViews[frameIndex].m_handle };
            const DescriptorSetWriteInfo writes[] = {
                {
                    .m_index = m_commonDescriptorSetIndices[0],
                    .m_descriptorData = { &descriptorData, 1 },
                }
            };
            _graphicsContext.UpdateDescriptorSet(m_commonDescriptorSet, writes, true);
            _graphicsContext.DeclarePassBufferViewUsage(_renderCommandList, { &m_commonConstantBufferViews[frameIndex], 1 }, BufferViewAccessType::Read);
        }

        if (m_textDescriptorSet == GenPool::kInvalidHandle && m_atlasManager != nullptr)
        {
            m_textDescriptorSet = _graphicsContext.CreateDescriptorSet(m_texturesDescriptorSetLayout);

            const DescriptorSetWriteInfo::DescriptorData atlasViewData = { .m_handle = m_atlasManager->GetAtlasView().m_handle };
            const DescriptorSetWriteInfo::DescriptorData textSamplerData = { .m_handle = m_textSampler.m_handle };

            const DescriptorSetWriteInfo writes[] = {
                {
                    .m_index = m_texturesDescriptorSetIndices[0],
                    .m_descriptorData = { &atlasViewData, 1 },
                },
                {
                    .m_index = m_texturesDescriptorSetIndices[1],
                    .m_descriptorData = { &textSamplerData, 1 },
                }
            };
            _graphicsContext.UpdateDescriptorSet(m_textDescriptorSet, writes, false);
        }

        TextureDataMap textureDataMap;
        SamplerDataMap samplerDataMap;
        {
            const auto pair = HandleTextureSets(
                m_texturesDescriptorSets.get_allocator(),
                _graphicsContext,
                renderCommandArray,
                m_texturesDescriptorSets,
                m_texturesDescriptorSetLayout,
                m_defaultSampler,
                m_texturesDescriptorSetIndices.data());
            textureDataMap = eastl::move(pair.first);
            samplerDataMap = eastl::move(pair.second);

            for (const auto [textureViewRawHandle, _]: textureDataMap)
            {
                TextureViewHandle handle { GenPool::Handle(textureViewRawHandle) };
                _graphicsContext.DeclarePassTextureViewUsage(_renderCommandList, { &handle, 1 }, TextureViewAccessType::Read);
            }
        }

        if (m_atlasManager != nullptr)
        {
            TextureViewHandle atlasView = m_atlasManager->GetAtlasView();
            _graphicsContext.DeclarePassTextureViewUsage(_renderCommandList, { &atlasView, 1 }, TextureViewAccessType::Read);
        }

        size_t sizeEstimation = 0;
        for (u32 i = 0; i < renderCommandArray.length; i++)
        {
            const Clay_RenderCommand& renderCommand = renderCommandArray.internalArray[i];

            switch (renderCommand.commandType)
            {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                    sizeEstimation += sizeof(PackedInstanceData);
                    break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                    sizeEstimation += sizeof(PackedInstanceData) * renderCommand.renderData.text.stringContents.length;
                    break;
                default:
                    break;
            }
        }
        const u64 sizeRequirement = Alignment::NextPowerOfTwo(sizeEstimation);
        if (m_instanceDataBuffer.GetSize(frameIndex) < sizeRequirement)
        {
            m_instanceDataBuffer.RequestResize(sizeRequirement);
        }
        auto* buffer = static_cast<std::byte*>(m_instanceDataBuffer.Map(&_graphicsContext, frameIndex));
        size_t offset = 0;

        constexpr auto packCornerRadii = [](const Clay_CornerRadius& _cornerRadius)
        {
            uint2 packedRadii {};
            using Math::Float16;
            packedRadii.x = Float16::PackFloat16x2(_cornerRadius.topLeft, _cornerRadius.topRight);
            packedRadii.y = Float16::PackFloat16x2(_cornerRadius.bottomLeft, _cornerRadius.bottomRight);
            return packedRadii;
        };

        Clay_RenderCommandType previous = CLAY_RENDER_COMMAND_TYPE_NONE;

        const BufferSpan bufferView {
            .m_size = sizeEstimation,
            .m_stride = sizeof(PackedInstanceData),
            .m_buffer = m_instanceDataBuffer.GetBuffer(frameIndex),
        };
        _graphicsContext.SetVertexBuffers(_renderCommandList, { &bufferView, 1 });

        size_t texturesDescriptorSetIndex = 0;
        const DescriptorSetHandle descriptorSets[] = { m_commonDescriptorSet, m_texturesDescriptorSets[0] };
        _graphicsContext.SetGraphicsDescriptorSets(_renderCommandList, m_commonPipelineLayout, descriptorSets);

        eastl::fixed_vector<Rect, 16, false> scissors;
        if (m_viewportConstants.ndcProjectionMatrix == float4x4())
        {
            _graphicsContext.SetViewport(_renderCommandList, {
               .m_width = static_cast<s32>(m_viewportConstants.viewportSize.x),
               .m_height = static_cast<s32>(m_viewportConstants.viewportSize.y)
           });

            scissors.push_back({
                .m_left = 0,
                .m_top = 0,
                .m_right = static_cast<u32>(m_viewportConstants.viewportSize.x),
                .m_bottom = static_cast<u32>(m_viewportConstants.viewportSize.y)
            });
            _graphicsContext.SetScissorsRect(_renderCommandList, scissors.back());
        }

        {
            KE_ZoneScoped("Fill instance data and dispatch render commands");
            for (u32 i = 0; i < renderCommandArray.length; i++)
            {
                const Clay_RenderCommand& renderCommand = renderCommandArray.internalArray[i];

                const float2 halfSize { 0.5f * renderCommand.boundingBox.width, 0.5f * renderCommand.boundingBox.height };
                const float2 center = float2(renderCommand.boundingBox.x, renderCommand.boundingBox.y) + halfSize;
                const uint2 packedRect = {
                    Math::Float16::PackFloat16x2(center.x, center.y),
                    Math::Float16::PackFloat16x2(halfSize.x, halfSize.y),
                };

                switch (renderCommand.commandType)
                {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                {
                    auto* packedInstanceData = reinterpret_cast<PackedInstanceData*>(buffer + offset);
                    packedInstanceData->m_packedRect = packedRect;
                    packedInstanceData->m_packedColor = Color(
                        renderCommand.renderData.rectangle.backgroundColor.r / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.g / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.b / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.a / 255.f).ToSrgb().ToRgba8();
                    const uint2 packedRadii = packCornerRadii(renderCommand.renderData.rectangle.cornerRadius);
                    packedInstanceData->m_packedData.x = packedRadii.x;
                    packedInstanceData->m_packedData.y = packedRadii.y;

                    if (previous != CLAY_RENDER_COMMAND_TYPE_RECTANGLE)
                    {
                        _graphicsContext.SetGraphicsPipeline(_renderCommandList, m_rectanglePipeline);
                    }
                    _graphicsContext.DrawInstanced(_renderCommandList, DrawInstancedDesc {
                        .m_vertexCount = 6,
                        .m_instanceOffset = static_cast<u32>(offset / sizeof(PackedInstanceData)),
                    });
                    offset += sizeof(PackedInstanceData);

                    previous = CLAY_RENDER_COMMAND_TYPE_RECTANGLE;
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                {
                    auto* packedInstanceData = reinterpret_cast<PackedInstanceData*>(buffer + offset);
                    packedInstanceData->m_packedRect = packedRect;
                    packedInstanceData->m_packedColor = Color(
                        renderCommand.renderData.border.color.r / 255.f,
                        renderCommand.renderData.border.color.g / 255.f,
                        renderCommand.renderData.border.color.b / 255.f,
                        renderCommand.renderData.border.color.a / 255.f).ToSrgb().ToRgba8();
                    const uint2 packedRadii = packCornerRadii(renderCommand.renderData.border.cornerRadius);
                    packedInstanceData->m_packedData.x = packedRadii.x;
                    packedInstanceData->m_packedData.y = packedRadii.y;
                    packedInstanceData->m_packedData.z = Math::Float16::PackFloat16x2(
                        renderCommand.renderData.border.width.top, renderCommand.renderData.border.width.bottom);
                    packedInstanceData->m_packedData.w = Math::Float16::PackFloat16x2(
                        renderCommand.renderData.border.width.left, renderCommand.renderData.border.width.right);

                    if (previous != CLAY_RENDER_COMMAND_TYPE_BORDER)
                    {
                        _graphicsContext.SetGraphicsPipeline(_renderCommandList, m_borderPipeline);
                    }
                    _graphicsContext.DrawInstanced(_renderCommandList, DrawInstancedDesc {
                        .m_vertexCount = 6,
                        .m_instanceOffset = static_cast<u32>(offset / sizeof(PackedInstanceData)),
                    });
                    offset += sizeof(PackedInstanceData);

                    previous = CLAY_RENDER_COMMAND_TYPE_BORDER;
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                {
                    if (m_atlasManager == nullptr)
                        break;

                    const eastl::string_view stringContents {
                        renderCommand.renderData.text.stringContents.chars,
                        static_cast<size_t>(renderCommand.renderData.text.stringContents.length)
                    };

                    TextRendering::Font* font = m_atlasManager->GetFontManager()->GetFont(renderCommand.renderData.text.fontId);
                    const float fontSize = renderCommand.renderData.text.fontSize;

                    float2_simd writePoint {
                        renderCommand.boundingBox.x,
                        renderCommand.boundingBox.y + font->GetAscender(fontSize)
                    };
                    for (Utf8Iterator utf8Iterator { stringContents }; utf8Iterator != stringContents.end(); ++utf8Iterator)
                    {
                        const TextRendering::GlyphLayoutMetrics glyphLayoutMetrics = font->GetGlyphLayoutMetrics(*utf8Iterator, fontSize);

                        constexpr float msdfFontSize = 32;
                        const TextRendering::MsdfAtlasManager::GlyphRegion glyphRegion =
                            m_atlasManager->GetGlyphRegion(font, *utf8Iterator, msdfFontSize);

                        // If the glyph has no visuals (special chars like space or tab), simply advance write point and
                        // don't render anything.
                        if (!glyphRegion.IsValid())
                        {
                            writePoint.x += glyphLayoutMetrics.m_advanceX;
                            continue;
                        }

                        const float scale = fontSize / msdfFontSize;

                        const float2_simd glyphHalfSize {
                            static_cast<float>(glyphRegion.m_width) * 0.5f * scale,
                            static_cast<float>(glyphRegion.m_height) * 0.5f * scale,
                        };
                        const float2_simd glyphOffset {
                            -static_cast<float>(glyphRegion.m_pxRange) * 0.5f * scale + glyphLayoutMetrics.m_bearingX,
                            -static_cast<float>(glyphRegion.m_baseline) * scale,
                        };
                        const float2_simd glyphCenter = writePoint + glyphOffset + glyphHalfSize;

                        const uint2 glyphPackedRect = {
                            Math::Float16::PackFloat16x2(glyphCenter.x, glyphCenter.y),
                            Math::Float16::PackFloat16x2(glyphHalfSize.x, glyphHalfSize.y),
                        };

                        auto* packedInstanceData = reinterpret_cast<PackedInstanceData*>(buffer + offset);

                        packedInstanceData->m_packedRect = glyphPackedRect;

                        Color tintColor {
                            renderCommand.renderData.text.textColor.r / 255.f,
                            renderCommand.renderData.text.textColor.g / 255.f,
                            renderCommand.renderData.text.textColor.b / 255.f,
                            renderCommand.renderData.text.textColor.a / 255.f,
                        };
                        if (tintColor.m_value == float4(0))
                        {
                            tintColor = Color(float4(1));
                        }
                        packedInstanceData->m_packedColor = tintColor.ToSrgb().ToRgba8();

                        packedInstanceData->m_packedData.x = BitUtils::BitfieldInsert<u32>(glyphRegion.m_x, glyphRegion.m_y, 16, 16);
                        packedInstanceData->m_packedData.y = BitUtils::BitfieldInsert<u32>(glyphRegion.m_width + glyphRegion.m_x, glyphRegion.m_height + glyphRegion.m_y, 16, 16);
                        packedInstanceData->m_packedData.z = glyphRegion.m_pxRange;

                        if (previous != CLAY_RENDER_COMMAND_TYPE_TEXT)
                        {
                            _graphicsContext.SetGraphicsPipeline(_renderCommandList, m_textPipeline);
                            previous = CLAY_RENDER_COMMAND_TYPE_TEXT;
                        }

                        // We use the textures descriptor sets size as the index for the text descriptor set
                        if (texturesDescriptorSetIndex != m_texturesDescriptorSets.size())
                        {
                            _graphicsContext.SetGraphicsDescriptorSetsWithOffset(
                                _renderCommandList,
                                m_commonPipelineLayout,
                                { &m_textDescriptorSet, 1 },
                                1);
                            texturesDescriptorSetIndex = m_texturesDescriptorSets.size();
                        }

                        _graphicsContext.DrawInstanced(_renderCommandList, DrawInstancedDesc {
                            .m_vertexCount = 6,
                            .m_instanceOffset = static_cast<u32>(offset / sizeof(PackedInstanceData)),
                        });
                        offset += sizeof(PackedInstanceData);

                        writePoint.x += glyphLayoutMetrics.m_advanceX;
                    }
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                {
                    auto* packedInstanceData = reinterpret_cast<PackedInstanceData*>(buffer + offset);
                    const auto* textureRegion = static_cast<const TextureRegion*>(renderCommand.renderData.image.imageData);

                    packedInstanceData->m_packedRect = packedRect;

                    Color tintColor {
                        renderCommand.renderData.rectangle.backgroundColor.r / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.g / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.b / 255.f,
                        renderCommand.renderData.rectangle.backgroundColor.a / 255.f,
                    };
                    if (tintColor.m_value == float4(0))
                    {
                        tintColor = Color(float4(1));
                    }
                    packedInstanceData->m_packedColor = tintColor.ToSrgb().ToRgba8();

                    // Pack texture and sampler indices
                    {
                        const auto it = textureDataMap.find(static_cast<u32>(textureRegion->m_textureView.m_handle));
                        if (!KE_VERIFY(it != textureDataMap.end())) break;
                        if (texturesDescriptorSetIndex != it->second.m_descriptorSetIndex)
                        {
                            texturesDescriptorSetIndex = it->second.m_descriptorSetIndex;
                            _graphicsContext.SetGraphicsDescriptorSetsWithOffset(
                                _renderCommandList,
                                m_commonPipelineLayout,
                                { m_texturesDescriptorSets.begin() + texturesDescriptorSetIndex, 1 },
                                1);
                        }

                        const SamplerArray& array = samplerDataMap[it->second.m_descriptorSetIndex];
                        const SamplerHandle samplerHandle = textureRegion->m_customSampler != GenPool::kInvalidHandle ? textureRegion->m_customSampler : m_defaultSampler;
                        const auto samplerIt = eastl::find(array.begin(), array.end(), samplerHandle);
                        if (!KE_VERIFY(samplerIt != array.end())) break;
                        packedInstanceData->m_packedData.x = BitUtils::BitfieldInsert<u32>(
                            it->second.m_index,
                            eastl::distance(array.begin(), samplerIt),
                            3,
                            5);
                    }

                    // Pack corner radii
                    {
                        float4 cornerRadii = *reinterpret_cast<const float4*>(&renderCommand.renderData.image.cornerRadius);
                        for (auto j = 0u; j < 4; j++)
                        {
                            KE_ASSERT(cornerRadii[j] >= 0.f);
                            KE_ASSERT_MSG(cornerRadii[j] <= 4095, "Max supported border radius size is 4095");
                        }
                        cornerRadii.MinComponents(float4(eastl::min(halfSize.x, halfSize.y)));
                        packedInstanceData->m_packedData.x = BitUtils::BitfieldInsert<u32>(
                            packedInstanceData->m_packedData.x,
                            static_cast<u16>(std::roundf(cornerRadii.x)),
                            12,
                            8);
                        packedInstanceData->m_packedData.x = BitUtils::BitfieldInsert<u32>(
                            packedInstanceData->m_packedData.x,
                            static_cast<u16>(std::roundf(cornerRadii.y)),
                            12,
                            20);
                        packedInstanceData->m_packedData.y = BitUtils::BitfieldInsert<u32>(
                            static_cast<u16>(std::roundf(cornerRadii.z)),
                            static_cast<u16>(std::roundf(cornerRadii.w)),
                            12,
                            12);
                    }

                    // Pack region rect
                    {
                        const float2 regionHalfSize = textureRegion->m_size * float2(0.5f);
                        const float2 regionCenter = textureRegion->m_offset + regionHalfSize;

                        packedInstanceData->m_packedData.z = Math::Float16::PackFloat16x2(regionCenter.x, regionCenter.y);
                        packedInstanceData->m_packedData.w = Math::Float16::PackFloat16x2(regionHalfSize.x, regionHalfSize.y);
                    }

                    if (previous != CLAY_RENDER_COMMAND_TYPE_IMAGE)
                        _graphicsContext.SetGraphicsPipeline(_renderCommandList, m_imagePipeline);

                    _graphicsContext.DrawInstanced(_renderCommandList, DrawInstancedDesc {
                        .m_vertexCount = 6,
                        .m_instanceOffset = static_cast<u32>(offset / sizeof(PackedInstanceData)),
                    });
                    offset += sizeof(PackedInstanceData);

                    previous = CLAY_RENDER_COMMAND_TYPE_IMAGE;
                    break;
                }
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                    if (KE_VERIFY(scissors.size() < scissors.capacity())) [[likely]]
                    {
                        scissors.push_back({
                            .m_left = static_cast<u32>(renderCommand.boundingBox.x),
                            .m_top = static_cast<u32>(renderCommand.boundingBox.y),
                            .m_right = static_cast<u32>(renderCommand.boundingBox.x + renderCommand.boundingBox.width),
                            .m_bottom = static_cast<u32>(renderCommand.boundingBox.y + renderCommand.boundingBox.height),
                        });
                        _graphicsContext.SetScissorsRect(_renderCommandList, scissors.back());
                    }
                    previous = CLAY_RENDER_COMMAND_TYPE_SCISSOR_START;
                    break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                    if (KE_VERIFY(scissors.size() > 1)) [[likely]]
                    {
                        scissors.pop_back();
                        _graphicsContext.SetScissorsRect(_renderCommandList, scissors.back());
                    }
                    previous = CLAY_RENDER_COMMAND_TYPE_SCISSOR_END;
                    break;
                case CLAY_RENDER_COMMAND_TYPE_NONE:
                    previous = CLAY_RENDER_COMMAND_TYPE_NONE;
                    break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                    previous = CLAY_RENDER_COMMAND_TYPE_CUSTOM;
                    break;
                }
            }
        }

        KE_ASSERT(scissors.size() == 1 || m_viewportConstants.ndcProjectionMatrix != float4x4());

        {
            KE_ZoneScoped("Upload instance data");
            m_instanceDataBuffer.Unmap(&_graphicsContext);
            m_instanceDataBuffer.PrepareBuffers(&_graphicsContext, _transferCommandList, BarrierAccessFlags::VertexBuffer, frameIndex);
        }
    }

} // namespace KryneEngine