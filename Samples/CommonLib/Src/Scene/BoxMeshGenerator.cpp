/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#include "Scene/BoxMeshGenerator.hpp"

#include <KryneEngine/Core/Math/CoordinateSystem.hpp>
#include <KryneEngine/Core/Math/Quaternion.hpp>

namespace KryneEngine::Samples::BoxMeshGenerator
{
    BoxMesh GenerateBoxMesh(const float3 _size, const AllocatorInstance _allocator)
    {
        BoxMesh boxMesh {
            .m_vertexCount = 6 * 4,
            .m_indexCount = 6 * 6,
        };

        boxMesh.m_vertices = _allocator.Allocate<std::byte>(boxMesh.m_vertexCount * kVertexSize);
        boxMesh.m_indices = _allocator.Allocate<std::byte>(boxMesh.m_indexCount * sizeof(uint32_t));

        const float3 baseMeshPositions[4 * 6] = {
            { -0.5, -0.5, -0.5 },
            {  0.5, -0.5, -0.5 },
            { -0.5,  0.5, -0.5 },
            {  0.5,  0.5, -0.5 },
            { -0.5, -0.5,  0.5 },
            {  0.5, -0.5,  0.5 },
            { -0.5,  0.5,  0.5 },
            {  0.5,  0.5,  0.5 },
        };
        const float3 baseNormals[6] = {
            float3( 1,  0,  0),
            float3( 0,  1,  0),
            float3( 0,  0,  1),
            float3(-1,  0,  0),
            float3( 0, -1,  0),
            float3( 0,  0, -1),
        };
        constexpr u16 baseIndices[4 * 6] = {
            7, 5, 1, 3, // X+ Face
            6, 7, 3, 3, // Y+ Face
            6, 4, 5, 7, // Z+ Face
            4, 6, 2, 0, // X- Face
            5, 4, 0, 1, // Y- Face
            3, 1, 0, 2, // Z- Face
        };

        auto* vertexPtr = reinterpret_cast<float3*>(boxMesh.m_vertices);
        auto* indexPtr = reinterpret_cast<u32*>(boxMesh.m_indices);
        for (u32 i = 0; i < 6; ++i)
        {
            for (u32 j = 0; j < 4; ++j)
            {
                const u16 index = baseIndices[i * 4 + j];
                vertexPtr[0] = baseMeshPositions[index] * _size;
                vertexPtr[1] = baseNormals[i];
                vertexPtr += 2;
            }

            indexPtr[0] = i * 4 + 0;
            indexPtr[1] = i * 4 + 1;
            indexPtr[2] = i * 4 + 2;
            indexPtr[3] = i * 4 + 0;
            indexPtr[4] = i * 4 + 2;
            indexPtr[5] = i * 4 + 3;
            indexPtr += 6;
        }

        return boxMesh;
    }
} // namespace KryneEngine::Samples::BoxMeshGenerator