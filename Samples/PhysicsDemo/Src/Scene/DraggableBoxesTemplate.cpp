/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#include "DraggableBoxesTemplate.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Window/Input/InputManager.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <cmath>

namespace KryneEngine::Samples::PhysicsDemo
{
    DraggableBoxesTemplate::~DraggableBoxesTemplate()
    {
        if (b3Joint_IsValid(m_dragJoint))
        {
            b3DestroyJoint(m_dragJoint, false);
        }
        if (b3Body_IsValid(m_cursorAnchorBody))
        {
            b3DestroyBody(m_cursorAnchorBody);
        }
    }

    void DraggableBoxesTemplate::Build(SceneBuildContext& _context)
    {
        // Static ground plane
        {
            const SimplePoolHandle model = _context.GetModel(GeometryType::Ground);

            b3BodyDef bodyDef = b3DefaultBodyDef();
            bodyDef.type = b3_staticBody;

            const Transform transform { .m_position = Math::UpVector() * -2.5f };
            const EntityHandle entity = _context.CreateEntity(transform, bodyDef, model);

            const b3BodyId body = _context.GetBody(entity);
            b3ShapeDef shapeDef = b3DefaultShapeDef();
            b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(GeometryType::Ground);
            b3CreateHullShape(body, &shapeDef, &hull.base);
        }

        for (size_t i = 0; i < kPyramidSize; i++)
        {
            const SimplePoolHandle model = _context.GetModel(GeometryType::Box);

            constexpr float boxSize = 1.f;
            const size_t n = kPyramidSize - i;
            const float width = boxSize * static_cast<float>(n) + kPyramidBoxGap * static_cast<float>(n - 1);
            const float start = (boxSize - width) / 2.f;

            const float z = -2.5f + boxSize * (static_cast<float>(i) + 0.5f);

            for (size_t j = 0; j < n; j++)
            {
                b3BodyDef bodyDef = b3DefaultBodyDef();
                bodyDef.type = b3_dynamicBody;

                const Transform transform {
                    .m_position = Math::UpVector() * z
                        + Math::RightVector() * (start + static_cast<float>(j) * (kPyramidBoxGap + boxSize))
                };
                const EntityHandle entity = _context.CreateEntity(transform, bodyDef, model);

                const b3BodyId body = _context.GetBody(entity);
                b3ShapeDef shapeDef = b3DefaultShapeDef();
                b3BoxHull hull = _context.GetGeometryLibrary().GetBoxHull(GeometryType::Box);
                b3CreateHullShape(body, &shapeDef, &hull.base);
                b3Body_ApplyMassFromShapes(body);

                b3Body_SetUserData(body, static_cast<void*>(entity.m_handle));
            }
        }

        // Kinematic anchor the drag joint pins to: has no shape (nothing should ever collide with
        // it) and no render representation, so it's created directly on the physics world rather
        // than through CreateEntity(); see the destructor for its matching cleanup.
        {
            b3BodyDef anchorBodyDef = b3DefaultBodyDef();
            anchorBodyDef.type = b3_kinematicBody;
            m_cursorAnchorBody = b3CreateBody(_context.GetWorld(), &anchorBodyDef);
        }
    }

    void DraggableBoxesTemplate::Process(SceneBuildContext& _context, const float _deltaTime)
    {
        if (InputManager::Get().WasMouseButtonJustPressed(MouseInputButton::Left))
        {
            BeginDrag(_context);
        }

        const bool isDragging = !(m_draggedEntity == EntityHandle {});
        if (isDragging && InputManager::Get().IsMouseButtonPressed(MouseInputButton::Left))
        {
            UpdateDrag(_context, _deltaTime);
        }
        else if (isDragging)
        {
            EndDrag();
        }
    }

