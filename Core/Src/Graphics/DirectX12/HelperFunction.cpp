/**
 * @file
 * @author Max Godefroy
 * @date 31/01/2024.
 */

#include <iostream>

#include "Graphics/DirectX12/HelperFunctions.hpp"

namespace KryneEngine
{
    D3D12_TEXTURE_ADDRESS_MODE Dx12Converters::ToDx12AddressMode(SamplerDesc::AddressMode _addressMode)
    {
        switch (_addressMode)
        {
        case SamplerDesc::AddressMode::Repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerDesc::AddressMode::MirroredRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerDesc::AddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerDesc::AddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        }
    }

    D3D12_BARRIER_SYNC Dx12Converters::ToDx12BarrierSync(BarrierSyncStageFlags _flags)
    {
        D3D12_BARRIER_SYNC flags = D3D12_BARRIER_SYNC_NONE;
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::None))
        {
            return D3D12_BARRIER_SYNC_NONE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::All))
        {
            flags |= D3D12_BARRIER_SYNC_ALL;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ExecuteIndirect))
        {
            flags |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::IndexInputAssembly))
        {
            flags |= D3D12_BARRIER_SYNC_INDEX_INPUT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::VertexInputAssembly | BarrierSyncStageFlags::VertexShading))
        {
            flags |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::FragmentShading))
        {
            flags |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ColorBlending))
        {
            flags |= D3D12_BARRIER_SYNC_RENDER_TARGET;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::DepthStencilTesting))
        {
            flags |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Transfer))
        {
            flags |= D3D12_BARRIER_SYNC_COPY;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::MultiSampleResolve))
        {
            flags |= D3D12_BARRIER_SYNC_RESOLVE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ComputeShading))
        {
            flags |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AllShading))
        {
            flags |= D3D12_BARRIER_SYNC_ALL_SHADING;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Raytracing))
        {
            flags |= D3D12_BARRIER_SYNC_RAYTRACING;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureBuild))
        {
            flags |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureCopy))
        {
            flags |= D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE;
        }

        return flags;
    }

    D3D12_BARRIER_ACCESS Dx12Converters::ToDx12BarrierAccess(BarrierAccessFlags _flags)
    {
        D3D12_BARRIER_ACCESS flags = D3D12_BARRIER_ACCESS_COMMON;

        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::None))
        {
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::VertexBuffer))
        {
            flags |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndexBuffer))
        {
            flags |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ConstantBuffer))
        {
            flags |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndirectBuffer))
        {
            flags |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ColorAttachment))
        {
            flags |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilRead))
        {
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilWrite))
        {
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShaderResource))
        {
            flags |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::UnorderedAccess))
        {
            flags |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveSrc))
        {
            flags |= D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveDst))
        {
            flags |= D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferSrc))
        {
            flags |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferDst))
        {
            flags |= D3D12_BARRIER_ACCESS_COPY_DEST;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureRead))
        {
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureWrite))
        {
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShadingRate))
        {
            flags |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllRead))
        {
            return D3D12_BARRIER_ACCESS_COMMON;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllWrite))
        {
            return D3D12_BARRIER_ACCESS_COMMON;
        }

        return flags;
    }

    D3D12_BARRIER_LAYOUT Dx12Converters::ToDx12BarrierLayout(TextureLayout _layout)
    {
        switch (_layout)
        {
        case TextureLayout::Unknown:
            return D3D12_BARRIER_LAYOUT_UNDEFINED;
        case TextureLayout::Common:
            return D3D12_BARRIER_LAYOUT_COMMON;
        case TextureLayout::Present:
            return D3D12_BARRIER_LAYOUT_PRESENT;
        case TextureLayout::GenericRead:
            return D3D12_BARRIER_LAYOUT_GENERIC_READ;
        case TextureLayout::ColorAttachment:
            return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        case TextureLayout::DepthStencilAttachment:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        case TextureLayout::DepthStencilReadOnly:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        case TextureLayout::UnorderedAccess:
            return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        case TextureLayout::ShaderResource:
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        case TextureLayout::TransferSrc:
            return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        case TextureLayout::TransferDst:
            return D3D12_BARRIER_LAYOUT_COPY_DEST;
        case TextureLayout::ResolveSrc:
            return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        case TextureLayout::ResolveDst:
            return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        case TextureLayout::ShadingRate:
            return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
        }
    }

    D3D12_RESOURCE_STATES Dx12Converters::RetrieveState(BarrierAccessFlags _access, TextureLayout _layout)
    {
        D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
        using namespace BitUtils;

        D3D12_BARRIER_ACCESS access = Dx12Converters::ToDx12BarrierAccess(_access);

        if (access == D3D12_BARRIER_ACCESS_COMMON)
        {
            return D3D12_RESOURCE_STATE_COMMON;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_CONSTANT_BUFFER))
        {
            states |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_INDEX_BUFFER))
        {
            states |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }
        if (_layout == TextureLayout::ColorAttachment)
        {
            states |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) || _layout == TextureLayout::UnorderedAccess)
        {
            states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        if (_layout == TextureLayout::DepthStencilAttachment)
        {
            states |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        if (_layout == TextureLayout::DepthStencilReadOnly)
        {
            states |= D3D12_RESOURCE_STATE_DEPTH_READ;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_SHADER_RESOURCE) || _layout == TextureLayout::ShaderResource)
        {
            states |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT))
        {
            states |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_COPY_SOURCE) || _layout == TextureLayout::TransferSrc)
        {
            states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_COPY_DEST) || _layout == TextureLayout::TransferDst)
        {
            states |= D3D12_RESOURCE_STATE_COPY_DEST;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_RESOLVE_SOURCE) || _layout == TextureLayout::ResolveSrc)
        {
            states |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_RESOLVE_DEST) || _layout == TextureLayout::ResolveDst)
        {
            states |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE))
        {
            states |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        }
        if (EnumHasAny(access, D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE))
        {
            states |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
        }

        return states;
    }

    D3D12_SHADER_VISIBILITY Dx12Converters::ToDx12ShaderVisibility(ShaderVisibility _visibility)
    {
        if (std::popcount((u8)_visibility) > 1)
        {
            return D3D12_SHADER_VISIBILITY_ALL;
        }
        switch (_visibility)
        {
        case ShaderVisibility::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case ShaderVisibility::TesselationControl:
            return D3D12_SHADER_VISIBILITY_HULL;
        case ShaderVisibility::TesselationEvaluation:
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case ShaderVisibility::Geometry:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case ShaderVisibility::Fragment:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case ShaderVisibility::Task:
            return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        case ShaderVisibility::Mesh:
            return D3D12_SHADER_VISIBILITY_MESH;
        case ShaderVisibility::Compute:
            return D3D12_SHADER_VISIBILITY_ALL;

        default:
            KE_ERROR("Unsupported visibility");
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_BLEND Dx12Converters::ToDx12Blend(ColorAttachmentBlendDesc::BlendFactor _blendFactor)
    {
        switch (_blendFactor)
        {
        case ColorAttachmentBlendDesc::BlendFactor::Zero:
            return D3D12_BLEND_ZERO;
        case ColorAttachmentBlendDesc::BlendFactor::One:
            return D3D12_BLEND_ONE;
        case ColorAttachmentBlendDesc::BlendFactor::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlphaSaturate:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case ColorAttachmentBlendDesc::BlendFactor::FactorColor:
            return D3D12_BLEND_BLEND_FACTOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorColor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case ColorAttachmentBlendDesc::BlendFactor::FactorAlpha:
            return D3D12_BLEND_ALPHA_FACTOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorAlpha:
            return D3D12_BLEND_INV_ALPHA_FACTOR;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        }
    }

    D3D12_BLEND_OP Dx12Converters::ToDx12BlendOp(ColorAttachmentBlendDesc::BlendOp _blendOp)
    {
        switch (_blendOp)
        {
        case ColorAttachmentBlendDesc::BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case ColorAttachmentBlendDesc::BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case ColorAttachmentBlendDesc::BlendOp::ReverseSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case ColorAttachmentBlendDesc::BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case ColorAttachmentBlendDesc::BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        }
    }

    D3D12_LOGIC_OP Dx12Converters::ToDx12LogicOp(ColorBlendingDesc::LogicOp _logicOp)
    {
        switch (_logicOp)
        {
        case ColorBlendingDesc::LogicOp::Clear:
            return D3D12_LOGIC_OP_CLEAR;
        case ColorBlendingDesc::LogicOp::Set:
            return D3D12_LOGIC_OP_SET;
        case ColorBlendingDesc::LogicOp::Copy:
            return D3D12_LOGIC_OP_COPY;
        case ColorBlendingDesc::LogicOp::CopyInverted:
            return D3D12_LOGIC_OP_COPY_INVERTED;
        case ColorBlendingDesc::LogicOp::None:
        case ColorBlendingDesc::LogicOp::NoOp:
            return D3D12_LOGIC_OP_NOOP;
        case ColorBlendingDesc::LogicOp::Invert:
            return D3D12_LOGIC_OP_INVERT;
        case ColorBlendingDesc::LogicOp::And:
            return D3D12_LOGIC_OP_AND;
        case ColorBlendingDesc::LogicOp::NAnd:
            return D3D12_LOGIC_OP_NAND;
        case ColorBlendingDesc::LogicOp::Or:
            return D3D12_LOGIC_OP_OR;
        case ColorBlendingDesc::LogicOp::NOr:
            return D3D12_LOGIC_OP_NOR;
        case ColorBlendingDesc::LogicOp::XOr:
            return D3D12_LOGIC_OP_XOR;
        case ColorBlendingDesc::LogicOp::Equiv:
            return D3D12_LOGIC_OP_EQUIV;
        case ColorBlendingDesc::LogicOp::AndReverse:
            return D3D12_LOGIC_OP_AND_REVERSE;
        case ColorBlendingDesc::LogicOp::AndInverted:
            return D3D12_LOGIC_OP_AND_INVERTED;
        case ColorBlendingDesc::LogicOp::OrReverse:
            return D3D12_LOGIC_OP_OR_REVERSE;
        case ColorBlendingDesc::LogicOp::OrInverted:
            return D3D12_LOGIC_OP_OR_INVERTED;
        }
    }

    D3D12_COMPARISON_FUNC Dx12Converters::ToDx12CompareFunc(DepthStencilStateDesc::CompareOp _compareOp)
    {
        switch (_compareOp)
        {
        case DepthStencilStateDesc::CompareOp::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case DepthStencilStateDesc::CompareOp::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case DepthStencilStateDesc::CompareOp::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case DepthStencilStateDesc::CompareOp::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case DepthStencilStateDesc::CompareOp::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case DepthStencilStateDesc::CompareOp::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case DepthStencilStateDesc::CompareOp::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case DepthStencilStateDesc::CompareOp::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        }
    }

    D3D12_STENCIL_OP Dx12Converters::ToDx12StencilOp(DepthStencilStateDesc::StencilOp _stencilOp)
    {
        switch (_stencilOp)
        {
        case DepthStencilStateDesc::StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case DepthStencilStateDesc::StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case DepthStencilStateDesc::StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case DepthStencilStateDesc::StencilOp::IncrementAndClamp:
            return D3D12_STENCIL_OP_INCR_SAT;
        case DepthStencilStateDesc::StencilOp::DecrementAndClamp:
            return D3D12_STENCIL_OP_DECR_SAT;
        case DepthStencilStateDesc::StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case DepthStencilStateDesc::StencilOp::IncrementAndWrap:
            return D3D12_STENCIL_OP_INCR;
        case DepthStencilStateDesc::StencilOp::DecrementAndWrap:
            return D3D12_STENCIL_OP_DECR;
        }
    }

    const char* Dx12Converters::ToDx12SemanticName(VertexLayoutElement::SemanticName _semanticName)
    {
        switch (_semanticName)
        {
        case VertexLayoutElement::SemanticName::Position:
            return "POSITION";
        case VertexLayoutElement::SemanticName::Normal:
            return "NORMAL";
        case VertexLayoutElement::SemanticName::Uv:
            return "TEXCOORD";
        case VertexLayoutElement::SemanticName::Color:
            return "COLOR";
        case VertexLayoutElement::SemanticName::Tangent:
            return "BITANGENT";
        case VertexLayoutElement::SemanticName::BiTangent:
            return "BINORMAL";
        case VertexLayoutElement::SemanticName::BoneIndices:
            return "BLENDINDICES";
        case VertexLayoutElement::SemanticName::BoneWeights:
            return "BLENDWEIGHTS";
        }
    }

    D3D_PRIMITIVE_TOPOLOGY Dx12Converters::ToDx12Topology(InputAssemblyDesc::PrimitiveTopology _topology)
    {
        switch(_topology)
        {
        case InputAssemblyDesc::PrimitiveTopology::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case InputAssemblyDesc::PrimitiveTopology::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case InputAssemblyDesc::PrimitiveTopology::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case InputAssemblyDesc::PrimitiveTopology::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case InputAssemblyDesc::PrimitiveTopology::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        }
    }

    u8 GetTextureBytesPerPixel(DXGI_FORMAT _format)
    {
        switch(_format)
        {
        default:
        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_V208:
        case DXGI_FORMAT_V408:
            KE_ERROR("Format not supported yet");
            return 0;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 16;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 12;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 8;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return 4;
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 2;
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;
        }
    }

    void DebugLayerMessageCallback(
            D3D12_MESSAGE_CATEGORY _category,
            D3D12_MESSAGE_SEVERITY _severity,
            D3D12_MESSAGE_ID _id,
            LPCSTR _description,
            void* _context)
    {
        constexpr auto kMinimumSeverity = D3D12_MESSAGE_SEVERITY_WARNING;
        constexpr auto kMinimumAssertSeverity = D3D12_MESSAGE_SEVERITY_ERROR;

        if (_severity > kMinimumSeverity)
        {
            return;
        }

        eastl::string severityString;
        switch (_severity)
        {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                severityString = "corruption";
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                severityString = "error";
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                severityString = "warning";
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                severityString = "info";
                break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:
                severityString = "message";
                break;
        }

        std::cout << "Validation layer (" << severityString.c_str() << "): " << _description << std::endl;

        KE_ASSERT(_severity > kMinimumAssertSeverity);
    }
}
