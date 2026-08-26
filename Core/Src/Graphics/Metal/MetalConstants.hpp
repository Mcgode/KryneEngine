/**
 * @file
 * @author Max Godefroy
 * @date 24/08/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include <Metal/Metal.hpp>

namespace KryneEngine::MetalConstants
{
    static constexpr u32 kMaxBuffersPerStage = 31; // From Metal features table

    static constexpr u32 kMaxArgumentBuffers = 8; // Use same limitation as spirv-cross and MoltenVK
    static constexpr u32 kMaxPushConstantBuffers = 4;
    static constexpr u32 kVertexStreamBuffersOffset = kMaxArgumentBuffers + kMaxPushConstantBuffers;
    static constexpr u32 kMaxVertexBuffers = 16;

    static constexpr u32 kDefaultArgumentTableSize = kMaxArgumentBuffers + kMaxPushConstantBuffers;
    static constexpr u32 kVertexArgumentTableSize = kMaxArgumentBuffers + kMaxPushConstantBuffers + kMaxVertexBuffers;

    static_assert(kDefaultArgumentTableSize <= kMaxBuffersPerStage);
    static_assert(kVertexArgumentTableSize <= kMaxBuffersPerStage);

    static constexpr MTL::RenderStages kAllRenderStages =
        MTL::RenderStageVertex
        | MTL::RenderStageMesh
        | MTL::RenderStageObject
        | MTL::RenderStageFragment
        | MTL::RenderStageTile
    ;
}