/**
 * @file
 * @author Max Godefroy
 * @date 02/08/2024.
 */

#pragma once

#include <EASTL/span.h>
#include "Enums.hpp"
#include "Handles.hpp"
#include "KryneEngine/Core/Common/BitUtils.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.hpp"

namespace KryneEngine
{
    enum class BarrierSyncStageFlags : u32
    {
        None                        = 1 << 0,
        All                         = 1 << 1,
        ExecuteIndirect             = 1 << 2,
        IndexInputAssembly          = 1 << 3,
        VertexInputAssembly         = 1 << 4,
        VertexShading               = 1 << 5,
        FragmentShading             = 1 << 6,
        ColorBlending               = 1 << 7,
        DepthStencilTesting         = 1 << 8,
        Transfer                    = 1 << 9,
        MultiSampleResolve          = 1 << 10,
        ComputeShading              = 1 << 11,
        AllShading                  = 1 << 12,
        Raytracing                  = 1 << 13,
        AccelerationStructureBuild  = 1 << 14,
        AccelerationStructureCopy   = 1 << 15,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(BarrierSyncStageFlags)

    enum class BarrierAccessFlags: u32
    {
        VertexBuffer                = 1 << 0,
        IndexBuffer                 = 1 << 1,
        ConstantBuffer              = 1 << 2,
        IndirectBuffer              = 1 << 3,
        ColorAttachment             = 1 << 4,
        DepthStencilRead            = 1 << 5,
        DepthStencilWrite           = 1 << 6,
        ShaderResource              = 1 << 7,
        UnorderedAccess             = 1 << 8,
        ResolveSrc                  = 1 << 9,
        ResolveDst                  = 1 << 10,
        TransferSrc                 = 1 << 11,
        TransferDst                 = 1 << 12,
        AccelerationStructureRead   = 1 << 13,
        AccelerationStructureWrite  = 1 << 14,
        ShadingRate                 = 1 << 15,
        AllRead                     = 1 << 16,
        AllWrite                    = 1 << 17,
        All                         = AllRead | AllWrite,
        None                        = 1 << 18,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(BarrierAccessFlags)

    enum class BarrierPlacementType
    {
        /**
         * @brief A set of memory barriers to execute within the current command encoder.
         *
         * @details
         * Use this when you need explicit synchronisation within the same command encoder.
         */
        IntraEncoder,

        /**
         * @brief A set of memory barriers that must be executed before the next command encoders.
         *
         * @details
         * If you need to *produce* memory access barriers relevant for later passes, use this barrier type.
         * Can be dispatched during the encoder pass or at the end of it.
         */
        Producer,

        /**
         * @brief A set of memory barriers that must be executed before this command encoder continues execution.
         *
         * @details
         * If you need to *consume* memory access barriers relevant for the current pass, use this barrier type.
         * Should be dispatched at the beginning of the encoder pass.
         */
        Consumer,
    };

    struct GlobalMemoryBarrier
    {
        BarrierSyncStageFlags m_stagesSrc;
        BarrierSyncStageFlags m_stagesDst;
        BarrierAccessFlags m_accessSrc;
        BarrierAccessFlags m_accessDst;
    };

    struct BufferMemoryBarrier
    {
        BarrierSyncStageFlags m_stagesSrc;
        BarrierSyncStageFlags m_stagesDst;
        BarrierAccessFlags m_accessSrc;
        BarrierAccessFlags m_accessDst;

        u64 m_offset = 0;
        u64 m_size = eastl::numeric_limits<u64>::max();
        BufferHandle m_buffer;
    };

    struct TextureMemoryBarrier
    {
        BarrierSyncStageFlags m_stagesSrc;
        BarrierSyncStageFlags m_stagesDst;
        BarrierAccessFlags m_accessSrc;
        BarrierAccessFlags m_accessDst;

        TextureHandle m_texture;
        u16 m_arrayStart = 0;
        u16 m_arrayCount = 1;
        TextureLayout m_layoutSrc;
        TextureLayout m_layoutDst;
        u8 m_mipStart = 0;
        u8 m_mipCount = 1;

        TexturePlane m_planes = TexturePlane::Color;
    };

    struct MemoryBarriers
    {
        BarrierPlacementType m_placementType = BarrierPlacementType::IntraEncoder;
        eastl::span<const GlobalMemoryBarrier> m_globalBarriers {};
        eastl::span<const BufferMemoryBarrier> m_bufferBarriers {};
        eastl::span<const TextureMemoryBarrier> m_textureBarriers {};
    };
}
