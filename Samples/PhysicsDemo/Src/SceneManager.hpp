/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once

#include "Rendering/DrawInstanceManager.hpp"
#include "Rendering/Fullscreen/ColorMappingPass.hpp"
#include "Rendering/Fullscreen/DeferredShadingPass.hpp"
#include "Rendering/Fullscreen/SkyPass.hpp"
#include "Rendering/MaterialManager.hpp"

#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Memory/Containers/SpscQueue.hpp>
#include <box3d/box3d.h>


namespace KryneEngine
{
    class FibersManager;

    namespace Modules::RenderGraph
    {
        class RenderGraph;
    }
}

namespace KryneEngine::Samples::PhysicsDemo
{
    class SceneManager
    {
    public:
        SceneManager(
            AllocatorInstance _allocator,
            GraphicsContext& _graphicsContext,
            FibersManager* _fibersManager,
            b3WorldId _world);

        void Process(float _deltaTime);

        void GameLoop();

        void InitPso(
            GraphicsContext& _graphicsContext,
            TextureViewHandle _gBuffer0View,
            TextureViewHandle _gBuffer1View,
            TextureViewHandle _gBuffer2View,
            TextureViewHandle _gBufferDepthView,
            TextureViewHandle _deferredShadowsView,
            TextureViewHandle _hdrView);

    private:
        AllocatorInstance m_allocator;
        FibersManager* m_fibersManager;
        b3WorldId m_world;

        DrawInstanceManager m_drawInstanceManager;
        MaterialManager m_materialManager;

        PassDispatcher* m_gBufferPassDispatcher = nullptr;

        u64 m_gameFrameId = 0;
        SpscQueue<u64> m_gameFramesQueue;

        float m_physicsTimeStep = 1.0f / 60.0f;
        s32 m_physicsSubSteps = 4;
        float m_timeProgress = 0.0f;

        MaterialHandle m_defaultMaterial {};

        DescriptorSetLayoutHandle m_fullscreenPassesLayout {};
        u32 m_fullscreenPassesCbIdx = 0;
        DeferredShadingPass m_deferredShadingPass;
        SkyPass m_skyPass;
        ColorMappingPass m_colorMappingPass;

    };
}