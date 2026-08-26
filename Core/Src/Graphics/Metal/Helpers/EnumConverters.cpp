/**
 * @file
 * @author Max Godefroy
 * @date 02/11/2024.
 */

#include "Graphics/Metal/Helpers/EnumConverters.hpp"

namespace KryneEngine::MetalConverters
{
    size_t GetPixelByteSize(TextureFormat _format)
    {
        static_assert(static_cast<u32>(TextureFormat::Count) == 30, "Enum values changed, please update");

        switch (_format)
        {
        case TextureFormat::NoFormat:
        case TextureFormat::Count:
        case TextureFormat::D24:
        case TextureFormat::RGB8_UNorm:
        case TextureFormat::RGB8_sRGB:
        case TextureFormat::RGB8_SNorm:
        case TextureFormat::RGB16_Float:
        case TextureFormat::RGB32_Float:
        case TextureFormat::RGB32_UInt:
            return 0;
        case TextureFormat::R8_UNorm:
        case TextureFormat::R8_SNorm:
            return 1;
        case TextureFormat::RG8_UNorm:
        case TextureFormat::RG8_SNorm:
        case TextureFormat::R16_Float:
        case TextureFormat::D16:
            return 2;
        case TextureFormat::RGBA8_UNorm:
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::BGRA8_UNorm:
        case TextureFormat::BGRA8_sRGB:
        case TextureFormat::RGBA8_SNorm:
        case TextureFormat::RG16_Float:
        case TextureFormat::R32_Float:
        case TextureFormat::R32_UInt:
        case TextureFormat::D24S8:
        case TextureFormat::D32F:
            return 4;
        case TextureFormat::RGBA16_Float:
        case TextureFormat::RG32_Float:
        case TextureFormat::RG32_UInt:
        case TextureFormat::D32FS8:
            return 8;
        case TextureFormat::RGBA32_Float:
        case TextureFormat::RGBA32_UInt:
            return 16;
        }
    }

    MTL::PixelFormat ToPixelFormat(TextureFormat _format)
    {
        static_assert(static_cast<u32>(TextureFormat::Count) == 30, "Enum values changed, please update");

        switch (_format)
        {
        case TextureFormat::NoFormat:
        case TextureFormat::RGB8_UNorm:
        case TextureFormat::RGB8_sRGB:
        case TextureFormat::RGB8_SNorm:
        case TextureFormat::RGB16_Float:
        case TextureFormat::RGB32_Float:
        case TextureFormat::RGB32_UInt:
        case TextureFormat::D24:
            KE_ASSERT_FATAL_MSG(_format == TextureFormat::NoFormat, "Unsupported format");
            return MTL::PixelFormatInvalid;
        case TextureFormat::R8_UNorm:
            return MTL::PixelFormatR8Unorm;
        case TextureFormat::RG8_UNorm:
            return MTL::PixelFormatRG8Unorm;
        case TextureFormat::RGBA8_UNorm:
            return MTL::PixelFormatRGBA8Unorm;
        case TextureFormat::RGBA8_sRGB:
            return MTL::PixelFormatRGBA8Unorm_sRGB;
        case TextureFormat::BGRA8_UNorm:
            return MTL::PixelFormatBGRA8Unorm;
        case TextureFormat::BGRA8_sRGB:
            return MTL::PixelFormatBGRA8Unorm_sRGB;
        case TextureFormat::R8_SNorm:
            return MTL::PixelFormatR8Snorm;
        case TextureFormat::RG8_SNorm:
            return MTL::PixelFormatRG8Snorm;
        case TextureFormat::RGBA8_SNorm:
            return MTL::PixelFormatRGBA8Snorm;
        case TextureFormat::R16_Float:
            return MTL::PixelFormatR16Float;
        case TextureFormat::RG16_Float:
            return MTL::PixelFormatRG16Float;
        case TextureFormat::RGBA16_Float:
            return MTL::PixelFormatRGBA16Float;
        case TextureFormat::R32_Float:
            return MTL::PixelFormatR32Float;
        case TextureFormat::RG32_Float:
            return MTL::PixelFormatRG32Float;
        case TextureFormat::RGBA32_Float:
            return MTL::PixelFormatRGBA32Float;
        case TextureFormat::R32_UInt:
            return MTL::PixelFormatR32Uint;
        case TextureFormat::RG32_UInt:
            return MTL::PixelFormatRG32Uint;
        case TextureFormat::RGBA32_UInt:
            return MTL::PixelFormatRGBA32Uint;
        case TextureFormat::D16:
            return MTL::PixelFormatDepth16Unorm;
        case TextureFormat::D24S8:
            return MTL::PixelFormatDepth24Unorm_Stencil8;
        case TextureFormat::D32F:
            return MTL::PixelFormatDepth32Float;
        case TextureFormat::D32FS8:
            return MTL::PixelFormatDepth32Float_Stencil8;
        case TextureFormat::Count:
            KE_ASSERT_FATAL_MSG(false, "Invalid TextureFormat");
            return MTL::PixelFormatInvalid;
        }
    }

