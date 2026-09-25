/**
 * @file
 * @author Max Godefroy
 * @date 22/09/2026.
 */

#pragma once

#include "SceneTemplate.hpp"

namespace KryneEngine::Samples::PhysicsDemo
{
    // A static ground plane with a handful of boxes dropped onto it: minimal proof-of-work
    // content that exercises the ECS end to end, not a real scene-loading format.
    class PrecariouslyStackingBoxesTemplate final : public SceneTemplate
    {
    public:
        static constexpr char kName[] = "Precariously stacking boxes";

        [[nodiscard]] const char* GetName() const override { return kName; }

        void Build(SceneBuildContext& _context) override;
    };
}
