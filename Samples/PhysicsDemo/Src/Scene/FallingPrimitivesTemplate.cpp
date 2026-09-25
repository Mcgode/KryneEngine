/**
 * @file
 * @author Max Godefroy
 * @date 23/09/2026.
 */

#include "FallingPrimitivesTemplate.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <cmath>

namespace KryneEngine::Samples::PhysicsDemo
{
    namespace
    {
        // The geometries this template cycles through when spawning; Ground is deliberately left
        // out, since it's only ever used for the static plane below.
        constexpr GeometryType kFallableTypes[] = {
            GeometryType::Box,
            GeometryType::Sphere,
            GeometryType::Capsule,
            GeometryType::Cylinder,
            GeometryType::Cone,
        };

        constexpr float kTwoPi = 6.283185307f;
    }

    void FallingPrimitivesTemplate::Build(SceneBuildContext& _context)
    {
        const SimplePoolHandle groundModel = _context.GetModel(GeometryType::Ground);

        b3BodyDef bodyDef = b3DefaultBodyDef();
        bodyDef.type = b3_staticBody;

        const Transform transform { .m_position = Math::UpVector() * -2.5f };
        const EntityHandle groundEntity = _context.CreateEntity(transform, bodyDef, groundModel);

        const b3BodyId groundBody = _context.GetBody(groundEntity);
        b3ShapeDef shapeDef = b3DefaultShapeDef();
        b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(GeometryType::Ground);
        b3CreateHullShape(groundBody, &shapeDef, &hull.base);

        m_ringBuffer.fill(EntityHandle {});
        m_ringCursor = 0;
        m_liveCount = 0;
        m_nextGeometryIndex = 0;
        m_spawnTimer = 0.f;
    }

    void FallingPrimitivesTemplate::Process(SceneBuildContext& _context, const float _deltaTime)
    {
        m_spawnTimer += _deltaTime;
        while (m_spawnTimer >= kSpawnInterval)
        {
            m_spawnTimer -= kSpawnInterval;
            SpawnPrimitive(_context);
        }
    }

    void FallingPrimitivesTemplate::SpawnPrimitive(SceneBuildContext& _context)
    {
        // The ring buffer is full: the entity at the write cursor is always the oldest one still
        // alive (every slot was filled in cursor order, and the cursor only ever moves forward),
        // so it's the one to evict to make room for the new spawn.
        if (m_liveCount == kRingBufferCapacity)
        {
            _context.DestroyEntity(m_ringBuffer[m_ringCursor]);
        }

        const GeometryType geometryType = kFallableTypes[m_nextGeometryIndex % std::size(kFallableTypes)];
        ++m_nextGeometryIndex;

        // Spawn point picked uniformly at random within a fixed XY square, at a fixed Z, so
        // successive drops rain down over different parts of the ground plane instead of
        // stacking in a single column.
        const float x = RandomInRange(-kSpawnAreaHalfExtent, kSpawnAreaHalfExtent);
        const float y = RandomInRange(-kSpawnAreaHalfExtent, kSpawnAreaHalfExtent);

        const Transform transform {
            .m_position = float3(x, y, kSpawnHeight),
            .m_rotation = RandomRotation(),
            .m_scale = float3(1.f, 1.f, 1.f),
        };

        b3BodyDef bodyDef = b3DefaultBodyDef();
        bodyDef.type = b3_dynamicBody;

        const SimplePoolHandle model = _context.GetModel(geometryType);
        const EntityHandle entity = _context.CreateEntity(transform, bodyDef, model);

        const b3BodyId body = _context.GetBody(entity);
        b3ShapeDef shapeDef = b3DefaultShapeDef();
        switch (geometryType)
        {
        case GeometryType::Sphere:
        {
            const b3Sphere sphere = _context.GetGeometryLibrary().GetSphere(geometryType);
            b3CreateSphereShape(body, &shapeDef, &sphere);
            break;
        }
        case GeometryType::Capsule:
        {
            const b3Capsule capsule = _context.GetGeometryLibrary().GetCapsule(geometryType);
            b3CreateCapsuleShape(body, &shapeDef, &capsule);
            break;
        }
        case GeometryType::Cylinder:
        case GeometryType::Cone:
        {
            const b3HullData* hull = _context.GetGeometryLibrary().GetHull(geometryType);
            b3CreateHullShape(body, &shapeDef, hull);
            break;
        }
        case GeometryType::Box:
        default:
        {
            b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(geometryType);
            b3CreateHullShape(body, &shapeDef, &hull.base);
            break;
        }
        }
        b3Body_ApplyMassFromShapes(body);

        m_ringBuffer[m_ringCursor] = entity;
        m_ringCursor = (m_ringCursor + 1) % kRingBufferCapacity;
        m_liveCount = eastl::min(m_liveCount + 1, kRingBufferCapacity);
    }

    Math::Quaternion FallingPrimitivesTemplate::RandomRotation()
    {
        // Axis sampled uniformly over the sphere (standard spherical-coordinates mapping from two
        // uniform variates), combined with a uniformly random angle: not a uniform sample over
        // SO(3), but enough variety for tumbling primitives to look distinct from one another.
        const float u1 = m_unitDistribution(m_rng);
        const float u2 = m_unitDistribution(m_rng);
        const float theta = std::acos(2.f * u1 - 1.f);
        const float phi = kTwoPi * u2;
        const float3 axis(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));

        const float angle = m_unitDistribution(m_rng) * kTwoPi;

        Math::Quaternion rotation;
        rotation.FromAxisAngle(axis, angle);
        return rotation;
    }

    float FallingPrimitivesTemplate::RandomInRange(const float _min, const float _max)
    {
        return _min + m_unitDistribution(m_rng) * (_max - _min);
    }
}
