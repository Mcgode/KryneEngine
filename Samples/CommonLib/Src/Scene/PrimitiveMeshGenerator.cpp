/**
 * @file
 * @author Max Godefroy
 * @date 23/09/2026.
 */

#include "Scene/PrimitiveMeshGenerator.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <cstring>
#include <KryneEngine/Core/Math/Vector3.hpp>

namespace KryneEngine::Samples::PrimitiveMeshGenerator
{
    static void WriteVertex(
        std::byte* _vertices,
        const u32 _index,
        const float3& _position,
        const float3& _normal,
        Math::BoundingBox& _boundingBox)
    {
        _boundingBox.Expand(_position);
        memcpy(_vertices + _index * kVertexSize + kVertexPositionOffset, _position.GetPtr(), kVertexPositionSize);
        memcpy(_vertices + _index * kVertexSize + kVertexNormalOffset, _normal.GetPtr(), kVertexNormalSize);
    }

    // Triangulates the quad spanning ring rows i/i+1 and columns j/j+1 of a (rowCount x
    // (_segments + 1)) vertex grid. Rows are expected to run from the mesh's "top" towards its
    // "bottom" (i.e. decreasing Z, or in towards the axis) and columns to run counterclockwise
    // around the Z axis (increasing azimuth) - see the derivation in GenerateSphereMesh for why
    // this winding faces outward under the engine's CounterClockwise front face convention.
    u32* WriteRingQuad(u32* _indices, const u32 _segmentsPlusOne, const u32 _i, const u32 _j)
    {
        const u32 a = _i * _segmentsPlusOne + _j;
        const u32 b = (_i + 1) * _segmentsPlusOne + _j;
        const u32 c = (_i + 1) * _segmentsPlusOne + _j + 1;
        const u32 d = _i * _segmentsPlusOne + _j + 1;

        _indices[0] = a;
        _indices[1] = b;
        _indices[2] = c;
        _indices[3] = a;
        _indices[4] = c;
        _indices[5] = d;
        return _indices + 6;
    }

    // Fans out from _centerIndex to consecutive vertices in [_ringStart, _ringStart + _segments],
    // facing +Z when _flip is false, -Z when _flip is true (see the cap-winding derivation next to
    // GenerateCylinderMesh's top/bottom caps).
    u32* WriteFan(u32* _indices, const u32 _centerIndex, const u32 _ringStart, const u32 _segments, const bool _flip)
    {
        for (u32 j = 0; j < _segments; ++j)
        {
            _indices[0] = _centerIndex;
            _indices[1] = _flip ? _ringStart + j + 1 : _ringStart + j;
            _indices[2] = _flip ? _ringStart + j : _ringStart + j + 1;
            _indices += 3;
        }
        return _indices;
    }
}