    MTL::SamplerMinMagFilter GetMinMagFilter(SamplerDesc::Filter _filter)
    {
        switch (_filter)
        {
        case SamplerDesc::Filter::Point:
            return MTL::SamplerMinMagFilterNearest;
        case SamplerDesc::Filter::Linear:
            return MTL::SamplerMinMagFilterLinear;
        }
    }

    MTL::SamplerMipFilter GetMipFilter(SamplerDesc::Filter _filter)
    {
        switch (_filter)
        {
        case SamplerDesc::Filter::Point:
            return MTL::SamplerMipFilterNearest;
        case SamplerDesc::Filter::Linear:
            return MTL::SamplerMipFilterLinear;
        }
    }

    MTL::SamplerAddressMode GetAddressMode(SamplerDesc::AddressMode _mode)
    {
        switch (_mode)
        {
        case SamplerDesc::AddressMode::Repeat:
            return MTL::SamplerAddressModeRepeat;
        case SamplerDesc::AddressMode::MirroredRepeat:
            return MTL::SamplerAddressModeMirrorRepeat;
        case SamplerDesc::AddressMode::Border:
            return MTL::SamplerAddressModeClampToBorderColor;
        case SamplerDesc::AddressMode::Clamp:
            return MTL::SamplerAddressModeClampToEdge;
        }
    }

    MTL::ResourceOptions GetResourceStorage(MemoryUsage _memoryUsage)
    {
        switch (_memoryUsage & MemoryUsage::USAGE_TYPE_MASK)
        {
        case MemoryUsage::StageOnce_UsageType:
            return MTL::ResourceStorageModeShared;
        case MemoryUsage::StageEveryFrame_UsageType:
#if defined(TARGET_OS_MAC)
            return MTL::ResourceStorageModeManaged;
#else
            return MTL::ResourceStorageModeShared;
#endif
        case MemoryUsage::GpuOnly_UsageType:
            return MTL::ResourceStorageModePrivate;
        case MemoryUsage::Readback_UsageType:
#if defined(TARGET_OS_MAC)
            return MTL::ResourceStorageModeManaged;
#else
            return MTL::ResourceStorageModeShared;
#endif
        default:
            return 0;
        }
    }

    MTL::StorageMode GetStorageMode(MemoryUsage _memoryUsage)
    {
        switch (_memoryUsage & MemoryUsage::USAGE_TYPE_MASK)
        {
        case MemoryUsage::StageOnce_UsageType:
            return MTL::StorageModeShared;
        case MemoryUsage::StageEveryFrame_UsageType:
#if defined(TARGET_OS_MAC)
            return MTL::StorageModeManaged;
#else
            return MTL::StorageModeShared;
#endif
        case MemoryUsage::GpuOnly_UsageType:
            return MTL::StorageModePrivate;
        case MemoryUsage::Readback_UsageType:
#if defined(TARGET_OS_MAC)
            return MTL::StorageModeManaged;
#else
            return MTL::StorageModeShared;
#endif
        default:
            return MTL::StorageModeShared;
        }
    }

