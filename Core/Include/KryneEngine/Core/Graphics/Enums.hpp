/**
 * @file
 * @author Max Godefroy
 * @date 03/04/2022.
 */

#pragma once

#include <EASTL/array.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/BitUtils.hpp"

namespace KryneEngine
{
    enum class TextureFormat : u16
    {
        NoFormat,

        R8_UNorm,
        RG8_UNorm,
        RGB8_UNorm,
        RGBA8_UNorm,

        RGB8_sRGB,
        RGBA8_sRGB,

        // Present special formats
        BGRA8_UNorm,
        BGRA8_sRGB,

        R8_SNorm,
        RG8_SNorm,
        RGB8_SNorm,
        RGBA8_SNorm,

        R16_Float,
        RG16_Float,
        RGB16_Float,
        RGBA16_Float,

        R32_Float,
        RG32_Float,
        RGB32_Float,
        RGBA32_Float,

        R32_UInt,
        RG32_UInt,
        RGB32_UInt,
        RGBA32_UInt,

        D16,
        D24,
        D24S8,
        D32F,
        D32FS8,

        Count,
    };

    enum class TextureTypes: u8
    {
        Single1D,
        Single2D,
        Single3D,
        Array1D,
        Array2D,
        SingleCube,
        ArrayCube,
    };

    enum class TextureUsage : u8
    {
        TransferSource          = 1 << 0,
        TransferDestination     = 1 << 1,
        ShaderSampling          = 1 << 2,
        UnorderedAccess         = 1 << 3,
        ColorAttachment         = 1 << 4,
        DepthStencilAttachment  = 1 << 5,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(TextureUsage)

    enum class TextureLayout: u8
    {
        Unknown,
        Common,
        Present,
        GenericRead,
        ColorAttachment,
        DepthStencilAttachment,
        DepthStencilReadOnly,
        UnorderedAccess,
        ShaderResource,
        TransferSrc,
        TransferDst,
        ResolveSrc,
        ResolveDst,
        ShadingRate,
    };

    enum class TexturePlane: u8
    {
        Color   = 1 << 0,
        Depth   = 1 << 1,
        Stencil = 1 << 2,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(TexturePlane)

    enum class MemoryUsage : u16
    {
        // Memory usage type saved in first 3 bits
        Undefined_UsageType         = 0 << 0,
        GpuOnly_UsageType           = 1 << 0,
        StageOnce_UsageType         = 2 << 0,
        StageEveryFrame_UsageType   = 3 << 0,
        Readback_UsageType          = 4 << 0,
        USAGE_TYPE_MASK             = BitUtils::BitMask<u16>(3, 0),

        // Buffer specific flags
        TransferSrcBuffer   = 1 << 3,
        TransferDstBuffer   = 1 << 4,
        ConstantBuffer      = 1 << 5,
        ReadBuffer          = 1 << 6,
        WriteBuffer         = 1 << 7,
        ReadWriteBuffer     = ReadBuffer | WriteBuffer,
        IndexBuffer         = 1 << 8,
        VertexBuffer        = 1 << 9,
        IndirectBuffer      = 1 << 10,
        AccelerationStruct  = 1 << 11,

        // Image specific flags
        TransferSrcImage        = 1 << 3,
        TransferDstImage        = 1 << 4,
        SampledImage            = 1 << 5,
        ReadImage               = 1 << 6,
        WriteImage              = 1 << 7,
        ReadWriteImage          = ReadImage | WriteImage,
        ColorTargetImage        = 1 << 8,
        DepthStencilTargetImage = 1 << 9,

        // Invalid setup
        Invalid = 0xffff,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(MemoryUsage)

    enum class TextureComponentMapping: u8
    {
        Red,
        Green,
        Blue,
        Alpha,
        Zero,
        One,
    };
    using Texture4ComponentsMapping = eastl::array<TextureComponentMapping, 4>;
    static constexpr Texture4ComponentsMapping kDefaultTexture4ComponentsMapping = {
        TextureComponentMapping::Red,
        TextureComponentMapping::Green,
        TextureComponentMapping::Blue,
        TextureComponentMapping::Alpha
    };
}