/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once

#include "KryneEngine/Core/Memory/Containers/SpscQueue.hpp"
#include "Rendering/DrawInstanceManager.hpp"
#include "Rendering/MaterialManager.hpp"
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <box3d/box3d.h>


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

    private:
        AllocatorInstance m_allocator;
        FibersManager* m_fibersManager;
        b3WorldId m_world;

        DrawInstanceManager m_drawInstanceManager;
        MaterialManager m_materialManager;

        PassDispatcher* m_gBufferPasDispatcher = nullptr;

        u64 m_gameFrameId = 0;
        SpscQueue<u64> m_gameFramesQueue;

        float m_physicsTimeStep = 1.0f / 60.0f;
        s32 m_physicsSubSteps = 4;
        float m_timeProgress = 0.0f;
    };
}