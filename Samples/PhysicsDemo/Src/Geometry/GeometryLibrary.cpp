/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#include "GeometryLibrary.hpp"

#include <KryneEngine/Core/Graphics/Drawing.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <Scene/BoxMeshGenerator.hpp>
#include <cstring>

namespace KryneEngine::Samples::PhysicsDemo
{
    GeometryLibrary::GeometryLibrary(const AllocatorInstance _allocator, GraphicsContext& _graphicsContext)
        : m_allocator(_allocator)
    {
        // Every hardcoded geometry's data is laid out back to back in one combined buffer, used
        // as both vertex and index buffer depending on which region is bound:
        // [vertex data for every geometry][index data for every geometry]. The single staging
        // buffer below mirrors that exact layout, so the whole upload only ever takes two
        // CopyBuffer calls, regardless of how many geometries exist.
        const BoxMeshGenerator::BoxMesh boxMesh = BoxMeshGenerator::GenerateBoxMesh(float3(1.f, 1.f, 1.f), m_allocator);
        const u64 boxVerticesSize = boxMesh.m_vertexCount * BoxMeshGenerator::kVertexSize;
        const u64 boxIndicesSize = boxMesh.m_indexCount * sizeof(u32);

        m_vertexRegionSize = boxVerticesSize;
        m_indexRegionSize = boxIndicesSize;

        m_geometryBuffer = _graphicsContext.CreateBuffer({
            .m_desc = {
                .m_size = m_vertexRegionSize + m_indexRegionSize,
#if !defined(KE_FINAL)
                .m_debugName = "Geometry library buffer",
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::VertexBuffer | MemoryUsage::IndexBuffer
                | MemoryUsage::TransferDstBuffer,
        });

        Geometry& box = m_geometries[static_cast<size_t>(GeometryType::Box)];
        box.m_boxHalfExtents = float3(0.5f, 0.5f, 0.5f);
        box.m_bufferViews = GeometryBuffers {
            .m_vertexBuffer = {
                .m_size = boxVerticesSize,
                .m_offset = 0,
                .m_stride = static_cast<u32>(BoxMeshGenerator::kVertexSize),
                .m_buffer = m_geometryBuffer,
            },
            .m_indexBuffer = {
                .m_size = boxIndicesSize,
                .m_offset = m_vertexRegionSize,
                .m_stride = sizeof(u32),
                .m_buffer = m_geometryBuffer,
            },
            // Despite its name, DrawInstanceManager::Model::m_vertexCount is used directly as the
            // indexed draw's element count by PassDispatcher::Dispatch, so callers registering a
            // model with these buffers must pass this index count there, not the vertex count.
            .m_indexCount = boxMesh.m_indexCount,
        };

        m_stagingBuffer = _graphicsContext.CreateBuffer({
            .m_desc = {
                .m_size = m_vertexRegionSize + m_indexRegionSize,
#if !defined(KE_FINAL)
                .m_debugName = "Geometry library staging buffer",
#endif
            },
            .m_usage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::TransferSrcBuffer,
        });

        {
            BufferMapping mapping { m_stagingBuffer };
            _graphicsContext.MapBuffer(mapping);
            memcpy(mapping.m_ptr, boxMesh.m_vertices, boxVerticesSize);
            memcpy(static_cast<std::byte*>(mapping.m_ptr) + m_vertexRegionSize, boxMesh.m_indices, boxIndicesSize);
            _graphicsContext.UnmapBuffer(mapping);
        }

        m_allocator.deallocate(boxMesh.m_vertices, boxVerticesSize);
        m_allocator.deallocate(boxMesh.m_indices, boxIndicesSize);
    }

    GeometryLibrary::~GeometryLibrary() = default;

    const GeometryBuffers& GeometryLibrary::GetBuffers(const GeometryType _type) const
    {
        return m_geometries[static_cast<size_t>(_type)].m_bufferViews;
    }

    b3BoxHull GeometryLibrary::GetBoxHull(const GeometryType _type) const
    {
        const float3& halfExtents = m_geometries[static_cast<size_t>(_type)].m_boxHalfExtents;
        return b3MakeBoxHull(halfExtents.x, halfExtents.y, halfExtents.z);
    }

    void GeometryLibrary::UploadPendingGeometry(
        GraphicsContext& _graphicsContext,
        const TransferCommandEncoderHandle _transferEncoder)
    {
        if (m_uploadRecorded)
        {
            return;
        }
        m_uploadRecorded = true;

        const BufferMemoryBarrier beforeBarriers[2] = {
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferSrc,
                .m_buffer = m_stagingBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::None,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferDst,
                .m_buffer = m_geometryBuffer,
            },
        };
        _graphicsContext.PlaceMemoryBarriers(
            _transferEncoder,
            {
                .m_placementType = BarrierPlacementType::IntraEncoder,
                .m_bufferBarriers = beforeBarriers,
            });

        _graphicsContext.CopyBuffer(_transferEncoder, {
            .m_copySize = m_vertexRegionSize,
            .m_bufferSrc = m_stagingBuffer,
            .m_bufferDst = m_geometryBuffer,
        });
        _graphicsContext.CopyBuffer(_transferEncoder, {
            .m_copySize = m_indexRegionSize,
            .m_bufferSrc = m_stagingBuffer,
            .m_bufferDst = m_geometryBuffer,
            .m_offsetSrc = m_vertexRegionSize,
            .m_offsetDst = m_vertexRegionSize,
        });

        // Split per-region, since the same buffer transitions into two different bind purposes.
        const BufferMemoryBarrier afterBarriers[2] = {
            {
                .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                .m_stagesDst = BarrierSyncStageFlags::VertexInputAssembly,
                .m_accessSrc = BarrierAccessFlags::TransferSrc,
                .m_accessDst = BarrierAccessFlags::VertexBuffer,
                .m_offset = 0,
                .m_size = m_vertexRegionSize,
                .m_buffer = m_geometryBuffer,
            },
            {
                .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                .m_stagesDst = BarrierSyncStageFlags::IndexInputAssembly,
                .m_accessSrc = BarrierAccessFlags::TransferSrc,
                .m_accessDst = BarrierAccessFlags::IndexBuffer,
                .m_offset = m_vertexRegionSize,
                .m_size = m_indexRegionSize,
                .m_buffer = m_geometryBuffer,
            },
        };
        _graphicsContext.PlaceMemoryBarriers(
            _transferEncoder,
            {
                .m_placementType = BarrierPlacementType::Producer,
                .m_bufferBarriers = afterBarriers,
            });

        m_uploadFrameId = _graphicsContext.GetFrameId();
        m_stagingBufferPendingFree = true;
    }

    void GeometryLibrary::Update(GraphicsContext& _graphicsContext)
    {
        if (m_stagingBufferPendingFree && _graphicsContext.IsFrameExecuted(m_uploadFrameId))
        {
            _graphicsContext.DestroyBuffer(m_stagingBuffer);
            m_stagingBufferPendingFree = false;
        }
    }
}
