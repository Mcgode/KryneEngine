/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2026.
 */

#include "FallingBoxesTemplate.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>

namespace KryneEngine::Samples::PhysicsDemo
{
    void FallingBoxesTemplate::Build(SceneBuildContext& _context)
    {
        // Static ground plane, registered as its own dedicated entity (rather than a bare,
        // unrendered Box3D body) so it can be drawn like any other world object.
        {
            const SimplePoolHandle groundModel = _context.GetModel(GeometryType::Ground);

            b3BodyDef bodyDef = b3DefaultBodyDef();
            bodyDef.type = b3_staticBody;

            // Lowered a couple of cube-heights below the falling boxes' resting height, so they
            // have some room to drop before landing.
            const Transform transform { .m_position = Math::UpVector() * -2.5f };
            const EntityHandle groundEntity = _context.CreateEntity(transform, bodyDef, groundModel);

            const b3BodyId groundBody = _context.GetBody(groundEntity);
            b3ShapeDef shapeDef = b3DefaultShapeDef();
            b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(GeometryType::Ground);
            b3CreateHullShape(groundBody, &shapeDef, &hull.base);
        }

        // A handful of falling box entities.
        {
            const SimplePoolHandle boxModel = _context.GetModel(GeometryType::Box);

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

                const EntityHandle entity = _context.CreateEntity(transform, bodyDef, boxModel);

                const b3BodyId body = _context.GetBody(entity);
                b3ShapeDef shapeDef = b3DefaultShapeDef();
                b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(GeometryType::Box);
                b3CreateHullShape(body, &shapeDef, &hull.base);
                b3Body_ApplyMassFromShapes(body);
            }
        }
    }
}
