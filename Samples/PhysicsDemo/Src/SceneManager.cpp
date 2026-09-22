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
#include "Scene/PrecariouslyStackingBoxesTemplate.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <Scene/OrbitCamera.hpp>
#include <Scene/SunLight.hpp>
#include <fstream>
#include <imgui.h>

namespace KryneEngine::Samples::PhysicsDemo
{
    SceneManager::SceneManager(
        const AllocatorInstance _allocator,
        GraphicsContext* _graphicsContext,
        const SwapChainHandle _mainSwapChainHandle,
        FibersManager* _fibersManager,
        const b3WorldId _world,
        const bool _singleThreadedMode)
            : m_allocator(_allocator)
            , m_fibersManager(_fibersManager)
            , m_world(_world)
            , m_singleThreadedMode(_singleThreadedMode)
            , m_drawInstanceManager(_allocator, *_graphicsContext)
            , m_materialManager(_allocator, static_cast<u8>(PassTypes::Count))
            , m_geometryLibrary(_allocator, *_graphicsContext)
            , m_worldObjectSystem(_allocator, _world)
            , m_cascadedShadowMap(_allocator)
            , m_gameFramesQueue(_allocator, 3)
            , m_sceneEntities(_allocator)
            , m_fullscreenConstantsBuffer(_allocator)
            , m_deferredShadingPass(_allocator)
            , m_skyPass(_allocator)
            , m_skyAmbientPass(_allocator)
            , m_colorMappingPass(_allocator)
            , m_deferredShadowPass(_allocator)
    {
        m_gBufferPassDispatcher = m_drawInstanceManager.CreatePassDispatcher(
            *_graphicsContext,
            &m_materialManager,
            static_cast<u8>(PassTypes::GBufferPass),
            "GBuffer pass dispatcher");

        for (auto i = 0; i < CascadedShadowMap::kMaxCascades; ++i)
        {
            char name[128];
            snprintf(name, sizeof(name), "Shadow pass dispatcher %d", i);
            m_shadowPassDispatchers[i] = m_drawInstanceManager.CreatePassDispatcher(
                *_graphicsContext,
                &m_materialManager,
                static_cast<u8>(PassTypes::ShadowPass),
                name);
        }

        m_defaultMaterial = m_materialManager.RegisterMaterial();

        // Registered once and shared by every scene template (see GeometryModelArray).
        for (u8 i = 0; i < static_cast<u8>(GeometryType::Count); ++i)
        {
            const GeometryBuffers& buffers = m_geometryLibrary.GetBuffers(static_cast<GeometryType>(i));
            m_geometryModels[i] = m_drawInstanceManager.RegisterModel(
                buffers.m_vertexBuffer,
                buffers.m_indexBuffer,
                m_defaultMaterial,
                buffers.m_indexCount);
        }

        const uint2 windowSize = _graphicsContext->GetSwapChainSize(_mainSwapChainHandle);
        const float aspectRatio = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
        m_orbitCamera = m_allocator.New<OrbitCamera>(aspectRatio);

        m_sunLight = m_allocator.New<SunLight>();
        m_sunLight->SetTheta(30.f);
        m_sunLight->SetPhi(30.f);
    }

