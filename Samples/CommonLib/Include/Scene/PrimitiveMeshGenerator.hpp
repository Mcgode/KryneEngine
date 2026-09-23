/**
 * @file
 * @author Max Godefroy
 * @date 23/09/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/BoundingBox.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine::Samples::PrimitiveMeshGenerator
{
    struct PrimitiveMesh
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

    // All shapes below are centered on the origin and built along the engine's up axis (Z, see
    // CoordinateSystem.hpp), matching the local frame box3d's own shapes (b3Sphere, b3Capsule) are
    // defined in, so a generated mesh and its matching Box3D collision shape line up without any
    // extra local transform.

    // UV sphere, poles on the Z axis.
    PrimitiveMesh GenerateSphereMesh(float _radius, u32 _rings, u32 _segments, AllocatorInstance _allocator);

    // Two hemispherical caps (poles on Z) joined by a cylindrical band. _halfHeight is the
    // distance from the capsule's center to each hemisphere's center, i.e. excluding the radius
    // (matches b3Capsule's center1/center2, which sit halfHeight above/below the origin).
    PrimitiveMesh GenerateCapsuleMesh(float _radius, float _halfHeight, u32 _rings, u32 _segments, AllocatorInstance _allocator);

    // Circular cylinder extruded along Z, from -_halfHeight to +_halfHeight.
    PrimitiveMesh GenerateCylinderMesh(float _radius, float _halfHeight, u32 _segments, AllocatorInstance _allocator);

    // Circular cone: base of radius _radius at -_halfHeight, apex at +_halfHeight.
    PrimitiveMesh GenerateConeMesh(float _radius, float _halfHeight, u32 _segments, AllocatorInstance _allocator);
}
