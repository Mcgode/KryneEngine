/**
 * @file
 * @author Max Godefroy
 * @date 12/09/2026.
 */

#pragma once

#include <box3d/box3d.h>
#include <EASTL/array.h>
#include <KryneEngine/Core/Graphics/Buffer.hpp>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Math/Vector.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

namespace KryneEngine::Samples::PhysicsDemo
{
    enum class GeometryType : u8
    {
        Box,
        Ground,

        Count,
    };

    struct GeometryBuffers
    {
        BufferSpan m_vertexBuffer;
        BufferSpan m_indexBuffer;
        u32 m_indexCount;
    };

    /**
     * @brief A hardcoded set of predefined geometries (a unit box, and the static ground slab)
     * that world objects can be spawned with: their GPU vertex/index buffers, and the matching
     * Box3D collision shape to attach to a physics body.
     *
     * @details
     * This does not register anything with DrawInstanceManager itself - callers retrieve the
     * buffers via GetBuffers() and register their own model/material combination.
     *
     * Since the full set of geometries is known upfront, every one of them is packed into a
     * single combined buffer - vertex data first, then index data - each geometry (and each of
     * its vertex/index regions) getting its own byte range via BufferSpan, bound as either a
     * vertex or an index buffer depending on which offset is used. This is supported uniformly by
     * Vulkan (a VkBuffer's usage flags simply OR together), DirectX12 and Metal (buffer resources
     * there have no creation-time usage restriction at all - vertex/index is purely a view-time
     * distinction).
     *
     * Buffer uploads are recorded into a transfer encoder handed down by the caller
     * (UploadPendingGeometry); this class never opens a command buffer of its own - those are
     * only ever created at the top level (see PhysicsDemo.cpp / SceneManager).
     */
    class GeometryLibrary
    {
    public:
        GeometryLibrary(AllocatorInstance _allocator, GraphicsContext& _graphicsContext);
        ~GeometryLibrary();

        [[nodiscard]] const GeometryBuffers& GetBuffers(GeometryType _type) const;

        // Returns a Box3D hull matching _type's visual dimensions, ready to pass to
        // b3CreateHullShape. Only meaningful for box-shaped geometries.
        [[nodiscard]] b3BoxHull GetBoxHull(GeometryType _type) const;

        // Records this library's pending geometry uploads into _transferEncoder, which must have
        // already been opened by the caller. Must be called before any of this geometry is first
        // drawn; a no-op on every call after the first.
        void UploadPendingGeometry(GraphicsContext& _graphicsContext, TransferCommandEncoderHandle _transferEncoder);

        // Must be called once per rendered frame: frees the upload staging buffer once the GPU
        // has finished consuming it.
        void Update(GraphicsContext& _graphicsContext);

    private:
        struct Geometry
        {
            GeometryBuffers m_bufferViews;
            float3 m_boxHalfExtents; // only meaningful for box-shaped geometries
        };

        AllocatorInstance m_allocator;

        eastl::array<Geometry, static_cast<size_t>(GeometryType::Count)> m_geometries {};

        // Shared by every geometry above: [vertex data for every geometry][index data for every
        // geometry], each geometry occupying its own byte range within each region (see
        // Geometry::m_bufferViews' BufferSpan offsets).
        BufferHandle m_geometryBuffer {};
        u64 m_vertexRegionSize = 0;
        u64 m_indexRegionSize = 0;

        BufferHandle m_stagingBuffer {};
        bool m_uploadRecorded = false;
        bool m_stagingBufferPendingFree = false;
        u64 m_uploadFrameId = 0;
    };
}