    void DraggableBoxesTemplate::BeginDrag(SceneBuildContext& _context)
    {
        const Math::Ray ray = _context.GetPickingRay(GetCursorNdc());

        const b3RayResult result = _context.CastRayClosest(ray.m_origin, ray.m_direction * kMaxPickDistance);
        if (!result.hit)
        {
            return;
        }

        const b3BodyId body = b3Shape_GetBody(result.shapeId);
        void* userData = b3Body_GetUserData(body);
        if (userData == nullptr)
        {
            return; // hit something that isn't a draggable entity (e.g. the ground plane)
        }

        m_draggedEntity = EntityHandle { GenPool::Handle::FromVoidPtr(userData) };

        // The drag plane faces the camera and passes through the initial hit point; intersecting
        // it with each frame's picking ray is what turns the mouse cursor into a 3D "target"
        // point for the anchor (and so the joint) to be pulled towards.
        m_dragPlaneNormal = ray.m_direction;
        m_dragPlanePoint = float3(result.point);

        // Snap the anchor to the hit point instantly, so the joint starts at rest instead of
        // having to resolve a large initial constraint violation.
        b3Body_SetTransform(m_cursorAnchorBody, result.point, b3Quat_identity);

        b3MotorJointDef jointDef = b3DefaultMotorJointDef();
        jointDef.base.bodyIdA = m_cursorAnchorBody;
        jointDef.base.bodyIdB = body;
        jointDef.base.localFrameB.p = b3Body_GetLocalPoint(body, result.point);
        jointDef.linearHertz = 7.5f;
        jointDef.linearDampingRatio = 1.f;

        b3MassData massData = b3Body_GetMassData(body);
        float g = b3Length(b3World_GetGravity(_context.GetWorld()));
        float mg = massData.mass * g;
        jointDef.maxSpringForce = 100.f * mg;

        if ( massData.mass > 0.0f )
        {
            // This acts like angular friction
            float trace = massData.inertia.cx.x + massData.inertia.cy.y + massData.inertia.cz.z;
            float lever = sqrtf( trace / ( 3.0f * massData.mass ) );
            jointDef.maxVelocityTorque = 0.5f * lever * mg;
        }

        m_dragJoint = b3CreateMotorJoint(_context.GetWorld(), &jointDef);
    }

    void DraggableBoxesTemplate::UpdateDrag(const SceneBuildContext& _context, const float _deltaTime)
    {
        if (!b3Joint_IsValid(m_dragJoint))
        {
            EndDrag();
            return;
        }

        const Math::Ray ray = _context.GetPickingRay(GetCursorNdc());

        const float denominator = float3::Dot(m_dragPlaneNormal, ray.m_direction);
        if (std::abs(denominator) < 1e-5f)
        {
            return; // ray (near-)parallel to the drag plane; leave the anchor where it was
        }
        const float distanceAlongRay = float3::Dot(m_dragPlaneNormal, m_dragPlanePoint - ray.m_origin) / denominator;
        const float3 targetWorldPoint = ray.m_origin + ray.m_direction * distanceAlongRay;

        // Drives the kinematic anchor towards the cursor's world position; the joint (a soft,
        // near-zero-length distance constraint) is what turns that motion into a spring pull on
        // the dragged body, integrated by the solver instead of by us.
        const b3WorldTransform target {
            .p = b3Pos { .x = targetWorldPoint.x, .y = targetWorldPoint.y, .z = targetWorldPoint.z },
            .q = b3Quat_identity,
        };
        b3Body_SetTargetTransform(m_cursorAnchorBody, target, _deltaTime, true);
    }

    void DraggableBoxesTemplate::EndDrag()
    {
        if (b3Joint_IsValid(m_dragJoint))
        {
            b3DestroyJoint(m_dragJoint, true);
            m_dragJoint = b3JointId {};
        }
        m_draggedEntity = EntityHandle {};
    }

    float2 DraggableBoxesTemplate::GetCursorNdc()
    {
        Window* window = InputManager::Get().GetFocusedWindow();
        if (window == nullptr)
        {
            return float2 {};
        }

        const float2 cursorPos = InputManager::Get().GetCursorPosition(window);
        const float2 uv = cursorPos / float2(window->GetSize());
        return { 2.f * uv.x - 1.f, 1.f - 2.f * uv.y };
    }
}