    SceneManager::~SceneManager()
    {
        if (SceneTemplate* pending = m_templateToLoad.exchange(nullptr, std::memory_order_acq_rel))
        {
            m_allocator.Delete(pending);
        }
        m_allocator.Delete(m_currentSceneTemplate);

        if (m_fullscreenConstantsBufferViews != nullptr)
        {
            m_allocator.deallocate(m_fullscreenConstantsBufferViews);
        }
        m_allocator.Delete(m_sunLight);
        m_allocator.Delete(m_orbitCamera);
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
        {
            if (m_singleThreadedMode)
            {
                GameLoop();
            }
            else
            {
                m_fibersManager->InitAndBatchJobsNoCounter({
                    .m_function =
                        [this](u16)
                    {
                        GameLoop();
                    },
                    .m_priority = FiberJob::Priority::High,
                });
            }
        }

        m_geometryLibrary.Update(*_graphicsContext);

        DrawMenuBar();

        m_sunLight->Process();

        if (m_showSunLightWindow)
            m_sunLight->DebugWindow(&m_showSunLightWindow);
        if (m_showDeferredShadowsWindow)
            m_cascadedShadowMap.Debug(&m_showDeferredShadowsWindow);

        // Interpolate between the last two fixed steps' worth of data and push the result to the
        // renderer; see WorldObjectSystem's threading contract (OrbitCamera::UpdatePose()/
        // SyncRenderTransform() follow the exact same one) for why this must run on this thread.
        const float alpha = m_timeProgress / m_physicsTimeStep;
        m_orbitCamera->SyncRenderTransform(alpha);
        m_worldObjectSystem.SyncRenderInstances(m_drawInstanceManager, alpha);

        // Update fullscreen passes
        {
            const DescriptorSetWriteInfo::DescriptorData descriptorData[] = {{
                .m_handle = m_fullscreenConstantsBufferViews[_graphicsContext->GetCurrentFrameContextIndex()].m_handle,
            }};

            const DescriptorSetWriteInfo writeInfo[] = {{
                .m_index = m_fullscreenPassesCbIdx,
                .m_descriptorData = descriptorData,
            }};

            _graphicsContext->UpdateDescriptorSet(m_fullscreenDescriptorSet, writeInfo, true);

            m_deferredShadingPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
            m_skyPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
            m_skyAmbientPass.UpdateSceneConstants(
                _graphicsContext, m_fullscreenConstantsBufferViews[_graphicsContext->GetCurrentFrameContextIndex()]);
            m_colorMappingPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
            m_deferredShadowPass.UpdateSceneConstants(
                _graphicsContext,
                m_fullscreenConstantsBufferViews[_graphicsContext->GetCurrentFrameContextIndex()],
                m_cascadedShadowMap.GetConstantsBufferView(_graphicsContext->GetCurrentFrameContextIndex()));
        }
    }

    void SceneManager::GameLoop()
    {
        const u64* frameId = m_gameFramesQueue.Front();

        while (!m_gameFramesQueue.Empty())
        {
            KE_ZoneScopedF("Game loop frame %lld", *frameId);

            // Pick up any scene template switch requested from another thread since the last
            // step, and apply it now: this is the only thread allowed to touch WorldObjectSystem.
            if (SceneTemplate* pending = m_templateToLoad.exchange(nullptr, std::memory_order_acq_rel))
            {
                SwapScene(pending);
            }

            // Process Input
            {
                KE_ZoneScoped("Input: Process");

                const auto lock = m_inputLock.AutoLock();
                InputManager::Get().Update();
            }

            // OrbitCamera reads the input state InputManager::Update() just wrote above; both
            // must run on this same thread (see OrbitCamera::UpdatePose()'s own contract) so this
            // read is never concurrent with it.
            m_orbitCamera->UpdatePose();

            // Let the active template drive its own custom interactions before the physics world
            // is stepped (e.g. spawning entities, applying forces, reacting to input).
            if (m_currentSceneTemplate != nullptr)
            {
                KE_ZoneScoped("Scene template: Process");
                SceneBuildContext context(m_geometryLibrary, m_worldObjectSystem, m_geometryModels, m_sceneEntities);
                m_currentSceneTemplate->Process(context, m_physicsTimeStep);
            }

            // Run physics
            {
                KE_ZoneScoped("Physics: World step");
                b3World_Step(m_world, m_physicsTimeStep, m_physicsSubSteps);
                m_worldObjectSystem.Update();
            }

            m_gameFramesQueue.Pop();
            frameId = m_gameFramesQueue.Front();
        }
    }

    void SceneManager::InitPso(
        GraphicsContext& _graphicsContext,
        const TextureFormat _swapChainFormat,
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
                if (!file.read(buffer, size))
                {
                    return {};
                }

                file.close();

                return {buffer, static_cast<size_t>(size)};
            };

