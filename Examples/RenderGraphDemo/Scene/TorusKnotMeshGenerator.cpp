/**
 * @file
 * @author Max Godefroy
 * @date 28/02/2025.
 */

#include "TorusKnotMeshGenerator.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <KryneEngine/Core/Math/Vector3.hpp>

namespace
{
    using namespace KryneEngine;

    float3 computePositionOnCurve(float _u, float _p, float _q, float _knotRadius)
    {
        const float cu = std::cos(_u);
        const float su = std::sin(_u);
        const float quOverP = _q * _u / _p;
        const float cs = std::cos(quOverP);

        return float3 {
            _knotRadius * (2 + cs) * cu * 0.5f,
            _knotRadius * (2 + cs) * su * 0.5f,
            _knotRadius * std::sin(quOverP) * 0.5f
        };
    }
}

namespace KryneEngine::Samples::RenderGraphDemo::TorusKnotMeshGenerator
{
    TorusKnotMesh GenerateMesh(
        u32 _tubularSegments,
        u32 _radialSegments,
        float _knotRadius,
        float _tubeRadius,
        u32 _p,
        u32 _q,
        AllocatorInstance _allocator)
    {
        TorusKnotMesh mesh {};

        mesh.m_indexCount = 3 * 2 * _tubularSegments * _radialSegments;
        mesh.m_vertexCount = (_tubularSegments + 1) * (_radialSegments + 1);

        mesh.m_indices = static_cast<std::byte*>(_allocator.allocate(mesh.m_indexCount * sizeof(u32)));
        mesh.m_vertices = static_cast<std::byte*>(_allocator.allocate(mesh.m_vertexCount * kVertexSize));

        mesh.m_boundingBox = Math::BoundingBox();

        const auto p = static_cast<float>(_p);
        const auto q = static_cast<float>(_q);

        u32 vertexId = 0;
        for (auto i = 0u; i <= _tubularSegments; i++)
        {
            const float u = static_cast<float>(i) / static_cast<float>(_tubularSegments) * p * float(M_PI) * 2.f;

            const float3 p1 = computePositionOnCurve(u, p, q, _knotRadius);
            const float3 p2 = computePositionOnCurve(u + 0.01f, p, q, _knotRadius);

            float3 t = p2 - p1;
            float3 n = p1 + p2;
            float3 b = float3::CrossProduct(t, n);
            n = float3::CrossProduct(b, t);

            n.Normalize();
            b.Normalize();

            for (auto j = 0u; j <= _radialSegments; j++)
            {
                const float v = static_cast<float>(j) / static_cast<float>(_radialSegments) * float(M_PI) * 2.f;
                const float3 cx(-_tubeRadius * std::cos(v));
                const float3 cy(_tubeRadius * std::sin(v));

                const float3 position = p1 + (cx * n) + (cy * b);
                mesh.m_boundingBox.Expand(float3(position));
                memcpy(
                    mesh.m_vertices + vertexId * kVertexSize + kVertexPositionOffset,
                    position.GetPtr(),
                    kVertexPositionSize);

                const float3 normal = (position - p1).Normalized();
                memcpy(
                    mesh.m_vertices + vertexId * kVertexSize + kVertexNormalOffset,
                    normal.GetPtr(),
                    kVertexNormalSize);

                vertexId++;
            }
        }

        u32* indexPtr = reinterpret_cast<u32*>(mesh.m_indices);
        for (auto i = 0u; i < _tubularSegments; i++)
        {
            for (auto j = 0u; j < _radialSegments; j++)
            {
                const u32 a = (_radialSegments + 1) * i + j;
                const u32 b = (_radialSegments + 1) * (i + 1) + j;
                const u32 c = (_radialSegments + 1) * (i + 1) + (j + 1);
                const u32 d = (_radialSegments + 1) * i + (j + 1);

                *(indexPtr++) = a;
                *(indexPtr++) = b;
                *(indexPtr++) = d;

                *(indexPtr++) = b;
                *(indexPtr++) = c;
                *(indexPtr++) = d;
            }
        }

        return mesh;
    }
}

