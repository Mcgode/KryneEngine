/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once

#include "Ecs/WorldObjectSystem.hpp"
#include "Geometry/GeometryLibrary.hpp"
#include "Rendering/Compute/DeferredShadowPass.hpp"
#include "Rendering/Compute/SkyAmbientPass.hpp"
#include "Rendering/DrawInstanceManager.hpp"
#include "Rendering/Fullscreen/ColorMappingPass.hpp"
#include "Rendering/Fullscreen/DeferredShadingPass.hpp"
#include "Rendering/Fullscreen/SkyPass.hpp"
#include "Rendering/MaterialManager.hpp"
#include "Rendering/Shadows/CascadedShadowMap.hpp"
#include "Scene/SceneTemplate.hpp"

#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/Containers/SpscQueue.hpp>
#include <atomic>


namespace KryneEngine
{
    class FibersManager;
    class Window;

    namespace Modules::RenderGraph
    {
        class RenderGraph;
    }

    namespace Samples
    {
        class OrbitCamera;
        class SunLight;
    }
}

namespace KryneEngine::Samples::PhysicsDemo
{
    class SceneManager
    {
    public:
        SceneManager(
            AllocatorInstance _allocator,
            GraphicsContext* _graphicsContext,
            SwapChainHandle _mainSwapChainHandle,
            FibersManager* _fibersManager,
            b3WorldId _world,
            bool _singleThreadedMode);

        ~SceneManager();

        void Process(GraphicsContext* _graphicsContext, float _deltaTime);

        void GameLoop();

        void InitPso(
            GraphicsContext& _graphicsContext,
            TextureFormat _swapChainFormat,
            TextureViewHandle _gBuffer0View,
            TextureViewHandle _gBuffer1View,
            TextureViewHandle _gBuffer2View,
            TextureViewHandle _gBufferDepthView,
            TextureViewHandle _deferredShadowsView,
            TextureViewHandle _hdrView);

        void UpdateFullscreenConstantsBuffer(
            GraphicsContext* _graphicsContext,
            TransferCommandEncoderHandle _transferEncoder,
            uint2 _screenResolution);

        [[nodiscard]] DeferredShadingPass& GetDeferredShadingPass() { return m_deferredShadingPass; }
        [[nodiscard]] SkyPass& GetSkyPass() { return m_skyPass; }
        [[nodiscard]] SkyAmbientPass& GetSkyAmbientPass() { return m_skyAmbientPass; }
        [[nodiscard]] ColorMappingPass& GetColorPass() { return m_colorMappingPass; }
        [[nodiscard]] DeferredShadowPass& GetDeferredShadowPass() { return m_deferredShadowPass; }

        void PrepareGBufferPass(GraphicsContext& _graphicsContext, TransferCommandEncoderHandle _transferEncoder, uint2 _screenResolution);
        void RenderGBufferPass(GraphicsContext& _graphicsContext, RenderCommandEncoderHandle _renderEncoder) const;

        [[nodiscard]] u32 GetCascadeCount() const { return m_cascadedShadowMap.GetCascadeCount(); }
        void PrepareShadowCascade(
            u32 _cascadeIndex,
            GraphicsContext& _graphicsContext,
            TransferCommandEncoderHandle _transferEncoder,
            uint2 _screenResolution) const;
        void RenderShadowCascade(u32 _cascadeIndex, GraphicsContext& _graphicsContext, RenderCommandEncoderHandle _renderEncoder) const;
        [[nodiscard]] CascadedShadowMap& GetCascadedShadowMap() { return m_cascadedShadowMap; }

        [[nodiscard]] SpinLock& GetInputLock() { return m_inputLock; }

        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

        void RequestLoadScene(SceneTemplate* _template);

    private:
        void SwapScene(SceneTemplate* _newTemplate);

        void DrawMenuBar();

        AllocatorInstance m_allocator;
        FibersManager* m_fibersManager;
        b3WorldId m_world;

        const bool m_singleThreadedMode;

        DrawInstanceManager m_drawInstanceManager;
        MaterialManager m_materialManager;
        GeometryLibrary m_geometryLibrary;
        WorldObjectSystem m_worldObjectSystem;

        OrbitCamera* m_orbitCamera = nullptr;
        SunLight* m_sunLight = nullptr;

        PassDispatcher* m_gBufferPassDispatcher = nullptr;
        PassDispatcher* m_shadowPassDispatchers[CascadedShadowMap::kMaxCascades] {};

        static constexpr u32 kCascadeCount = 4;
        static constexpr u32 kCascadeResolution = 2048;
        static constexpr float kMaxShadowDistance = 60.f;
        CascadedShadowMap m_cascadedShadowMap;

        u64 m_gameFrameId = 0;
        SpscQueue<u64> m_gameFramesQueue;

        float m_physicsTimeStep = 1.0f / 60.0f;
        s32 m_physicsSubSteps = 4;
        float m_timeProgress = 0.0f;

        MaterialHandle m_defaultMaterial {};

        GeometryModelArray m_geometryModels {};
        eastl::vector<EntityHandle> m_sceneEntities;

        SceneTemplate* m_currentSceneTemplate = nullptr;
        std::atomic<SceneTemplate*> m_templateToLoad { nullptr };

        DescriptorSetLayoutHandle m_fullscreenPassesLayout {};
        u32 m_fullscreenPassesCbIdx = 0;
        DescriptorSetHandle m_fullscreenDescriptorSet {};
        Modules::GraphicsUtils::DynamicBuffer m_fullscreenConstantsBuffer;
        BufferViewHandle* m_fullscreenConstantsBufferViews = nullptr;

        DeferredShadingPass m_deferredShadingPass;
        SkyPass m_skyPass;
        SkyAmbientPass m_skyAmbientPass;
        ColorMappingPass m_colorMappingPass;
        DeferredShadowPass m_deferredShadowPass;

        SpinLock m_inputLock;

        bool m_showSunLightWindow = false;
        bool m_showDeferredShadowsWindow = false;
    };
}