    MTL::TextureSwizzle GetSwizzle(TextureComponentMapping _mapping)
    {
        switch (_mapping)
        {
        case TextureComponentMapping::Red:
            return MTL::TextureSwizzleRed;
        case TextureComponentMapping::Green:
            return MTL::TextureSwizzleGreen;
        case TextureComponentMapping::Blue:
            return MTL::TextureSwizzleBlue;
        case TextureComponentMapping::Alpha:
            return MTL::TextureSwizzleAlpha;
        case TextureComponentMapping::Zero:
            return MTL::TextureSwizzleZero;
        case TextureComponentMapping::One:
            return MTL::TextureSwizzleOne;
        }
    }

    MTL::TextureType GetTextureType(TextureTypes _type)
    {
        switch (_type)
        {
        case TextureTypes::Single1D:
            return MTL::TextureType1D;
        case TextureTypes::Single2D:
            return MTL::TextureType2D;
        case TextureTypes::Single3D:
            return MTL::TextureType3D;
        case TextureTypes::Array1D:
            return MTL::TextureType1DArray;
        case TextureTypes::Array2D:
            return MTL::TextureType2DArray;
        case TextureTypes::SingleCube:
            return MTL::TextureTypeCube;
        case TextureTypes::ArrayCube:
            return MTL::TextureTypeCubeArray;
        }
    }

