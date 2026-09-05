/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#include "SceneManager.hpp"

#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "PassTypes.hpp"
#include "RenderTargetFormats.hpp"
#include "Rendering/Fullscreen/FullscreenPassConstants.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp>
#include <fstream>

namespace KryneEngine::Samples::PhysicsDemo
{
    SceneManager::SceneManager(
        const AllocatorInstance _allocator,
        GraphicsContext& _graphicsContext,
        FibersManager* _fibersManager,
        const b3WorldId _world)
            : m_allocator(_allocator)
            , m_fibersManager(_fibersManager)
            , m_world(_world)
            , m_drawInstanceManager(_allocator, _graphicsContext)
            , m_materialManager(_allocator, static_cast<u8>(PassTypes::Count))
            , m_gameFramesQueue(_allocator, 3)
            , m_fullscreenConstantsBuffer(_allocator)
            , m_deferredShadingPass(_allocator)
            , m_skyPass(_allocator)
            , m_colorMappingPass(_allocator)
    {
        m_gBufferPassDispatcher = m_drawInstanceManager.CreatePassDispatcher(
            _graphicsContext,
            &m_materialManager,
            static_cast<u8>(PassTypes::GBufferPass),
            "GBuffer pass dispatcher");

        m_defaultMaterial = m_materialManager.RegisterMaterial();
    }

    void SceneManager::Process(GraphicsContext* _graphicsContext, const float _deltaTime)
    {
        KE_ZoneScopedFunction("SceneManager::Process");

        m_timeProgress += _deltaTime;
        const bool gameLoopRunning = !m_gameFramesQueue.Empty();

        bool queuedGameLoopFrame = false;
        while (m_timeProgress >= m_physicsTimeStep)
        {
            m_timeProgress -= m_physicsTimeStep;
            if (m_gameFramesQueue.TryEmplace(m_gameFrameId))
            {
                queuedGameLoopFrame = true;
                ++m_gameFrameId;
            }
        }

        if (!gameLoopRunning && queuedGameLoopFrame)
            m_fibersManager->InitAndBatchJobsNoCounter({
                .m_function = [this](u16) { GameLoop(); },
                .m_priority = FiberJob::Priority::High,
            });

        // Update fullscreen passes
        {
            const DescriptorSetWriteInfo::DescriptorData descriptorData[] = {
                {
                    .m_handle = m_fullscreenConstantsBufferViews[_graphicsContext->GetCurrentFrameContextIndex()].m_handle,
                }
            };

            const DescriptorSetWriteInfo writeInfo[] = {
                {
                    .m_index = m_fullscreenPassesCbIdx,
                    .m_descriptorData = descriptorData,
                }
            };

            _graphicsContext->UpdateDescriptorSet(m_fullscreenDescriptorSet, writeInfo, true);

            m_deferredShadingPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
            m_skyPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
            m_colorMappingPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
        }
    }

    void SceneManager::GameLoop()
    {
        const u64* frameId = m_gameFramesQueue.Front();

        while (!m_gameFramesQueue.Empty())
        {
            KE_ZoneScopedF("Game loop frame %lld", *frameId);

            // Run physics
            {
                KE_ZoneScoped("Physics: World step");
                b3World_Step(m_world, m_physicsTimeStep, m_physicsSubSteps);
            }

            m_gameFramesQueue.Pop();
            frameId = m_gameFramesQueue.Front();
        }
    }

