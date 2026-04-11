/**
 * @file
 * @author Max Godefroy
 * @date 01/08/2024.
 */

#include "Graphics/Vulkan/HelperFunctions.hpp"

namespace KryneEngine::VkHelperFunctions
{
    u32 RetrieveBufferUsage(MemoryUsage _usage)
    {
        u32 flags = 0;
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::TransferSrcBuffer))
        {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::TransferDstBuffer))
        {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::ConstantBuffer))
        {
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::ReadWriteBuffer))
        {
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::IndexBuffer))
        {
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::VertexBuffer))
        {
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::IndirectBuffer))
        {
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }
        if (BitUtils::EnumHasAny(_usage, MemoryUsage::AccelerationStruct))
        {
            flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                     | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }
        return flags;
    }

    VkPipelineStageFlagBits2 ToVkPipelineStageFlagBits2(BarrierSyncStageFlags _flags, bool _isSrc)
    {
        VkPipelineStageFlagBits2 flags = VK_PIPELINE_STAGE_2_NONE;

        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::None))
        {
            return _isSrc ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::All))
        {
            flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ExecuteIndirect))
        {
            flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT; // Works for all indirect commands, not just draw
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::IndexInputAssembly | BarrierSyncStageFlags::VertexInputAssembly))
        {
            flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::VertexShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
                     | VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT
                     | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT
                     | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT
                     | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
                     | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::FragmentShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ColorBlending))
        {
            flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::DepthStencilTesting))
        {
            flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Transfer))
        {
            flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::MultiSampleResolve))
        {
            flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ComputeShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Raytracing | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureBuild))
        {
            flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureCopy))
        {
            flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
        }

        return static_cast<VkPipelineStageFlagBits>(flags);
    }

    VkPipelineStageFlags ToVkPipelineStageFlagBits(BarrierSyncStageFlags _flags, bool _isSrc)
    {
        int flags = VK_PIPELINE_STAGE_NONE;

        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::None))
        {
            return _isSrc ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::All))
        {
            flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ExecuteIndirect))
        {
            flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT; // Works for all indirect commands, not just draw
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::IndexInputAssembly | BarrierSyncStageFlags::VertexInputAssembly))
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::VertexShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                   | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                   | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                   | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                   | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT
                   | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::FragmentShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ColorBlending))
        {
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::DepthStencilTesting))
        {
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Transfer))
        {
            flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::MultiSampleResolve))
        {
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::ComputeShading | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::Raytracing | BarrierSyncStageFlags::AllShading))
        {
            flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureBuild))
        {
            flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierSyncStageFlags::AccelerationStructureCopy))
        {
            flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // No specific flag for AS copy in this enum, using most conservative barrier.
        }

        return static_cast<VkPipelineStageFlags>(flags);
    }

    VkAccessFlags2 ToVkAccessFlags2(BarrierAccessFlags _flags)
    {
        VkAccessFlags2 flags = VK_ACCESS_2_NONE;

        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::None))
        {
            return VK_ACCESS_2_NONE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::VertexBuffer))
        {
            flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndexBuffer))
        {
            flags |= VK_ACCESS_2_INDEX_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ConstantBuffer))
        {
            flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndirectBuffer))
        {
            flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ColorAttachment))
        {
            flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilWrite))
        {
            flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilRead))
        {
            flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShaderResource))
        {
            flags |= VK_ACCESS_2_SHADER_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::UnorderedAccess))
        {
            flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveSrc))
        {
            flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveDst))
        {
            flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferSrc))
        {
            flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferDst))
        {
            flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureRead))
        {
            flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureWrite))
        {
            flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShadingRate))
        {
            flags |= VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllRead))
        {
            flags |= VK_ACCESS_2_MEMORY_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllWrite))
        {
            flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
        }

        return flags;
    }

    VkAccessFlags ToVkAccessFlags(BarrierAccessFlags _flags)
    {
        int flags = VK_ACCESS_NONE;

        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::None))
        {
            return VK_ACCESS_NONE;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::VertexBuffer))
        {
            flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndexBuffer))
        {
            flags |= VK_ACCESS_INDEX_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ConstantBuffer))
        {
            flags |= VK_ACCESS_UNIFORM_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::IndirectBuffer))
        {
            flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ColorAttachment))
        {
            flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilWrite))
        {
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::DepthStencilRead))
        {
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShaderResource))
        {
            flags |= VK_ACCESS_SHADER_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::UnorderedAccess))
        {
            flags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveSrc))
        {
            flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ResolveDst))
        {
            flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferSrc))
        {
            flags |= VK_ACCESS_TRANSFER_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::TransferDst))
        {
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureRead))
        {
            flags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AccelerationStructureWrite))
        {
            flags |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::ShadingRate))
        {
            flags |= VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllRead))
        {
            flags |= VK_ACCESS_MEMORY_READ_BIT;
        }
        if (BitUtils::EnumHasAny(_flags, BarrierAccessFlags::AllWrite))
        {
            flags |= VK_ACCESS_MEMORY_WRITE_BIT;
        }

        return flags;
    }

    VkSamplerAddressMode ToVkAddressMode(SamplerDesc::AddressMode _addressMode)
    {
        switch (_addressMode)
        {
        case SamplerDesc::AddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerDesc::AddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerDesc::AddressMode::Border:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SamplerDesc::AddressMode::Clamp:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
    }

    VkDescriptorType ToVkDescriptorType(DescriptorBindingDesc::Type _type)
    {
        switch (_type)
        {
        case DescriptorBindingDesc::Type::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorBindingDesc::Type::SampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
        case DescriptorBindingDesc::Type::StorageReadWriteTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorBindingDesc::Type::ConstantBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
        case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    }

    VkShaderStageFlags ToVkShaderStageFlags(ShaderVisibility _visibility)
    {
        VkShaderStageFlags flags = 0;
        if (_visibility == ShaderVisibility::None)
        {
            return 0;
        }
        else if (_visibility == ShaderVisibility::All)
        {
            return VK_SHADER_STAGE_ALL;
        }

        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Vertex))
        {
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::TesselationControl))
        {
            flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::TesselationEvaluation))
        {
            flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Geometry))
        {
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Fragment))
        {
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Compute))
        {
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Task))
        {
            flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
        }
        if (BitUtils::EnumHasAny(_visibility, ShaderVisibility::Mesh))
        {
            flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
        }
        return flags;
    }

    VkShaderStageFlagBits ToVkShaderStageFlagBits(ShaderStage::Stage _stage)
    {
        switch (_stage)
        {
        case ShaderStage::Stage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Stage::TesselationControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::Stage::TesselationEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Stage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Stage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Stage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Stage::Task:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStage::Stage::Mesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            return static_cast<VkShaderStageFlagBits>(0);
        }
    }

    VkPrimitiveTopology ToVkPrimitiveTopology(InputAssemblyDesc::PrimitiveTopology _topology)
    {
        switch (_topology)
        {
        case InputAssemblyDesc::PrimitiveTopology::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case InputAssemblyDesc::PrimitiveTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case InputAssemblyDesc::PrimitiveTopology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case InputAssemblyDesc::PrimitiveTopology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case InputAssemblyDesc::PrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        }
    }

    VkPolygonMode ToVkPolygonMode(RasterStateDesc::FillMode _fillMode)
    {
        switch (_fillMode)
        {
        case RasterStateDesc::FillMode::Wireframe:
            return VK_POLYGON_MODE_LINE;
        case RasterStateDesc::FillMode::Solid:
            return VK_POLYGON_MODE_FILL;
        }
    }

    VkCullModeFlags ToVkCullModeFlags(RasterStateDesc::CullMode _cullMode)
    {
        switch (_cullMode)
        {
        case RasterStateDesc::CullMode::None:
            return VK_CULL_MODE_NONE;
        case RasterStateDesc::CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case RasterStateDesc::CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        }
    }

    VkFrontFace ToVkFrontFace(RasterStateDesc::Front _face)
    {
        switch (_face)
        {
        case RasterStateDesc::Front::Clockwise:
            return VK_FRONT_FACE_CLOCKWISE;
        case RasterStateDesc::Front::CounterClockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }
    }

    VkCompareOp ToVkCompareOp(DepthStencilStateDesc::CompareOp _compareOp)
    {
        switch (_compareOp)
        {
        case DepthStencilStateDesc::CompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case DepthStencilStateDesc::CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case DepthStencilStateDesc::CompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case DepthStencilStateDesc::CompareOp::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthStencilStateDesc::CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case DepthStencilStateDesc::CompareOp::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case DepthStencilStateDesc::CompareOp::GreaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthStencilStateDesc::CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
        }
    }

    VkStencilOp ToVkStencilOp(DepthStencilStateDesc::StencilOp _stencilOp)
    {
        switch (_stencilOp)
        {
        case DepthStencilStateDesc::StencilOp::Keep:
            return VK_STENCIL_OP_KEEP;
        case DepthStencilStateDesc::StencilOp::Zero:
            return VK_STENCIL_OP_ZERO;
        case DepthStencilStateDesc::StencilOp::Replace:
            return VK_STENCIL_OP_REPLACE;
        case DepthStencilStateDesc::StencilOp::IncrementAndClamp:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case DepthStencilStateDesc::StencilOp::DecrementAndClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case DepthStencilStateDesc::StencilOp::Invert:
            return VK_STENCIL_OP_INVERT;
        case DepthStencilStateDesc::StencilOp::IncrementAndWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case DepthStencilStateDesc::StencilOp::DecrementAndWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }
    }

    VkLogicOp ToVkLogicOp(ColorBlendingDesc::LogicOp _logicOp)
    {
        switch (_logicOp)
        {
        case ColorBlendingDesc::LogicOp::Clear:
            return VK_LOGIC_OP_CLEAR;
        case ColorBlendingDesc::LogicOp::Set:
            return VK_LOGIC_OP_SET;
        case ColorBlendingDesc::LogicOp::Copy:
            return VK_LOGIC_OP_COPY;
        case ColorBlendingDesc::LogicOp::CopyInverted:
            return VK_LOGIC_OP_COPY_INVERTED;
        case ColorBlendingDesc::LogicOp::None:
        case ColorBlendingDesc::LogicOp::NoOp:
            return VK_LOGIC_OP_NO_OP;
        case ColorBlendingDesc::LogicOp::Invert:
            return VK_LOGIC_OP_INVERT;
        case ColorBlendingDesc::LogicOp::And:
            return VK_LOGIC_OP_AND;
        case ColorBlendingDesc::LogicOp::NAnd:
            return VK_LOGIC_OP_NAND;
        case ColorBlendingDesc::LogicOp::Or:
            return VK_LOGIC_OP_OR;
        case ColorBlendingDesc::LogicOp::NOr:
            return VK_LOGIC_OP_NOR;
        case ColorBlendingDesc::LogicOp::XOr:
            return VK_LOGIC_OP_XOR;
        case ColorBlendingDesc::LogicOp::Equiv:
            return VK_LOGIC_OP_EQUIVALENT;
        case ColorBlendingDesc::LogicOp::AndReverse:
            return VK_LOGIC_OP_AND_REVERSE;
        case ColorBlendingDesc::LogicOp::AndInverted:
            return VK_LOGIC_OP_AND_INVERTED;
        case ColorBlendingDesc::LogicOp::OrReverse:
            return VK_LOGIC_OP_OR_REVERSE;
        case ColorBlendingDesc::LogicOp::OrInverted:
            return VK_LOGIC_OP_OR_INVERTED;
        }
    }

    VkBlendFactor ToVkBlendFactor(ColorAttachmentBlendDesc::BlendFactor _blendFactor)
    {
        switch (_blendFactor)
        {
        case ColorAttachmentBlendDesc::BlendFactor::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case ColorAttachmentBlendDesc::BlendFactor::One:
            return VK_BLEND_FACTOR_ONE;
        case ColorAttachmentBlendDesc::BlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::SrcAlphaSaturate:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case ColorAttachmentBlendDesc::BlendFactor::FactorColor:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorColor:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::FactorAlpha:
            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvFactorAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Color:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Color:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case ColorAttachmentBlendDesc::BlendFactor::Src1Alpha:
            return VK_BLEND_FACTOR_SRC1_ALPHA;
        case ColorAttachmentBlendDesc::BlendFactor::InvSrc1Alpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        }
    }

    VkBlendOp ToVkBlendOp(ColorAttachmentBlendDesc::BlendOp _blendOp)
    {
        switch (_blendOp)
        {
        case ColorAttachmentBlendDesc::BlendOp::Add:
            return VK_BLEND_OP_ADD;
        case ColorAttachmentBlendDesc::BlendOp::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case ColorAttachmentBlendDesc::BlendOp::ReverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case ColorAttachmentBlendDesc::BlendOp::Min:
            return VK_BLEND_OP_MIN;
        case ColorAttachmentBlendDesc::BlendOp::Max:
            return VK_BLEND_OP_MAX;
        }
    }

    VkColorComponentFlags ToVkColorComponentFlags(ColorAttachmentBlendDesc::WriteMask _mask)
    {
        VkColorComponentFlags flags = 0;

        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Red))
        {
            flags |= VK_COLOR_COMPONENT_R_BIT;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Green))
        {
            flags |= VK_COLOR_COMPONENT_G_BIT;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Blue))
        {
            flags |= VK_COLOR_COMPONENT_B_BIT;
        }
        if (BitUtils::EnumHasAny(_mask, ColorAttachmentBlendDesc::WriteMask::Alpha))
        {
            flags |= VK_COLOR_COMPONENT_A_BIT;
        }

        return flags;
    }

    u16 GetByteSizePerBlock(VkFormat _format)
    {
        switch (_format)
        {
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            return 32;
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64_SFLOAT:
            return 24;
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64_SFLOAT:
            return 16;
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 12;
        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64_SFLOAT:
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
            return 8;
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:
            return 6;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return 5;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
            return 4;
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_D16_UNORM_S8_UINT:
            return 3;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_R10X6_UNORM_PACK16:
        case VK_FORMAT_R12X4_UNORM_PACK16:
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR:
            return 2;
        case VK_FORMAT_R4G4_UNORM_PACK8:
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_A8_UNORM_KHR:
            return 1;
        case VK_FORMAT_UNDEFINED:
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM:
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_MAX_ENUM:
        default:
            KE_ERROR("Format not supported yet");
            return 0;
        }
    }
}