            eastl::span<char> vertexBytecode, fragmentBytecode;
            ShaderModuleHandle vertexShader, fragmentShader;
            {
                char path[256];
                snprintf(
                    path,
                    sizeof(path),
                    "Shaders/Samples/PhysicsDemo/Basic_MainVs.%s",
                    GraphicsContext::GetShaderFileExtension());
                vertexBytecode = readShaderFile(path);
                snprintf(
                    path,
                    sizeof(path),
                    "Shaders/Samples/PhysicsDemo/Basic_MainFs.%s",
                    GraphicsContext::GetShaderFileExtension());
                fragmentBytecode = readShaderFile(path);

                vertexShader = _graphicsContext.RegisterShaderModule(vertexBytecode.data(), vertexBytecode.size());
                fragmentShader =
                    _graphicsContext.RegisterShaderModule(fragmentBytecode.data(), fragmentBytecode.size());
            }

            PipelineLayoutHandle defaultPipelineLayout;
            {
                const DescriptorSetLayoutHandle sets[] = {
                    m_drawInstanceManager.GetPassDescriptorSetLayout(_graphicsContext)};

                defaultPipelineLayout = _graphicsContext.CreatePipelineLayout({
                    .m_descriptorSets = sets,
                });
            }
            m_materialManager.SetPipelineLayout(
                m_defaultMaterial, static_cast<u8>(PassTypes::GBufferPass), defaultPipelineLayout);
            m_materialManager.SetPipelineLayout(
                m_defaultMaterial, static_cast<u8>(PassTypes::ShadowPass), defaultPipelineLayout);

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
                    }};

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
                    }};

                constexpr VertexBindingDesc vertexBindings[]{
                    {
                        .m_stride = sizeof(float3) * 2,
                        .m_binding = 0,
                        .m_inputRate = VertexInputRate::Vertex,
                    },
                    {
                        .m_stride = sizeof(u32),
                        .m_binding = 1,
                        .m_inputRate = VertexInputRate::Instance,
                    }};

                defaultPipelineGBuffer = _graphicsContext.CreateGraphicsPipeline({
                    .m_stages = shaderStages,
                    .m_vertexInput =
                        {
                            .m_elements = vertexLayoutElements,
                            .m_bindings = vertexBindings,
                        },
                    .m_colorBlending =
                        {
                            .m_attachments =
                                {ColorAttachmentBlendDesc{}, ColorAttachmentBlendDesc{}, ColorAttachmentBlendDesc{}},
                        },
                    // Reverse depth (near = 1, far = 0), matching the GBuffer pass's clear value of
                    // 0 and OrbitCamera's reversed-depth projection matrix.
                    .m_depthStencil =
                        {
                            .m_depthCompare = DepthStencilStateDesc::CompareOp::Greater,
                        },
                    .m_renderTargets =
                        {
                            .m_numColorAttachments = 3,
                            .m_colorFormats = {kGBuffer0Format, kGBuffer1Format, kGBuffer2Format},
                            .m_depthStencilFormat = kGBufferDepthFormat,
                        },
                    .m_pipelineLayout = defaultPipelineLayout,
#if !defined(KE_FINAL)
                    .m_debugName = "Default GBuffer PSO",
#endif
                });

                defaultPipelineShadow = _graphicsContext.CreateGraphicsPipeline({
                    .m_stages = {shaderStages, 1},
                    .m_vertexInput =
                        {
                            .m_elements = vertexLayoutElements,
                            .m_bindings = vertexBindings,
                        },
                    .m_renderTargets =
                        {
                            .m_numColorAttachments = 0,
                            .m_depthStencilFormat = kShadowFormat,
                        },
                    .m_pipelineLayout = defaultPipelineLayout,
#if !defined(KE_FINAL)
                    .m_debugName = "Default Shadow PSO",
#endif
                });
            }
            m_materialManager.SetGraphicsPipeline(
                m_defaultMaterial, static_cast<u8>(PassTypes::GBufferPass), defaultPipelineGBuffer);
            m_materialManager.SetGraphicsPipeline(
                m_defaultMaterial, static_cast<u8>(PassTypes::ShadowPass), defaultPipelineShadow);

            _graphicsContext.FreeShaderModule(fragmentShader);
            _graphicsContext.FreeShaderModule(vertexShader);
            m_allocator.deallocate(fragmentBytecode.data(), fragmentBytecode.size_bytes());
            m_allocator.deallocate(vertexBytecode.data(), vertexBytecode.size_bytes());
        }

        // Runs before the render/game loop starts, so calling SwapScene directly (rather than
        // going through RequestLoadScene) is safe here.
        SwapScene(m_allocator.New<PrecariouslyStackingBoxesTemplate>());

        // Fullscreen passes
        {
            {
                constexpr DescriptorBindingDesc bindings[] = {{
                    .m_type = DescriptorBindingDesc::Type::ConstantBuffer,
                    .m_visibility = ShaderVisibility::Fragment,
                }};
                m_fullscreenPassesLayout = _graphicsContext.CreateDescriptorSetLayout(
                    {
                        .m_bindings = bindings,
                    },
                    &m_fullscreenPassesCbIdx);
            }

            m_fullscreenDescriptorSet = _graphicsContext.CreateDescriptorSet(m_fullscreenPassesLayout);

            m_fullscreenConstantsBuffer.Init(
                &_graphicsContext,
                {
                    .m_desc =
                        {
                            .m_size = sizeof(FullscreenPassConstants),
#if !defined(KE_FINAL)
                            .m_debugName = "FullscreenConstants",
#endif
                        },
                    .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferDstBuffer
                               | MemoryUsage::ConstantBuffer,
                },
                _graphicsContext.GetFrameContextCount());

            m_fullscreenConstantsBufferViews =
                m_allocator.Allocate<BufferViewHandle>(_graphicsContext.GetFrameContextCount());
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

            m_skyAmbientPass.Initialize(&_graphicsContext);
            m_skyAmbientPass.CreatePso(&_graphicsContext);

            m_cascadedShadowMap.Initialize(&_graphicsContext, kCascadeCount, kCascadeResolution);

            m_deferredShadowPass.Initialize(
                &_graphicsContext,
                _gBufferDepthView,
                _gBuffer1View,
                m_cascadedShadowMap.GetShadowArrayView(),
                _deferredShadowsView);
            m_deferredShadowPass.CreatePso(&_graphicsContext);

            m_deferredShadingPass.Initialize(
                &_graphicsContext,
                m_fullscreenPassesLayout,
                _gBuffer0View,
                _gBuffer1View,
                _gBufferDepthView,
                _deferredShadowsView,
                _gBuffer2View,
                m_skyAmbientPass.GetSkyAmbientBufferView());
            m_deferredShadingPass.CreatePso(
                &_graphicsContext,
                {
                    .m_numColorAttachments = 1,
                    .m_colorFormats = {kHdrFormat},
                });

            m_skyPass.Initialize(&_graphicsContext, m_fullscreenPassesLayout);
            m_skyPass.CreatePso(
                &_graphicsContext,
                {
                    .m_numColorAttachments = 1,
                    .m_colorFormats = {kHdrFormat},
                    .m_depthStencilFormat = kGBufferDepthFormat,
                });

            m_colorMappingPass.Initialize(&_graphicsContext, m_fullscreenPassesLayout, _hdrView);
            m_colorMappingPass.CreatePso(
                &_graphicsContext,
                {
                    .m_numColorAttachments = 1,
                    .m_colorFormats = {_swapChainFormat},
                });
        }
    }

    void SceneManager::UpdateFullscreenConstantsBuffer(
        GraphicsContext* _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const uint2 _screenResolution)
    {
        auto* constants = static_cast<FullscreenPassConstants*>(
            m_fullscreenConstantsBuffer.Map(_graphicsContext, _graphicsContext->GetCurrentFrameContextIndex()));

        constants->m_cameraQuaternion = float4(m_orbitCamera->GetViewRotation());

        constants->m_cameraTranslation = m_orbitCamera->GetViewTranslation();
        constants->m_tanHalfFov = std::tan(m_orbitCamera->GetFov() * 0.5f);

        constants->m_screenResolution = float2(_screenResolution);
        constants->m_depthLinearizationConstants = m_orbitCamera->GetDepthLinearizeConstants();

        constants->m_sunLightDirection = m_sunLight->GetDirection();

        constants->m_sunDiffuse = m_sunLight->GetDiffuse();

        m_fullscreenConstantsBuffer.Unmap(_graphicsContext);

        m_fullscreenConstantsBuffer.PrepareBuffers(
            _graphicsContext,
            _transferEncoder,
            BarrierAccessFlags::ConstantBuffer,
            _graphicsContext->GetCurrentFrameContextIndex());

        m_cascadedShadowMap.UpdateCascades(
            _graphicsContext,
            _transferEncoder,
            m_orbitCamera->GetViewRotation(),
            m_orbitCamera->GetViewTranslation(),
            std::tan(m_orbitCamera->GetFov() * 0.5f),
            static_cast<float>(_screenResolution.x) / static_cast<float>(_screenResolution.y),
            m_orbitCamera->GetNear(),
            kMaxShadowDistance,
            m_sunLight->GetDirection());
    }

    void SceneManager::PrepareGBufferPass(
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const uint2 _screenResolution)
    {
        // No-op after the first call: geometry buffers only need uploading once, but this must
        // happen from within a transfer encoder the render graph already opened for this frame,
        // rather than GeometryLibrary opening a command buffer of its own.
        m_geometryLibrary.UploadPendingGeometry(_graphicsContext, _transferEncoder);

        m_drawInstanceManager.UpdateGpuData(_graphicsContext, _transferEncoder);
        m_gBufferPassDispatcher->PrepareDispatch(
            m_orbitCamera->GetViewMatrix(),
            m_orbitCamera->GetProjectionMatrix(),
            _screenResolution,
            _graphicsContext,
            _transferEncoder);
    }

    void SceneManager::RenderGBufferPass(
        GraphicsContext& _graphicsContext,
        const RenderCommandEncoderHandle _renderEncoder) const
    {
        m_gBufferPassDispatcher->Dispatch(_graphicsContext, _renderEncoder);
    }

    void SceneManager::PrepareShadowCascade(
        const u32 _cascadeIndex,
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const uint2 _screenResolution) const
    {
        m_shadowPassDispatchers[_cascadeIndex]->PrepareDispatch(
            m_cascadedShadowMap.GetCascadeViewMatrix(_cascadeIndex),
            m_cascadedShadowMap.GetCascadeProjectionMatrix(_cascadeIndex),
            _screenResolution,
            _graphicsContext,
            _transferEncoder);
    }

    void SceneManager::RenderShadowCascade(
        const u32 _cascadeIndex,
        GraphicsContext& _graphicsContext,
        const RenderCommandEncoderHandle _renderEncoder) const
    {
        m_shadowPassDispatchers[_cascadeIndex]->Dispatch(_graphicsContext, _renderEncoder);
    }

    void SceneManager::RequestLoadScene(SceneTemplate* _template)
    {
        // If a previous request hasn't been picked up by the game loop yet, its instance would
        // otherwise leak (it was never installed as m_currentSceneTemplate, so nothing else owns
        // it); safe to destroy from any thread, since it never became the active template.
        SceneTemplate* previous = m_templateToLoad.exchange(_template, std::memory_order_acq_rel);
        if (previous != nullptr)
        {
            m_allocator.Delete(previous);
        }
    }

    void SceneManager::SwapScene(SceneTemplate* _newTemplate)
    {
        for (const EntityHandle entity : m_sceneEntities)
        {
            m_worldObjectSystem.DestroyEntity(entity);
        }
        m_sceneEntities.clear();

        if (m_currentSceneTemplate != nullptr)
        {
            m_allocator.Delete(m_currentSceneTemplate);
        }

        SceneBuildContext context(m_geometryLibrary, m_worldObjectSystem, m_geometryModels, m_sceneEntities);
        _newTemplate->Build(context);

        m_currentSceneTemplate = _newTemplate;
    }

    void SceneManager::DrawMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::BeginMenu("Load scene"))
            {
                if (ImGui::MenuItem(PrecariouslyStackingBoxesTemplate::kName))
                    RequestLoadScene(m_allocator.New<PrecariouslyStackingBoxesTemplate>());
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Rendering"))
        {
            ImGui::MenuItem("Sunlight options", nullptr, &m_showSunLightWindow);
            ImGui::MenuItem("Shadows", nullptr, &m_showDeferredShadowsWindow);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
} // namespace KryneEngine::Samples::PhysicsDemo