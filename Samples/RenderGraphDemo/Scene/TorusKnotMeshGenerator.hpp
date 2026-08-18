/**
 * @file
 * @author Max Godefroy
 * @date 28/02/2025.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/BoundingBox.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine::Samples::RenderGraphDemo::TorusKnotMeshGenerator
{
    struct TorusKnotMesh
    {
        std::byte* m_vertices;
        std::byte* m_indices;
        u32 m_vertexCount;
        u32 m_indexCount;
        Math::BoundingBox m_boundingBox;
    };

    using VertexPositionType = float3;
    static constexpr size_t kVertexPositionSize = sizeof(VertexPositionType);
    static constexpr size_t kVertexPositionOffset = 0;

    using VertexNormalType = float3;
    static constexpr size_t kVertexNormalSize = sizeof(VertexNormalType);
    static constexpr size_t kVertexNormalOffset = kVertexPositionOffset + kVertexPositionSize;

    static constexpr size_t kVertexSize = sizeof(VertexPositionType) + sizeof(VertexNormalType);

    TorusKnotMesh GenerateMesh(
        u32 _tubularSegments,
        u32 _radialSegments,
        float _knotRadius,
        float _tubeRadius,
        u32 _p,
        u32 _q,
        AllocatorInstance _allocator);
}
