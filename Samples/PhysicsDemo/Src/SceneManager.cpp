/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#include "SceneManager.hpp"

#include <KryneEngine/Core/Profiling/TracyHeader.hpp>

#include "PassTypes.hpp"

namespace KryneEngine::Samples::PhysicsDemo
{
    SceneManager::SceneManager(const AllocatorInstance _allocator, FibersManager* _fibersManager, const b3WorldId _world)
        : m_allocator(_allocator)
        , m_fibersManager(_fibersManager)
        , m_world(_world)
        , m_drawInstanceManager(_allocator)
        , m_materialManager(_allocator, static_cast<u8>(PassTypes::Count))
        , m_gameFramesQueue(_allocator, 3)
    {}

    void SceneManager::Process(const float _deltaTime)
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
}