/**
 * @file
 * @author Max Godefroy
 * @date 04/08/2024.
 */

#pragma once

#include "EASTL/fixed_vector.h"
#include "EASTL/span.h"

#include "Enums.hpp"
#include "Handles.hpp"
#include "KryneEngine/Core/Common/BitUtils.hpp"
#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"
#include "RenderPass.hpp"

namespace KryneEngine
{
    struct ShaderStage
    {
        enum class Stage : u8
        {
            Vertex,
            TesselationControl,
            TesselationEvaluation,
            Geometry,
            Fragment,
            Compute,
            Mesh,
            Task,
        };

        ShaderModuleHandle m_shaderModule { GenPool::kInvalidHandle };
        Stage m_stage = Stage::Vertex;
        eastl::string m_entryPoint = "main";
    };

    struct VertexLayoutElement
    {
        enum class SemanticName : u8
        {
            Position,
            Normal,
            Uv,
            Color,
            Tangent,
            BiTangent,
            BoneIndices,
            BoneWeights,
        };

        SemanticName m_semanticName = SemanticName::Position;
        u8 m_semanticIndex : 4 = 0;
        u8 m_bindingIndex : 4 = 0;
        TextureFormat m_format = TextureFormat::RGBA8_UNorm;
        u16 m_offset = 0;
        u8 m_location = 0;
    };

    enum class VertexInputRate: u8
    {
        Vertex,
        Instance,
    };

    struct VertexBindingDesc
    {
        u16 m_stride = 0;
        u8 m_binding = 0;
        VertexInputRate m_inputRate = VertexInputRate::Vertex;
    };

    struct VertexInputDesc
    {
        eastl::span<const VertexLayoutElement> m_elements{};
        eastl::span<const VertexBindingDesc> m_bindings {};
    };

    struct InputAssemblyDesc
    {
        enum class PrimitiveTopology: u8
        {
            PointList,
            LineList,
            LineStrip,
            TriangleList,
            TriangleStrip,
        };

        enum class IndexIntSize: u8
        {
            U16,
            U32,
        };

        PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
        IndexIntSize m_indexSize = IndexIntSize::U32;
        bool m_cutStripAtSpecialIndex = false;
    };

    struct RasterStateDesc
    {
        enum class FillMode: u8
        {
            Wireframe,
            Solid
        };

        enum class CullMode: u8
        {
            None,
            Front,
            Back,
        };

        enum class Front: u8
        {
            Clockwise,
            CounterClockwise,
        };

        FillMode m_fillMode = FillMode::Solid;
        CullMode m_cullMode = CullMode::Back;
        Front m_front = Front::CounterClockwise;
        bool m_depthClip = true;
        bool m_depthBias = false;
        float m_depthBiasConstantFactor = 0;
        float m_depthBiasSlopeFactor = 0;
        float m_depthBiasClampValue = 0;
    };

    struct ColorAttachmentBlendDesc
    {
        enum class BlendFactor: u8
        {
            Zero,
            One,
            SrcColor,
            InvSrcColor,
            SrcAlpha,
            InvSrcAlpha,
            DstColor,
            InvDstColor,
            DstAlpha,
            InvDstAlpha,
            SrcAlphaSaturate,
            FactorColor,
            InvFactorColor,
            FactorAlpha,
            InvFactorAlpha,
            Src1Color,
            InvSrc1Color,
            Src1Alpha,
            InvSrc1Alpha,
        };

        enum class BlendOp: u8
        {
            Add,
            Subtract,
            ReverseSubtract,
            Min,
            Max,
        };

        enum class WriteMask: u8
        {
            Red     = 1 << 0,
            Green   = 1 << 1,
            Blue    = 1 << 2,
            Alpha   = 1 << 3,
            All     = Red | Green | Blue | Alpha,
        };

        bool m_blendEnable = false;
        BlendFactor m_srcColor = BlendFactor::One;
        BlendFactor m_dstColor = BlendFactor::Zero;
        BlendOp m_colorOp = BlendOp::Add;
        BlendFactor m_srcAlpha = BlendFactor::One;
        BlendFactor m_dstAlpha = BlendFactor::Zero;
        BlendOp m_alphaOp = BlendOp::Add;
        WriteMask m_writeMask = WriteMask::All;
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(ColorAttachmentBlendDesc::WriteMask)

    static constexpr ColorAttachmentBlendDesc kDefaultColorAttachmentOpaqueBlendDesc {};
    static constexpr ColorAttachmentBlendDesc kDefaultColorAttachmentAlphaBlendDesc {
        .m_blendEnable = true,
        .m_srcColor = ColorAttachmentBlendDesc::BlendFactor::SrcAlpha,
        .m_dstColor = ColorAttachmentBlendDesc::BlendFactor::InvSrcAlpha,
        .m_colorOp = ColorAttachmentBlendDesc::BlendOp::Add,
        .m_srcAlpha = ColorAttachmentBlendDesc::BlendFactor::One,
        .m_dstAlpha = ColorAttachmentBlendDesc::BlendFactor::InvSrcAlpha,
        .m_alphaOp = ColorAttachmentBlendDesc::BlendOp::Add,
    };

    struct ColorBlendingDesc
    {
        enum class LogicOp
        {
            None = 0,
            Clear,
            Set,
            Copy,
            CopyInverted,
            NoOp,
            Invert,
            And,
            NAnd,
            Or,
            NOr,
            XOr,
            Equiv,
            AndReverse,
            AndInverted,
            OrReverse,
            OrInverted,
        };