    MTL::TextureUsage GetTextureUsage(MemoryUsage _usage)
    {
        MTL::TextureUsage usage {};
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::ReadImage | MemoryUsage::SampledImage))
        {
            usage |= MTL::TextureUsageShaderRead;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::WriteImage))
        {
            usage |= MTL::TextureUsageShaderWrite;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::ColorTargetImage | MemoryUsage::DepthStencilTargetImage))
        {
            usage |= MTL::TextureUsageRenderTarget;
        }
        return usage;
    }

    MTL::DataType GetDataType(DescriptorBindingDesc::Type _type)
    {
        switch (_type)
        {
        case DescriptorBindingDesc::Type::Sampler:
            return MTL::DataTypeSampler;
        case DescriptorBindingDesc::Type::SampledTexture:
        case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
        case DescriptorBindingDesc::Type::StorageReadWriteTexture:
            return MTL::DataTypeTexture;
        case DescriptorBindingDesc::Type::ConstantBuffer:
        case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
        case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
            return MTL::DataTypePointer;
        }
    }

    MTL::BindingAccess GetBindingAccess(DescriptorBindingDesc::Type _type)
    {
        switch (_type)
        {
        case DescriptorBindingDesc::Type::Sampler:
        case DescriptorBindingDesc::Type::SampledTexture:
        case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
        case DescriptorBindingDesc::Type::ConstantBuffer:
        case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
            return MTL::ArgumentAccessReadOnly;
        case DescriptorBindingDesc::Type::StorageReadWriteTexture:
        case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
            return MTL::ArgumentAccessReadWrite;
        }
    }

    MTL::VertexFormat GetVertexFormat(TextureFormat _format)
    {
        static_assert(static_cast<u32>(TextureFormat::Count) == 30, "Enum values changed, please update");
        switch (_format)
        {
        case TextureFormat::NoFormat:
            return MTL::VertexFormatInvalid;
        case TextureFormat::R8_UNorm:
            return MTL::VertexFormatUCharNormalized;
        case TextureFormat::RG8_UNorm:
            return MTL::VertexFormatUChar2Normalized;
        case TextureFormat::RGB8_UNorm:
            return MTL::VertexFormatUChar3Normalized;
        case TextureFormat::RGBA8_UNorm:
            return MTL::VertexFormatUChar4Normalized;
        case TextureFormat::R8_SNorm:
            return MTL::VertexFormatCharNormalized;
        case TextureFormat::RG8_SNorm:
            return MTL::VertexFormatChar2Normalized;
        case TextureFormat::RGB8_SNorm:
            return MTL::VertexFormatChar3Normalized;
        case TextureFormat::RGBA8_SNorm:
            return MTL::VertexFormatChar4Normalized;
        case TextureFormat::R16_Float:
            return MTL::VertexFormatHalf;
        case TextureFormat::RG16_Float:
            return MTL::VertexFormatHalf2;
        case TextureFormat::RGB16_Float:
            return MTL::VertexFormatHalf3;
        case TextureFormat::RGBA16_Float:
            return MTL::VertexFormatHalf4;
        case TextureFormat::R32_Float:
            return MTL::VertexFormatFloat;
        case TextureFormat::RG32_Float:
            return MTL::VertexFormatFloat2;
        case TextureFormat::RGB32_Float:
            return MTL::VertexFormatFloat3;
        case TextureFormat::RGBA32_Float:
            return MTL::VertexFormatFloat4;
        case TextureFormat::R32_UInt:
            return MTL::VertexFormatUInt;
        case TextureFormat::RG32_UInt:
            return MTL::VertexFormatUInt2;
        case TextureFormat::RGB32_UInt:
            return MTL::VertexFormatUInt3;
        case TextureFormat::RGBA32_UInt:
            return MTL::VertexFormatUInt4;
        default:
            KE_ERROR("Unsupported format");
            return MTL::VertexFormatInvalid;
        }
    }

    MTL::BlendOperation GetBlendOperation(ColorAttachmentBlendDesc::BlendOp _op)
    {
        switch(_op)
        {
        case ColorAttachmentBlendDesc::BlendOp::Add:
            return MTL::BlendOperationAdd;
        case ColorAttachmentBlendDesc::BlendOp::Subtract:
            return MTL::BlendOperationSubtract;
        case ColorAttachmentBlendDesc::BlendOp::ReverseSubtract:
            return MTL::BlendOperationReverseSubtract;
        case ColorAttachmentBlendDesc::BlendOp::Min:
            return MTL::BlendOperationMin;
        case ColorAttachmentBlendDesc::BlendOp::Max:
            return MTL::BlendOperationMax;
        }
    }

    MTL::BlendFactor GetBlendFactor(ColorAttachmentBlendDesc::BlendFactor _factor)
    {
        switch (_factor)
        {
        case ColorAttachmentBlendDesc::BlendFactor::Zero:
            return MTL::BlendFactorZero;
        case ColorAttachmentBlendDesc::BlendFactor::One:
            return MTL::BlendFactorOne;
        case ColorAttachmentBlendDesc::BlendFactor::SrcColor:
            return MTL::BlendFactorSourceColor;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcColor:
            return MTL::BlendFactorOneMinusSourceColor;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlpha:
            return MTL::BlendFactorSourceAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcAlpha:
            return MTL::BlendFactorOneMinusSourceAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::DstColor:
            return MTL::BlendFactorDestinationColor;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstColor:
            return MTL::BlendFactorOneMinusDestinationColor;
        case ColorAttachmentBlendDesc::BlendFactor::DstAlpha:
            return MTL::BlendFactorDestinationAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstAlpha:
            return MTL::BlendFactorOneMinusDestinationAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlphaSaturate:
            return MTL::BlendFactorSourceAlphaSaturated;
        case ColorAttachmentBlendDesc::BlendFactor::FactorColor:
            return MTL::BlendFactorBlendColor;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorColor:
            return MTL::BlendFactorOneMinusBlendColor;
        case ColorAttachmentBlendDesc::BlendFactor::FactorAlpha:
            return MTL::BlendFactorBlendAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorAlpha:
            return MTL::BlendFactorOneMinusBlendAlpha;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Color:
            return MTL::BlendFactorSource1Color;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Color:
            return MTL::BlendFactorOneMinusSource1Color;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Alpha:
            return MTL::BlendFactorSource1Alpha;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Alpha:
            return MTL::BlendFactorOneMinusSource1Alpha;
        }
    }

    MTL::ColorWriteMask GetColorWriteMask(ColorAttachmentBlendDesc::WriteMask _mask)
    {
        MTL::ColorWriteMask mask = 0;
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Red))
        {
            mask |= MTL::ColorWriteMaskRed;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Green))
        {
            mask |= MTL::ColorWriteMaskGreen;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Blue))
        {
            mask |= MTL::ColorWriteMaskBlue;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Alpha))
        {
            mask |= MTL::ColorWriteMaskAlpha;
        }
        return mask;
    }

    MTL::CompareFunction GetCompareOperation(DepthStencilStateDesc::CompareOp _op)
    {
        switch (_op)
        {
        case DepthStencilStateDesc::CompareOp::Never:
            return MTL::CompareFunctionNever;
        case DepthStencilStateDesc::CompareOp::Less:
            return MTL::CompareFunctionLess;
        case DepthStencilStateDesc::CompareOp::Equal:
            return MTL::CompareFunctionEqual;
        case DepthStencilStateDesc::CompareOp::LessEqual:
            return MTL::CompareFunctionLessEqual;
        case DepthStencilStateDesc::CompareOp::Greater:
            return MTL::CompareFunctionGreater;
        case DepthStencilStateDesc::CompareOp::NotEqual:
            return MTL::CompareFunctionNotEqual;
        case DepthStencilStateDesc::CompareOp::GreaterEqual:
            return MTL::CompareFunctionGreaterEqual;
        case DepthStencilStateDesc::CompareOp::Always:
            return MTL::CompareFunctionAlways;
        }
    }

    MTL::StencilOperation GetStencilOperation(DepthStencilStateDesc::StencilOp _op)
    {
        switch (_op)
        {
        case DepthStencilStateDesc::StencilOp::Keep:
            return MTL::StencilOperationKeep;
        case DepthStencilStateDesc::StencilOp::Zero:
            return MTL::StencilOperationZero;
        case DepthStencilStateDesc::StencilOp::Replace:
            return MTL::StencilOperationReplace;
        case DepthStencilStateDesc::StencilOp::IncrementAndClamp:
            return MTL::StencilOperationIncrementClamp;
        case DepthStencilStateDesc::StencilOp::DecrementAndClamp:
            return MTL::StencilOperationDecrementClamp;
        case DepthStencilStateDesc::StencilOp::Invert:
            return MTL::StencilOperationInvert;
        case DepthStencilStateDesc::StencilOp::IncrementAndWrap:
            return MTL::StencilOperationIncrementWrap;
        case DepthStencilStateDesc::StencilOp::DecrementAndWrap:
            return MTL::StencilOperationDecrementWrap;
        }
    }

    MTL::LoadAction GetMetalLoadOperation(RenderPassDesc::Attachment::LoadOperation _op)
    {
        switch (_op)
        {
        case RenderPassDesc::Attachment::LoadOperation::Load:
            return MTL::LoadActionLoad;
        case RenderPassDesc::Attachment::LoadOperation::Clear:
            return MTL::LoadActionClear;
        case RenderPassDesc::Attachment::LoadOperation::DontCare:
            return MTL::LoadActionDontCare;
        }
    }

    MTL::StoreAction GetMetalStoreOperation(RenderPassDesc::Attachment::StoreOperation _op)
    {
        switch (_op)
        {
        case RenderPassDesc::Attachment::StoreOperation::Store:
            return MTL::StoreActionStore;
        case RenderPassDesc::Attachment::StoreOperation::Resolve:
            return MTL::StoreActionStoreAndMultisampleResolve;
        case RenderPassDesc::Attachment::StoreOperation::DontCare:
            return MTL::StoreActionDontCare;
        }
    }

    MTL::TriangleFillMode GetTriangleFillMode(RasterStateDesc::FillMode _mode)
    {
        switch (_mode)
        {
        case RasterStateDesc::FillMode::Wireframe:
            return MTL::TriangleFillModeLines;
        case RasterStateDesc::FillMode::Solid:
            return MTL::TriangleFillModeFill;
        }
    }

    MTL::CullMode GetCullMode(RasterStateDesc::CullMode _mode)
    {
        switch (_mode)
        {
        case RasterStateDesc::CullMode::None:
            return MTL::CullModeNone;
        case RasterStateDesc::CullMode::Front:
            return MTL::CullModeFront;
        case RasterStateDesc::CullMode::Back:
            return MTL::CullModeBack;
        }
    }

    MTL::Winding GetWinding(RasterStateDesc::Front _mode)
    {
        switch (_mode)
        {
        case RasterStateDesc::Front::Clockwise:
            return MTL::WindingClockwise;
        case RasterStateDesc::Front::CounterClockwise:
            return MTL::WindingCounterClockwise;
        }
    }

    MTL::PrimitiveType GetPrimitiveType(InputAssemblyDesc::PrimitiveTopology _topology)
    {
        switch (_topology)
        {
        case InputAssemblyDesc::PrimitiveTopology::PointList:
            return MTL::PrimitiveTypePoint;
        case InputAssemblyDesc::PrimitiveTopology::LineList:
            return MTL::PrimitiveTypeLine;
        case InputAssemblyDesc::PrimitiveTopology::LineStrip:
            return MTL::PrimitiveTypeLineStrip;
        case InputAssemblyDesc::PrimitiveTopology::TriangleList:
            return MTL::PrimitiveTypeTriangle;
        case InputAssemblyDesc::PrimitiveTopology::TriangleStrip:
            return MTL::PrimitiveTypeTriangleStrip;
        }
    }

    MTL::Stages GetMetalStages(const BarrierSyncStageFlags _stageFlags)
    {
        if (_stageFlags == BarrierSyncStageFlags::None)
            return 0;

        if (BitUtils::EnumHasAny(_stageFlags, BarrierSyncStageFlags::All))
            return MTL::StageAll;

        MTL::Stages stages = 0;

        constexpr BarrierSyncStageFlags vertexFlags = BarrierSyncStageFlags::ExecuteIndirect
            | BarrierSyncStageFlags::IndexInputAssembly
            | BarrierSyncStageFlags::VertexInputAssembly
            | BarrierSyncStageFlags::VertexShading
            | BarrierSyncStageFlags::AllShading;
        if (BitUtils::EnumHasAny(_stageFlags, vertexFlags))
        {
            stages |= MTL::StageVertex | MTL::StageMesh | MTL::StageObject;
        }

        constexpr BarrierSyncStageFlags fragmentFlags = BarrierSyncStageFlags::FragmentShading
            | BarrierSyncStageFlags::ColorBlending
            | BarrierSyncStageFlags::DepthStencilTesting
            | BarrierSyncStageFlags::MultiSampleResolve
            | BarrierSyncStageFlags::AllShading;
        if (BitUtils::EnumHasAny(_stageFlags, fragmentFlags))
        {
            stages |= MTL::StageFragment;
        }

        constexpr BarrierSyncStageFlags tileFlags = BarrierSyncStageFlags::MultiSampleResolve
            | BarrierSyncStageFlags::AllShading;
        if (BitUtils::EnumHasAny(_stageFlags, tileFlags))
        {
            stages |= MTL::StageTile;
        }

        constexpr BarrierSyncStageFlags computeFlags = BarrierSyncStageFlags::ComputeShading
            | BarrierSyncStageFlags::ExecuteIndirect
            | BarrierSyncStageFlags::Raytracing // Metal dispatches raytracing commands as compute workloads
            | BarrierSyncStageFlags::AllShading;
        if (BitUtils::EnumHasAny(_stageFlags, computeFlags))
        {
            stages |= MTL::StageDispatch;
        }

        constexpr BarrierSyncStageFlags blit = BarrierSyncStageFlags::Transfer;
        if (BitUtils::EnumHasAny(_stageFlags, blit))
        {
            stages |= MTL::StageBlit;
        }

        constexpr BarrierSyncStageFlags accelerationStructureFlags = BarrierSyncStageFlags::AccelerationStructureBuild
            | BarrierSyncStageFlags::AccelerationStructureCopy;
        if (BitUtils::EnumHasAny(_stageFlags, accelerationStructureFlags))
        {
            stages |= MTL::StageAccelerationStructure;
        }

        return stages;
    }
} // namespace KryneEngine::MetalConverters