    void SceneManager::InitPso(
        GraphicsContext& _graphicsContext,
        const TextureViewHandle _gBuffer0View,
        const TextureViewHandle _gBuffer1View,
        const TextureViewHandle _gBuffer2View,
        const TextureViewHandle _gBufferDepthView,
        const TextureViewHandle _deferredShadowsView,
        const TextureViewHandle _hdrView)
    {
        // Default material PSOs
        {
            const auto readShaderFile = [this](const eastl::string_view _filePath) -> eastl::span<char>
            {
                std::ifstream file(_filePath.data(), std::ios::binary);
                KE_ASSERT(file);

                file.seekg(0, std::ios::end);
                const std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                auto* buffer = static_cast<char*>(m_allocator.allocate(size));
                if (!file.read(buffer, size)) return {};

                file.close();

                return { buffer, static_cast<size_t>(size) };
            };

            eastl::span<char> vertexBytecode, fragmentBytecode;
            ShaderModuleHandle vertexShader, fragmentShader;
            {
                char path[256];
                snprintf(path, sizeof(path), "Shaders/Samples/PhysicsDemo/Basic_MainVs.%s", GraphicsContext::GetShaderFileExtension());
                vertexBytecode = readShaderFile(path);
                snprintf(path, sizeof(path), "Shaders/Samples/PhysicsDemo/Basic_MainFs.%s", GraphicsContext::GetShaderFileExtension());
                fragmentBytecode = readShaderFile(path);

                vertexShader = _graphicsContext.RegisterShaderModule(vertexBytecode.data(), vertexBytecode.size());
                fragmentShader = _graphicsContext.RegisterShaderModule(fragmentBytecode.data(), fragmentBytecode.size());
            }

            PipelineLayoutHandle defaultPipelineLayout;
            {
                const DescriptorSetLayoutHandle sets[] = {
                    m_drawInstanceManager.GetPassDescriptorSetLayout(_graphicsContext)
                };

                defaultPipelineLayout = _graphicsContext.CreatePipelineLayout({
                    .m_descriptorSets = sets,
                });
            }
            m_materialManager.SetPipelineLayout(m_defaultMaterial, static_cast<u8>(PassTypes::GBufferPass), defaultPipelineLayout);
            m_materialManager.SetPipelineLayout(m_defaultMaterial, static_cast<u8>(PassTypes::ShadowPass), defaultPipelineLayout);

            GraphicsPipelineHandle defaultPipelineGBuffer, defaultPipelineShadow;
            {
                const ShaderStage shaderStages[2] = {
                    {
                        .m_shaderModule = vertexShader,
                        .m_stage = ShaderStage::Stage::Vertex,
                        .m_entryPoint = "MainVs",
                    },
                    {
                        .m_shaderModule = fragmentShader,
                        .m_stage = ShaderStage::Stage::Fragment,
                        .m_entryPoint = "MainFs",
                    }
                };

                constexpr VertexLayoutElement vertexLayoutElements[] = {
                    {
                        .m_semanticName = VertexLayoutElement::SemanticName::Position,
                        .m_bindingIndex = 0,
                        .m_format = TextureFormat::RGB32_Float,
                        .m_offset = 0,
                        .m_location = 0,
                    },
                    {
                        .m_semanticName = VertexLayoutElement::SemanticName::Normal,
                        .m_bindingIndex = 0,
                        .m_format = TextureFormat::RGB32_Float,
                        .m_offset = sizeof(float3),
                        .m_location = 1,
                    },
                    {
                        .m_semanticName = VertexLayoutElement::SemanticName::BoneIndices,
                        .m_bindingIndex = 1,
                        .m_format = TextureFormat::R32_UInt,
                        .m_offset = 0,
                        .m_location = 2,
                    }
                };

                constexpr VertexBindingDesc vertexBindings[] {
                    {
                        .m_stride = sizeof(float3) * 2,
                        .m_binding = 0,
                        .m_inputRate = VertexInputRate::Vertex,
                    },
                    {
                        .m_stride = sizeof(u32),
                        .m_binding = 1,
                        .m_inputRate = VertexInputRate::Instance,
                    }
                };

                defaultPipelineGBuffer = _graphicsContext.CreateGraphicsPipeline({
                    .m_stages = shaderStages,
                    .m_vertexInput = {
                        .m_elements = vertexLayoutElements,
                        .m_bindings = vertexBindings,
                    },
                    .m_colorBlending = {
                        .m_attachments = { ColorAttachmentBlendDesc {}, ColorAttachmentBlendDesc {}, ColorAttachmentBlendDesc {} },
                    },
                    .m_renderTargets = {
                        .m_numColorAttachments = 3,
                        .m_colorFormats = { kGBuffer0Format, kGBuffer1Format, kGBuffer2Format },
                        .m_depthStencilFormat = kGBufferDepthFormat,
                    },
                    .m_pipelineLayout = defaultPipelineLayout,
    #if !defined(KE_FINAL)
                    .m_debugName = "Default GBuffer PSO",
    #endif
                });

                defaultPipelineShadow = _graphicsContext.CreateGraphicsPipeline({
                    .m_stages = { shaderStages, 1 },
                    .m_vertexInput = {
                        .m_elements = vertexLayoutElements,
                        .m_bindings = vertexBindings,
                    },
                    .m_renderTargets = {
                        .m_numColorAttachments = 0,
                        .m_depthStencilFormat = kShadowFormat,
                    },
                    .m_pipelineLayout = defaultPipelineLayout,
    #if !defined(KE_FINAL)
                    .m_debugName = "Default Shadow PSO",
    #endif
                });
            }
            m_materialManager.SetGraphicsPipeline(m_defaultMaterial, static_cast<u8>(PassTypes::GBufferPass), defaultPipelineGBuffer);
            m_materialManager.SetGraphicsPipeline(m_defaultMaterial, static_cast<u8>(PassTypes::ShadowPass), defaultPipelineShadow);

            _graphicsContext.FreeShaderModule(fragmentShader);
            _graphicsContext.FreeShaderModule(vertexShader);
            m_allocator.deallocate(fragmentBytecode.data(), fragmentBytecode.size_bytes());
            m_allocator.deallocate(vertexBytecode.data(), vertexBytecode.size_bytes());
        }

        // Fullscreen passes
        {
            {
                constexpr DescriptorBindingDesc bindings[] = {
                    {
                        .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                        .m_visibility = ShaderVisibility::Fragment,
                    }
                };
                m_fullscreenPassesLayout = _graphicsContext.CreateDescriptorSetLayout({
                   .m_bindings = bindings,
                }, &m_fullscreenPassesCbIdx);
            }

            m_fullscreenDescriptorSet = _graphicsContext.CreateDescriptorSet(m_fullscreenPassesLayout);

            m_fullscreenConstantsBuffer.Init(
                &_graphicsContext,
                {
                    .m_desc = {
                        .m_size = sizeof(FullscreenPassConstants),
#if !defined(KE_FINAL)
                        .m_debugName = "FullscreenConstants",
#endif
                    },
                    .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer | MemoryUsage::ConstantBuffer,
                },
                _graphicsContext.GetFrameContextCount());

            m_fullscreenConstantsBufferViews = m_allocator.Allocate<BufferViewHandle>(_graphicsContext.GetFrameContextCount());
            for (u32 i = 0; i < _graphicsContext.GetFrameContextCount(); i++)
            {
                char name[256];
                snprintf(name, sizeof(name), "FullscreenConstantsBufferView_%u", i);
                m_fullscreenConstantsBufferViews[i] = _graphicsContext.CreateBufferView({
                    .m_buffer = m_fullscreenConstantsBuffer.GetBuffer(i),
                    .m_size = sizeof(FullscreenPassConstants),
                    .m_stride = sizeof(FullscreenPassConstants),
                    .m_accessType = BufferViewAccessType::Constant,
#if !defined(KE_FINAL)
                    .m_debugName = name,
#endif
                });
            }

            m_deferredShadingPass.Initialize(
                &_graphicsContext,
                m_fullscreenPassesLayout,
                _gBuffer0View,
                _gBuffer1View,
                _gBufferDepthView,
                _deferredShadowsView,
                _gBuffer2View);
            m_deferredShadingPass.CreatePso(&_graphicsContext, {
                .m_numColorAttachments = 1,
                .m_colorFormats = { kHdrFormat },
            });

            m_skyPass.Initialize(
                &_graphicsContext,
                m_fullscreenPassesLayout);
            m_skyPass.CreatePso(&_graphicsContext, {
                .m_numColorAttachments = 1,
                .m_colorFormats = { kHdrFormat },
                .m_depthStencilFormat = kGBufferDepthFormat,
            });

            m_colorMappingPass.Initialize(
                &_graphicsContext,
                m_fullscreenPassesLayout,
                _hdrView);
            m_colorMappingPass.CreatePso(&_graphicsContext, {
                .m_numColorAttachments = 1,
                .m_colorFormats = { _graphicsContext.GetPresentTextureFormat() },
            });
        }
    }
} // namespace KryneEngine::Samples::PhysicsDemo