/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#pragma once

#include "SceneTemplate.hpp"

namespace KryneEngine::Samples::PhysicsDemo
{
    // A handful of static boxes the user can grab and fling around with the mouse: left-clicking
    // a box picks it at the clicked point (the "drag point"), then for as long as the button stays
    // held, a spring joint pulls that point towards wherever the cursor currently points -
    // dragging and swinging the box the way you'd nudge a physical object with a stick.
    //
    // The drag is implemented as a soft, near-zero-length b3DistanceJoint between the grabbed
    // body and a small kinematic "cursor anchor" body that gets teleported/driven to the cursor's
    // position every step; letting the constraint solver integrate the spring is what keeps this
    // stable at high stiffness, unlike manually calling b3Body_ApplyForce every step (which is
    // plain explicit-Euler integration and can blow up for the same stiffness values).
    class DraggableBoxesTemplate final : public SceneTemplate
    {
    public:
        static constexpr char kName[] = "Draggable boxes";

        [[nodiscard]] const char* GetName() const override { return kName; }

        ~DraggableBoxesTemplate() override;

        void Build(SceneBuildContext& _context) override;
        void Process(SceneBuildContext& _context, float _deltaTime) override;

    private:
        static constexpr size_t kPyramidSize = 10;
        static constexpr float kPyramidBoxGap = 0.1f;
        static constexpr float kMaxPickDistance = 100.f;

        b3BodyId m_cursorAnchorBody {};

        b3JointId m_dragJoint {};
        EntityHandle m_draggedEntity {};
        float3 m_dragPlanePoint {};  // world-space point the drag plane passes through
        float3 m_dragPlaneNormal {}; // world-space, constant for the duration of a drag

        void BeginDrag(SceneBuildContext& _context);
        void UpdateDrag(const SceneBuildContext& _context, float _deltaTime);
        void EndDrag();

        [[nodiscard]] static float2 GetCursorNdc();
    };
}
