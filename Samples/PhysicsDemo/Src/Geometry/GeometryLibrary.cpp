/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#include "GeometryLibrary.hpp"

#include <KryneEngine/Core/Graphics/Drawing.hpp>
#include <KryneEngine/Core/Graphics/MemoryBarriers.hpp>
#include <Scene/BoxMeshGenerator.hpp>
#include <Scene/PrimitiveMeshGenerator.hpp>
#include <cstring>

namespace KryneEngine::Samples::PhysicsDemo
{
    namespace
    {
        // Common shape of whichever generator (BoxMeshGenerator or PrimitiveMeshGenerator) built
        // a given geometry's render mesh; both use the same position+normal vertex layout
        // (BoxMeshGenerator::kVertexSize == PrimitiveMeshGenerator::kVertexSize), so a single type
        // can carry either one's output through the rest of the upload pipeline below.
        struct MeshData
        {
            std::byte* m_vertices;
            std::byte* m_indices;
            u32 m_vertexCount;
            u32 m_indexCount;
        };

        // sides/segments passed to the render-mesh generators; the collision hulls built further
        // below (Cylinder, Cone) use their own, coarser tessellation, since a smooth silhouette
        // matters far more for what's drawn than for what's collided against.
        constexpr u32 kMeshRings = 12;
        constexpr u32 kMeshSegments = 24;

        // Rotates a box3d hull generator's Y-up local frame (b3CreateCylinder/b3CreateCone
        // extrude along local Y) onto the engine's Z-up one, so the baked hull lines up with the
        // Z-extruded render meshes PrimitiveMeshGenerator produces. See b3MakeQuatFromAxisAngle's
        // rotation of the Y axis onto Z: rotating +90 degrees about X sends (x, y, z) to
        // (x, -z, y), i.e. the old height axis (y) becomes the new one (z).
        b3HullData* ReorientYUpHull(b3HullData* _yUpHull, const b3Vec3& _postTranslation)
        {
            const b3Transform transform {
                .p = _postTranslation,
                .q = b3MakeQuatFromAxisAngle(b3Vec3{.x = 1.f, .y = 0.f, .z = 0.f}, B3_PI * 0.5f),
            };
            b3HullData* hull = b3CloneAndTransformHull(_yUpHull, transform, b3Vec3{ .x = 1.f, .y = 1.f, .z = 1.f });
            b3DestroyHull(_yUpHull);
            return hull;
        }
    }