namespace KryneEngine::Samples::PrimitiveMeshGenerator
{
    // A UV sphere: (_rings + 1) rows of (_segments + 1) vertices each (the last column of each row
    // duplicates the first, closing the seam), row i=0 at the +Z pole and row i=_rings at the -Z
    // pole. Adjacent rows/columns are quad-triangulated by WriteRingQuad.
    //
    // Winding derivation (why (a, b, c) / (a, c, d) faces outward, a=(i,j) b=(i+1,j) c=(i+1,j+1)
    // d=(i,j+1)): near the equator, at φ=0, a≈(r,0,0), b is one row further down (higher θ, so
    // lower Z) ≈(r,0,-ε), and d is one column further round (higher φ, CCW seen from +Z) ≈(r,ε,0).
    // (b-a)×(d-a) ≈ (0,0,-ε)×(0,ε,0) = (ε²,0,0), i.e. it points along the position vector itself -
    // outward - which is exactly the CCW-as-seen-from-outside front face the engine's rasterizer
    // state expects (RasterStateDesc::Front::CounterClockwise).
    PrimitiveMesh GenerateSphereMesh(const float _radius, const u32 _rings, const u32 _segments, const AllocatorInstance _allocator)
    {
        PrimitiveMesh mesh {};
        mesh.m_vertexCount = (_rings + 1) * (_segments + 1);
        mesh.m_indexCount = _rings * _segments * 6;

        mesh.m_vertices = _allocator.Allocate<std::byte>(mesh.m_vertexCount * kVertexSize);
        mesh.m_indices = _allocator.Allocate<std::byte>(mesh.m_indexCount * sizeof(u32));

        const u32 columns = _segments + 1;
        for (u32 i = 0; i <= _rings; ++i)
        {
            const float theta = static_cast<float>(i) / static_cast<float>(_rings) * float(M_PI);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (u32 j = 0; j <= _segments; ++j)
            {
                const float phi = static_cast<float>(j) / static_cast<float>(_segments) * 2.f * float(M_PI);
                const float3 direction(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

                WriteVertex(mesh.m_vertices, i * columns + j, direction * _radius, direction, mesh.m_boundingBox);
            }
        }

        u32* indexPtr = reinterpret_cast<u32*>(mesh.m_indices);
        for (u32 i = 0; i < _rings; ++i)
        {
            for (u32 j = 0; j < _segments; ++j)
            {
                indexPtr = WriteRingQuad(indexPtr, columns, i, j);
            }
        }

        return mesh;
    }

    // Reuses the exact same unit-sphere-direction math as GenerateSphereMesh for both the
    // hemispherical caps and the cylindrical band between them: rows [0, _rings] sweep the top
    // hemisphere (θ in [0, π/2], offset by +_halfHeight), rows [_rings + 1, 2 * _rings + 1] sweep
    // the bottom hemisphere (θ in [π/2, π], offset by -_halfHeight). The row pair straddling
    // _rings/_rings+1 shares the same θ=π/2 circle (radius _radius, direction.z = 0) at two
    // different Z offsets, which is exactly a cylinder's side wall - and its direction there is
    // already the correct outward radial normal, so no special-casing is needed for the join.
    PrimitiveMesh GenerateCapsuleMesh(
        const float _radius,
        const float _halfHeight,
        const u32 _rings,
        const u32 _segments,
        const AllocatorInstance _allocator)
    {
        PrimitiveMesh mesh {};
        const u32 rows = 2 * _rings + 2;
        mesh.m_vertexCount = rows * (_segments + 1);
        mesh.m_indexCount = (rows - 1) * _segments * 6;

        mesh.m_vertices = _allocator.Allocate<std::byte>(mesh.m_vertexCount * kVertexSize);
        mesh.m_indices = _allocator.Allocate<std::byte>(mesh.m_indexCount * sizeof(u32));

        const u32 columns = _segments + 1;
        for (u32 i = 0; i < rows; ++i)
        {
            float theta;
            float zOffset;
            if (i <= _rings)
            {
                theta = static_cast<float>(i) / static_cast<float>(_rings) * float(M_PI) * 0.5f;
                zOffset = _halfHeight;
            }
            else
            {
                const u32 i2 = i - _rings - 1;
                theta = float(M_PI) * 0.5f + static_cast<float>(i2) / static_cast<float>(_rings) * float(M_PI) * 0.5f;
                zOffset = -_halfHeight;
            }
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (u32 j = 0; j <= _segments; ++j)
            {
                const float phi = static_cast<float>(j) / static_cast<float>(_segments) * 2.f * float(M_PI);
                const float3 direction(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
                const float3 position(direction.x * _radius, direction.y * _radius, direction.z * _radius + zOffset);

                WriteVertex(mesh.m_vertices, i * columns + j, position, direction, mesh.m_boundingBox);
            }
        }

        u32* indexPtr = reinterpret_cast<u32*>(mesh.m_indices);
        for (u32 i = 0; i < rows - 1; ++i)
        {
            for (u32 j = 0; j < _segments; ++j)
            {
                indexPtr = WriteRingQuad(indexPtr, columns, i, j);
            }
        }

        return mesh;
    }

    // Top cap (center + ring, normal +Z) - side wall (two rings, radial normals, triangulated with
    // the same WriteRingQuad winding as the sphere) - bottom cap (ring + center, normal -Z). The
    // side rings are duplicated from the caps' rings rather than shared with them, since a sharp
    // edge needs distinct normals on either side of it.
    //
    // Cap fan winding: for the top cap, center C=(0,0,h), ring[j] and ring[j+1] (j+1 = one step
    // CCW in φ as seen from +Z). (ring[j]-C)×(ring[j+1]-C) has a +Z component of r²sin(Δφ) > 0, so
    // (C, ring[j], ring[j+1]) already faces +Z outward. The bottom cap is the mirror image, so its
    // fan uses the opposite vertex order to face -Z outward - that's WriteFan's _flip.
    PrimitiveMesh GenerateCylinderMesh(const float _radius, const float _halfHeight, const u32 _segments, const AllocatorInstance _allocator)
    {
        PrimitiveMesh mesh {};
        const u32 columns = _segments + 1;
        mesh.m_vertexCount = 2 + 4 * columns;
        mesh.m_indexCount = _segments * 12;

        mesh.m_vertices = _allocator.Allocate<std::byte>(mesh.m_vertexCount * kVertexSize);
        mesh.m_indices = _allocator.Allocate<std::byte>(mesh.m_indexCount * sizeof(u32));

        const u32 topCenterIdx = 0;
        const u32 topCapRingStart = 1;
        const u32 topSideRingStart = topCapRingStart + columns;
        const u32 bottomSideRingStart = topSideRingStart + columns;
        const u32 bottomCapRingStart = bottomSideRingStart + columns;
        const u32 bottomCenterIdx = bottomCapRingStart + columns;

        WriteVertex(mesh.m_vertices, topCenterIdx, float3(0.f, 0.f, _halfHeight), float3(0.f, 0.f, 1.f), mesh.m_boundingBox);
        WriteVertex(mesh.m_vertices, bottomCenterIdx, float3(0.f, 0.f, -_halfHeight), float3(0.f, 0.f, -1.f), mesh.m_boundingBox);

        for (u32 j = 0; j <= _segments; ++j)
        {
            const float phi = static_cast<float>(j) / static_cast<float>(_segments) * 2.f * float(M_PI);
            const float3 radial(std::cos(phi), std::sin(phi), 0.f);
            const float3 topPos(radial.x * _radius, radial.y * _radius, _halfHeight);
            const float3 bottomPos(radial.x * _radius, radial.y * _radius, -_halfHeight);

            WriteVertex(mesh.m_vertices, topCapRingStart + j, topPos, float3(0.f, 0.f, 1.f), mesh.m_boundingBox);
            WriteVertex(mesh.m_vertices, topSideRingStart + j, topPos, radial, mesh.m_boundingBox);
            WriteVertex(mesh.m_vertices, bottomSideRingStart + j, bottomPos, radial, mesh.m_boundingBox);
            WriteVertex(mesh.m_vertices, bottomCapRingStart + j, bottomPos, float3(0.f, 0.f, -1.f), mesh.m_boundingBox);
        }

        u32* indexPtr = reinterpret_cast<u32*>(mesh.m_indices);
        indexPtr = WriteFan(indexPtr, topCenterIdx, topCapRingStart, _segments, false);
        for (u32 j = 0; j < _segments; ++j)
        {
            const u32 a = topSideRingStart + j;
            const u32 b = bottomSideRingStart + j;
            const u32 c = bottomSideRingStart + j + 1;
            const u32 d = topSideRingStart + j + 1;
            indexPtr[0] = a; indexPtr[1] = b; indexPtr[2] = c;
            indexPtr[3] = a; indexPtr[4] = c; indexPtr[5] = d;
            indexPtr += 6;
        }
        indexPtr = WriteFan(indexPtr, bottomCenterIdx, bottomCapRingStart, _segments, true);

        return mesh;
    }

    // Base cap (ring + center, normal -Z, same fan winding as GenerateCylinderMesh's bottom cap) -
    // side (apex ring + base ring). Unlike the cylinder, the cone's side normal is not radial: for
    // a point at parameter t (t=0 base, t=1 apex) and azimuth φ, the surface tangents are
    // dP/dt = (-r cosφ, -r sinφ, height) and dP/dφ = -(1-t)r (sinφ, -cosφ, 0) (height = 2 *
    // _halfHeight); their cross product dP/dφ × dP/dt simplifies (the (1-t)r factor cancels) to a
    // direction of (height cosφ, height sinφ, r), independent of t - i.e. constant along each
    // straight side line from base to apex, as expected for a cone. That also means the apex
    // vertices (row 0) can reuse the exact same per-column normal formula as the base ring (row 1)
    // and be triangulated with the ordinary WriteRingQuad; the second triangle of each side quad
    // degenerates to zero area there (both its row-0 vertices sit at the same apex position), which
    // is harmless and mirrors how GenerateSphereMesh already handles its poles.
    PrimitiveMesh GenerateConeMesh(const float _radius, const float _halfHeight, const u32 _segments, const AllocatorInstance _allocator)
    {
        PrimitiveMesh mesh {};
        const u32 columns = _segments + 1;
        mesh.m_vertexCount = 1 + 3 * columns;
        mesh.m_indexCount = _segments * 3 + _segments * 6;

        mesh.m_vertices = _allocator.Allocate<std::byte>(mesh.m_vertexCount * kVertexSize);
        mesh.m_indices = _allocator.Allocate<std::byte>(mesh.m_indexCount * sizeof(u32));

        const u32 apexRingStart = 0;
        const u32 baseSideRingStart = columns;
        const u32 baseCapRingStart = 2 * columns;
        const u32 baseCenterIdx = 3 * columns;

        const float height = 2.f * _halfHeight;
        WriteVertex(mesh.m_vertices, baseCenterIdx, float3(0.f, 0.f, -_halfHeight), float3(0.f, 0.f, -1.f), mesh.m_boundingBox);

        for (u32 j = 0; j <= _segments; ++j)
        {
            const float phi = static_cast<float>(j) / static_cast<float>(_segments) * 2.f * float(M_PI);
            const float3 radial(std::cos(phi), std::sin(phi), 0.f);
            const float3 sideNormal = float3(height * radial.x, height * radial.y, _radius).Normalized();
            const float3 basePos(radial.x * _radius, radial.y * _radius, -_halfHeight);

            WriteVertex(mesh.m_vertices, apexRingStart + j, float3(0.f, 0.f, _halfHeight), sideNormal, mesh.m_boundingBox);
            WriteVertex(mesh.m_vertices, baseSideRingStart + j, basePos, sideNormal, mesh.m_boundingBox);
            WriteVertex(mesh.m_vertices, baseCapRingStart + j, basePos, float3(0.f, 0.f, -1.f), mesh.m_boundingBox);
        }

        // apexRingStart (row 0) and baseSideRingStart (row 1) are `columns` apart, so this is a
        // plain contiguous two-row grid as far as WriteRingQuad is concerned.
        u32* indexPtr = reinterpret_cast<u32*>(mesh.m_indices);
        for (u32 j = 0; j < _segments; ++j)
        {
            indexPtr = WriteRingQuad(indexPtr, columns, 0, j);
        }
        indexPtr = WriteFan(indexPtr, baseCenterIdx, baseCapRingStart, _segments, true);

        return mesh;
    }
}
