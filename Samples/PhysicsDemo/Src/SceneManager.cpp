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

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <Scene/OrbitCamera.hpp>
#include <Scene/SunLight.hpp>
#include <fstream>

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
            , m_gameFramesQueue(_allocator, 3)
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

        m_defaultMaterial = m_materialManager.RegisterMaterial();

        const uint2 windowSize = _graphicsContext->GetSwapChainSize(_mainSwapChainHandle);
        const float aspectRatio = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
        m_orbitCamera = m_allocator.New<OrbitCamera>(aspectRatio);

        m_sunLight = m_allocator.New<SunLight>();
        m_sunLight->SetTheta(30.f);
        m_sunLight->SetPhi(30.f);
    }

    SceneManager::~SceneManager()
    {
        if (m_fullscreenConstantsBufferViews != nullptr)
            m_allocator.deallocate(m_fullscreenConstantsBufferViews);
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
                   .m_function = [this](u16) { GameLoop(); },
                   .m_priority = FiberJob::Priority::High,
               });
            }
        }

        m_geometryLibrary.Update(*_graphicsContext);

        m_sunLight->Process();

        // Interpolate between the last two fixed steps' worth of data and push the result to the
        // renderer; see WorldObjectSystem's threading contract (OrbitCamera::UpdatePose()/
        // SyncRenderTransform() follow the exact same one) for why this must run on this thread.
        const float alpha = m_timeProgress / m_physicsTimeStep;
        m_orbitCamera->SyncRenderTransform(alpha);
        m_worldObjectSystem.SyncRenderInstances(m_drawInstanceManager, alpha);

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
            m_skyAmbientPass.UpdateSceneConstants(_graphicsContext, m_fullscreenConstantsBufferViews[_graphicsContext->GetCurrentFrameContextIndex()]);
            m_colorMappingPass.UpdateSceneConstants(m_fullscreenDescriptorSet);
        }
    }

    void SceneManager::GameLoop()
    {
        const u64* frameId = m_gameFramesQueue.Front();

        while (!m_gameFramesQueue.Empty())
        {
            KE_ZoneScopedF("Game loop frame %lld", *frameId);

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
                    // Reverse depth (near = 1, far = 0), matching the GBuffer pass's clear value of
                    // 0 and OrbitCamera's reversed-depth projection matrix.
                    .m_depthStencil = {
                        .m_depthCompare = DepthStencilStateDesc::CompareOp::Greater,
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

        // Static ground plane, registered as its own dedicated entity (rather than a bare,
        // unrendered Box3D body) so it can be drawn like any other world object.
        {
            const GeometryBuffers& groundBuffers = m_geometryLibrary.GetBuffers(GeometryType::Ground);
            const SimplePoolHandle groundModel = m_drawInstanceManager.RegisterModel(
                groundBuffers.m_vertexBuffer,
                groundBuffers.m_indexBuffer,
                m_defaultMaterial,
                groundBuffers.m_indexCount);

            b3BodyDef bodyDef = b3DefaultBodyDef();
            bodyDef.type = b3_staticBody;

            // Lowered a couple of cube-heights below the falling boxes' resting height, so they
            // have some room to drop before landing.
            const Transform transform { .m_position = Math::UpVector() * -2.5f };
            const EntityHandle groundEntity = m_worldObjectSystem.CreateEntity(transform, bodyDef, groundModel);

            const b3BodyId groundBody = m_worldObjectSystem.GetBody(groundEntity);
            b3ShapeDef shapeDef = b3DefaultShapeDef();
            b3BoxHull hull = m_geometryLibrary.GetBoxHull(GeometryType::Ground);
            b3CreateHullShape(groundBody, &shapeDef, &hull.base);
        }

        // A handful of falling box entities: minimal proof-of-work content that exercises the
        // ECS end to end, not a real scene-loading format.
        {
            const GeometryBuffers& boxBuffers = m_geometryLibrary.GetBuffers(GeometryType::Box);
            const SimplePoolHandle boxModel = m_drawInstanceManager.RegisterModel(
                boxBuffers.m_vertexBuffer,
                boxBuffers.m_indexBuffer,
                m_defaultMaterial,
                boxBuffers.m_indexCount);

            for (u32 i = 0; i < 5; ++i)
            {
                const Transform transform {
                    .m_position = Math::UpVector() * (2.0f + static_cast<float>(i) * 1.5f) + float3(0.1f * static_cast<float>(i), 0.f, 0.f),
                    // Small per-box yaw for ambient-lighting normal diversity. Kept modest: these
                    // boxes stack almost directly on top of each other as they fall, and a large
                    // relative yaw between neighbours turns their contact into corner-on-face
                    // instead of face-on-face, which is enough for box3d to topple/launch them
                    // clean off the (effectively unbounded) ground plane and out of camera view.
                    .m_rotation = Math::Quaternion().FromAxisAngle(Math::UpVector(), static_cast<float>(i) * 0.15f),
                    .m_scale = float3(1.f, 1.f, 1.f),
                };

                b3BodyDef bodyDef = b3DefaultBodyDef();
                bodyDef.type = b3_dynamicBody;

                const EntityHandle entity = m_worldObjectSystem.CreateEntity(transform, bodyDef, boxModel);

                const b3BodyId body = m_worldObjectSystem.GetBody(entity);
                b3ShapeDef shapeDef = b3DefaultShapeDef();
                b3BoxHull hull = m_geometryLibrary.GetBoxHull(GeometryType::Box);
                b3CreateHullShape(body, &shapeDef, &hull.base);
                b3Body_ApplyMassFromShapes(body);
            }
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

            m_skyAmbientPass.Initialize(&_graphicsContext);
            m_skyAmbientPass.CreatePso(&_graphicsContext);

            m_deferredShadowPass.Initialize(&_graphicsContext, _gBufferDepthView, _deferredShadowsView);
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
                .m_colorFormats = { _swapChainFormat },
            });
        }
    }

    void SceneManager::UpdateFullscreenConstantsBuffer(
        GraphicsContext* _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder,
        const uint2 _screenResolution)
    {
        auto* constants = static_cast<FullscreenPassConstants*>(m_fullscreenConstantsBuffer.Map(
                _graphicsContext,
                _graphicsContext->GetCurrentFrameContextIndex()));

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
    }

    void SceneManager::PrepareGBufferPass(
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        // No-op after the first call: geometry buffers only need uploading once, but this must
        // happen from within a transfer encoder the render graph already opened for this frame,
        // rather than GeometryLibrary opening a command buffer of its own.
        m_geometryLibrary.UploadPendingGeometry(_graphicsContext, _transferEncoder);

        m_drawInstanceManager.UpdateGpuData(_graphicsContext, _transferEncoder);
        m_gBufferPassDispatcher->PrepareDispatch(
            m_orbitCamera->GetViewMatrix(),
            m_orbitCamera->GetProjectionMatrix(),
            _graphicsContext,
            _transferEncoder);
    }

    void SceneManager::RenderGBufferPass(
        GraphicsContext& _graphicsContext,
        const RenderCommandEncoderHandle _renderEncoder)
    {
        m_gBufferPassDispatcher->Dispatch(_graphicsContext, _renderEncoder);
    }
} // namespace KryneEngine::Samples::PhysicsDemo