    GeometryLibrary::GeometryLibrary(const AllocatorInstance _allocator, GraphicsContext& _graphicsContext)
        : m_allocator(_allocator)
    {
        // Every hardcoded geometry's data is laid out back to back in one combined buffer, used
        // as both vertex and index buffer depending on which region is bound:
        // [vertex data for every geometry][index data for every geometry]. The single staging
        // buffer below mirrors that exact layout, so the whole upload only ever takes two
        // CopyBuffer calls, regardless of how many geometries exist.
        struct GeometryDesc
        {
            GeometryType m_type {};
            float3 m_meshSize {};             // Box, Ground: full extents of the generated box mesh
            float3 m_collisionHalfExtents {};  // Box, Ground: half extents passed to b3MakeBoxHull
            float m_radius = 0.f;              // Sphere, Capsule, Cylinder, Cone
            float m_halfHeight = 0.f;          // Capsule, Cylinder, Cone
        };

        const GeometryDesc kGeometryDescs[] = {
            {
                .m_type = GeometryType::Box,
                .m_meshSize = float3(1.f, 1.f, 1.f),
                .m_collisionHalfExtents = float3(0.5f, 0.5f, 0.5f)
            },
            {
                .m_type = GeometryType::Sphere,
                .m_radius = 0.5f,
            },
            {
                .m_type = GeometryType::Capsule,
                .m_radius = 0.3f,
                .m_halfHeight = 0.35f,
            },
            {
                .m_type = GeometryType::Cylinder,
                .m_radius = 0.5f,
                .m_halfHeight = 0.5f,
            },
            {
                .m_type = GeometryType::Cone,
                .m_radius = 0.5f,
                .m_halfHeight = 0.5f,
            },
            // Visually a thin slab, but the collision hull keeps a zero-thickness top plane so
            // resting bodies sit exactly at the ground entity's origin.
            {
                .m_type = GeometryType::Ground,
                .m_meshSize = float3(200.f, 200.f, 0.01f),
                .m_collisionHalfExtents = float3(100.f, 100.f, 0.f)
            },
        };
        constexpr size_t kGeometryCount = std::size(kGeometryDescs);
        static_assert(kGeometryCount == static_cast<size_t>(GeometryType::Count));

        MeshData meshes[kGeometryCount];
        for (size_t i = 0; i < kGeometryCount; i++)
        {
            const GeometryDesc& desc = kGeometryDescs[i];
            switch (desc.m_type)
            {
            case GeometryType::Sphere:
            {
                const auto mesh = PrimitiveMeshGenerator::GenerateSphereMesh(desc.m_radius, kMeshRings, kMeshSegments, m_allocator);
                meshes[i] = {
                    .m_vertices = mesh.m_vertices,
                    .m_indices = mesh.m_indices,
                    .m_vertexCount = mesh.m_vertexCount,
                    .m_indexCount = mesh.m_indexCount
                };
                break;
            }
            case GeometryType::Capsule:
            {
                const auto mesh = PrimitiveMeshGenerator::GenerateCapsuleMesh(
                    desc.m_radius, desc.m_halfHeight, kMeshRings / 2, kMeshSegments, m_allocator);
                meshes[i] = {
                    .m_vertices = mesh.m_vertices,
                    .m_indices = mesh.m_indices,
                    .m_vertexCount = mesh.m_vertexCount,
                    .m_indexCount = mesh.m_indexCount
                };
                break;
            }
            case GeometryType::Cylinder:
            {
                const auto mesh = PrimitiveMeshGenerator::GenerateCylinderMesh(desc.m_radius, desc.m_halfHeight, kMeshSegments, m_allocator);
                meshes[i] = {
                    .m_vertices = mesh.m_vertices,
                    .m_indices = mesh.m_indices,
                    .m_vertexCount = mesh.m_vertexCount,
                    .m_indexCount = mesh.m_indexCount
                };
                break;
            }
            case GeometryType::Cone:
            {
                const auto mesh = PrimitiveMeshGenerator::GenerateConeMesh(desc.m_radius, desc.m_halfHeight, kMeshSegments, m_allocator);
                meshes[i] = {
                    .m_vertices = mesh.m_vertices,
                    .m_indices = mesh.m_indices,
                    .m_vertexCount = mesh.m_vertexCount,
                    .m_indexCount = mesh.m_indexCount
                };
                break;
            }
            case GeometryType::Box:
            case GeometryType::Ground:
            default:
            {
                const auto mesh = BoxMeshGenerator::GenerateBoxMesh(desc.m_meshSize, m_allocator);
                meshes[i] = {
                    .m_vertices = mesh.m_vertices,
                    .m_indices = mesh.m_indices,
                    .m_vertexCount = mesh.m_vertexCount,
                    .m_indexCount = mesh.m_indexCount
                };
                break;
            }
            }
            m_vertexRegionSize += meshes[i].m_vertexCount * BoxMeshGenerator::kVertexSize;
            m_indexRegionSize += meshes[i].m_indexCount * sizeof(u32);
        }

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

        m_stagingBuffer = _graphicsContext.CreateBuffer({
            .m_desc = {
                .m_size = m_vertexRegionSize + m_indexRegionSize,
#if !defined(KE_FINAL)
                .m_debugName = "Geometry library staging buffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        });

        {
            BufferMapping mapping { m_stagingBuffer };
            _graphicsContext.MapBuffer(mapping);

            u64 vertexCursor = 0;
            u64 indexCursor = m_vertexRegionSize;
            for (size_t i = 0; i < kGeometryCount; i++)
            {
                const MeshData& mesh = meshes[i];
                const u64 verticesSize = mesh.m_vertexCount * BoxMeshGenerator::kVertexSize;
                const u64 indicesSize = mesh.m_indexCount * sizeof(u32);

                const GeometryDesc& desc = kGeometryDescs[i];
                Geometry& geometry = m_geometries[static_cast<size_t>(desc.m_type)];
                geometry.m_boxHalfExtents = desc.m_collisionHalfExtents;
                geometry.m_radius = desc.m_radius;
                geometry.m_halfHeight = desc.m_halfHeight;
                geometry.m_bufferViews = GeometryBuffers {
                    .m_vertexBuffer = {
                        .m_size = verticesSize,
                        .m_offset = vertexCursor,
                        .m_stride = static_cast<u32>(BoxMeshGenerator::kVertexSize),
                        .m_buffer = m_geometryBuffer,
                    },
                    .m_indexBuffer = {
                        .m_size = indicesSize,
                        .m_offset = indexCursor,
                        .m_stride = sizeof(u32),
                        .m_buffer = m_geometryBuffer,
                    },
                    // Despite its name, DrawInstanceManager::Model::m_vertexCount is used directly
                    // as the indexed draw's element count by PassDispatcher::Dispatch, so callers
                    // registering a model with these buffers must pass this index count there, not
                    // the vertex count.
                    .m_indexCount = mesh.m_indexCount,
                };

                memcpy(mapping.m_ptr + vertexCursor, mesh.m_vertices, verticesSize);
                memcpy(mapping.m_ptr + indexCursor, mesh.m_indices, indicesSize);

                vertexCursor += verticesSize;
                indexCursor += indicesSize;

                m_allocator.deallocate(mesh.m_vertices, verticesSize);
                m_allocator.deallocate(mesh.m_indices, indicesSize);
            }

            _graphicsContext.UnmapBuffer(mapping);
        }

        // Box3D has no dedicated cylinder/cone shape type (unlike sphere and capsule); the closest
        // it offers is a tessellated hull (b3CreateCylinder/b3CreateCone), built along local Y. It
        // is built once here - rather than per-body in a scene template - both because it's
        // comparatively expensive (unlike a plain b3Sphere/b3Capsule value) and because the same
        // b3HullData* can be handed to b3CreateHullShape for every body that needs it (see
        // GetHull()); this library owns it and frees it in the destructor below.
        {
            Geometry& cylinder = m_geometries[static_cast<size_t>(GeometryType::Cylinder)];
            b3HullData* rawCylinder = b3CreateCylinder(2.f * cylinder.m_halfHeight, cylinder.m_radius, -cylinder.m_halfHeight, 16);
            cylinder.m_hull = ReorientYUpHull(rawCylinder, b3Vec3{ .x = 0.f, .y = 0.f, .z = 0.f });

            Geometry& cone = m_geometries[static_cast<size_t>(GeometryType::Cone)];
            // b3CreateCone always builds a frustum (both radii must be > 0); a near-zero tip radius
            // approximates a true cone closely enough for collision purposes.
            b3HullData* rawCone = b3CreateCone(2.f * cone.m_halfHeight, cone.m_radius, 0.01f, 16);
            cone.m_hull = ReorientYUpHull(rawCone, b3Vec3{ .x = 0.f, .y = 0.f, .z = -cone.m_halfHeight });
        }
    }

    GeometryLibrary::~GeometryLibrary()
    {
        b3DestroyHull(m_geometries[static_cast<size_t>(GeometryType::Cylinder)].m_hull);
        b3DestroyHull(m_geometries[static_cast<size_t>(GeometryType::Cone)].m_hull);
    }

    const GeometryBuffers& GeometryLibrary::GetBuffers(const GeometryType _type) const
    {
        return m_geometries[static_cast<size_t>(_type)].m_bufferViews;
    }

    b3BoxHull GeometryLibrary::GetBoxHull(const GeometryType _type) const
    {
        const float3& halfExtents = m_geometries[static_cast<size_t>(_type)].m_boxHalfExtents;
        return b3MakeBoxHull(halfExtents.x, halfExtents.y, halfExtents.z);
    }

    b3Sphere GeometryLibrary::GetSphere(const GeometryType _type) const
    {
        return b3Sphere { .center = { .x = 0.f, .y = 0.f, .z = 0.f }, .radius = m_geometries[static_cast<size_t>(_type)].m_radius };
    }

    b3Capsule GeometryLibrary::GetCapsule(const GeometryType _type) const
    {
        const Geometry& geometry = m_geometries[static_cast<size_t>(_type)];
        return b3Capsule {
            .center1 = { .x = 0.f, .y = 0.f, .z = -geometry.m_halfHeight },
            .center2 = { .x = 0.f, .y = 0.f, .z = geometry.m_halfHeight },
            .radius = geometry.m_radius,
        };
    }

    const b3HullData* GeometryLibrary::GetHull(const GeometryType _type) const
    {
        return m_geometries[static_cast<size_t>(_type)].m_hull;
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
