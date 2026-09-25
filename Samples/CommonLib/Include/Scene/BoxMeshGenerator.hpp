/**
 * @file
 * @author Max Godefroy
 * @date 19/08/2026.
 */

#pragma once

#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Math/BoundingBox.hpp>


namespace KryneEngine::Samples::BoxMeshGenerator
{
    struct BoxMesh
    {
        std::byte* m_vertices = nullptr;
        std::byte* m_indices = nullptr;
        u32 m_vertexCount = 0;
        u32 m_indexCount = 0;
        Math::BoundingBox m_aabb {};
    };

    using VertexPositionType = float3;
    static constexpr size_t kVertexPositionSize = sizeof(VertexPositionType);
    static constexpr size_t kVertexPositionOffset = 0;

    using VertexNormalType = float3;
    static constexpr size_t kVertexNormalSize = sizeof(VertexNormalType);
    static constexpr size_t kVertexNormalOffset = kVertexPositionOffset + kVertexPositionSize;

    static constexpr size_t kVertexSize = sizeof(VertexPositionType) + sizeof(VertexNormalType);

    BoxMesh GenerateBoxMesh(float3 _size, AllocatorInstance _allocator);
}