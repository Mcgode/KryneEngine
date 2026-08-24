/**
 * @file
 * @author Max Godefroy
 * @date 24/08/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine::MetalConstants
{
    static constexpr u32 kMaxBuffersPerStage = 31; // From Metal features table

    static constexpr u32 kMaxArgumentBuffers = 8; // Use same limitation as spirv-cross and MoltenVK
    static constexpr u32 kMaxPushConstantBuffers = 4;
    static constexpr u32 kVertexStreamBuffersOffset = kMaxArgumentBuffers + kMaxPushConstantBuffers;
    static constexpr u32 kMaxVertexBuffers = 16;

    static_assert(kMaxArgumentBuffers + kMaxPushConstantBuffers + kMaxVertexBuffers <= kMaxBuffersPerStage);
}