        eastl::fixed_vector<ColorAttachmentBlendDesc, 8, false> m_attachments {};
        float4 m_blendFactor = float4(0);
        LogicOp m_logicOp = LogicOp::None;
        bool m_dynamicBlendFactor = false;
    };

    struct DepthStencilStateDesc
    {
        enum class CompareOp : u8
        {
            Never,
            Less,
            Equal,
            LessEqual,
            Greater,
            NotEqual,
            GreaterEqual,
            Always,
        };

        enum class StencilOp : u8
        {
            Keep,
            Zero,
            Replace,
            IncrementAndClamp,
            DecrementAndClamp,
            Invert,
            IncrementAndWrap,
            DecrementAndWrap,
        };

        struct StencilOpState
        {
            StencilOp m_passOp = StencilOp::Keep;
            StencilOp m_failOp = StencilOp::Keep;
            StencilOp m_depthFailOp = StencilOp::Keep;
            CompareOp m_compareOp = CompareOp::Never;
        };

        bool m_depthTest = true;
        bool m_depthWrite = true;
        CompareOp m_depthCompare = CompareOp::Less;
        bool m_stencilTest = false;

        u8 m_stencilReadMask = 0xFF;
        u8 m_stencilWriteMask = 0xFF;
        u8 m_stencilRef = 0xFF;
        bool m_dynamicStencilRef = false;

        StencilOpState m_front {};
        StencilOpState m_back {};
    };

    enum class ShaderVisibility: u8
    {
        Vertex = 1 << 0,
        TesselationControl = 1 << 1,
        TesselationEvaluation = 1 << 2,
        Geometry = 1 << 3,
        Fragment = 1 << 4,
        Compute = 1 << 5,
        Task = 1 << 6,
        Mesh = 1 << 7,

        All = 0xFF,
        None = 0,
    };
    KE_ENUM_IMPLEMENT_BITWISE_OPERATORS(ShaderVisibility)

    struct PushConstantDesc
    {
        u8 m_sizeInBytes = 0;
        u8 m_offset = 0;
        u8 m_index = 0;
        ShaderVisibility m_visibility = ShaderVisibility::All;
    };

    struct DescriptorBindingDesc
    {
        enum class Type: u8
        {
            Sampler,
            SampledTexture,
            StorageReadOnlyTexture,
            StorageReadWriteTexture,
            ConstantBuffer,
            StorageReadOnlyBuffer,
            StorageReadWriteBuffer,
        };

        static constexpr u16 kImplicitBindingIndex = ~0;

        Type m_type = Type::SampledTexture;
        ShaderVisibility m_visibility = ShaderVisibility::Fragment;
        u16 m_count = 1;
        u16 m_bindingIndex = kImplicitBindingIndex; //< Specify explicit binding index. Leave to default value for implicit index.
        TextureTypes m_textureType = TextureTypes::Single2D;
    };

    struct DescriptorSetDesc
    {
        // Note: Array is expected to be sorted by binding indices between descriptors of the same group type
        eastl::span<const DescriptorBindingDesc> m_bindings{};
    };

    struct DescriptorSetWriteInfo
    {
        u32 m_index = 0;
        u16 m_arrayOffset = 0;

        struct DescriptorData
        {
            TextureLayout m_textureLayout = TextureLayout::Unknown;
            GenPool::Handle m_handle = GenPool::kInvalidHandle;
        };
        eastl::span<const DescriptorData> m_descriptorData;
    };

    struct PipelineLayoutDesc
    {
        eastl::span<const DescriptorSetLayoutHandle> m_descriptorSets {};
        eastl::span<const PushConstantDesc> m_pushConstants {};
        bool m_useVertexLayout = true;
    };

    struct RenderTargetSetDesc
    {
        u8 m_numColorAttachments = 0;
        MultisamplingSampleCount m_sampleCount = MultisamplingSampleCount::One;
        TextureFormat m_colorFormats[RenderPassDesc::kMaxSupportedColorAttachments] {};
        TextureFormat m_depthStencilFormat = TextureFormat::NoFormat;
    };
    // Ensure that the struct is tightly packed, for consistent hashing results
    static_assert(sizeof(RenderTargetSetDesc) == offsetof(RenderTargetSetDesc, m_depthStencilFormat) + sizeof(RenderTargetSetDesc::m_depthStencilFormat));

    struct GraphicsPipelineDesc
    {
        eastl::span<const ShaderStage> m_stages {};
        VertexInputDesc m_vertexInput {};
        InputAssemblyDesc m_inputAssembly {};
        RasterStateDesc m_rasterState {};
        ColorBlendingDesc m_colorBlending {};
        DepthStencilStateDesc m_depthStencil {};
        RenderTargetSetDesc m_renderTargets {};
        PipelineLayoutHandle m_pipelineLayout { GenPool::kInvalidHandle };

#if !defined(KE_FINAL)
        eastl::string m_debugName = "";
#endif
    };

    struct ComputePipelineDesc
    {
        ShaderStage m_computeStage {};
        PipelineLayoutHandle m_pipelineLayout { GenPool::kInvalidHandle };

#if !defined(KE_FINAL)
        eastl::string m_debugName = "";
#endif
    };
}
