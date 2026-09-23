/**
 * @file
 * @author Max Godefroy
 * @date 23/09/2026.
 */

#pragma once

#include "SceneTemplate.hpp"

#include <EASTL/array.h>
#include <random>

namespace KryneEngine::Samples::PhysicsDemo
{
    // A ground plane that keeps getting pelted by primitives dropped in from above, one every
    // few frames, at a random XY position (within a fixed square) and a fixed height. Spawned
    // entities are recycled through a fixed-size ring buffer: once it is full, spawning a new one
    // first destroys whichever entity currently sits at the ring's write cursor - always the
    // longest-lived survivor, since the cursor only ever advances in spawn order - keeping the
    // live entity count bounded no matter how long the demo runs.
    class FallingPrimitivesTemplate final : public SceneTemplate
    {
    public:
        static constexpr char kName[] = "Falling primitives";

        [[nodiscard]] const char* GetName() const override { return kName; }

        void Build(SceneBuildContext& _context) override;
        void Process(SceneBuildContext& _context, float _deltaTime) override;

    private:
        static constexpr u32 kRingBufferCapacity = 24;
        static constexpr float kSpawnInterval = 0.35f;
        static constexpr float kSpawnHeight = 6.f;
        static constexpr float kSpawnAreaHalfExtent = 3.f; // half-width of the XY spawn square

        eastl::array<EntityHandle, kRingBufferCapacity> m_ringBuffer {};
        u32 m_ringCursor = 0;
        u32 m_liveCount = 0;
        u32 m_nextGeometryIndex = 0;
        float m_spawnTimer = 0.f;

        std::mt19937 m_rng { std::random_device{}() };
        std::uniform_real_distribution<float> m_unitDistribution { 0.f, 1.f };

        void SpawnPrimitive(SceneBuildContext& _context);
        Math::Quaternion RandomRotation();
        float RandomInRange(float _min, float _max);
    };
}
