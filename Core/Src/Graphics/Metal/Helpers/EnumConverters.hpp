/**
 * @file
 * @author Max Godefroy
 * @date 02/11/2024.
 */

#pragma once

#include "Graphics/Metal/MetalHeaders.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/RenderPass.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

namespace KryneEngine::MetalConverters
{
    [[nodiscard]] size_t GetPixelByteSize(TextureFormat _format);
    [[nodiscard]] MTL::PixelFormat ToPixelFormat(TextureFormat _format);
    [[nodiscard]] TextureFormat FromPixelFormat(MTL::PixelFormat _format);

    [[nodiscard]] MTL::SamplerMinMagFilter GetMinMagFilter(SamplerDesc::Filter _filter);
    [[nodiscard]] MTL::SamplerMipFilter GetMipFilter(SamplerDesc::Filter _filter);
    [[nodiscard]] MTL::SamplerAddressMode GetAddressMode(SamplerDesc::AddressMode _mode);

    [[nodiscard]] MTL::ResourceOptions GetResourceStorage(MemoryUsage _memoryUsage);
    [[nodiscard]] MTL::StorageMode GetStorageMode(MemoryUsage _memoryUsage);
    [[nodiscard]] MTL::TextureUsage GetTextureUsage(MemoryUsage _usage);

    [[nodiscard]] MTL::TextureSwizzle GetSwizzle(TextureComponentMapping _mapping);

    [[nodiscard]] MTL::TextureType GetTextureType(TextureTypes _type);

    [[nodiscard]] MTL::DataType GetDataType(DescriptorBindingDesc::Type _type);
    [[nodiscard]] MTL::BindingAccess GetBindingAccess(DescriptorBindingDesc::Type _type);

    [[nodiscard]] MTL::VertexFormat GetVertexFormat(TextureFormat _format);

    [[nodiscard]] MTL::BlendOperation GetBlendOperation(ColorAttachmentBlendDesc::BlendOp _op);
    [[nodiscard]] MTL::BlendFactor GetBlendFactor(ColorAttachmentBlendDesc::BlendFactor _factor);
    [[nodiscard]] MTL::ColorWriteMask GetColorWriteMask(ColorAttachmentBlendDesc::WriteMask _mask);

    [[nodiscard]] MTL::CompareFunction GetCompareOperation(DepthStencilStateDesc::CompareOp _op);
    [[nodiscard]] MTL::StencilOperation GetStencilOperation(DepthStencilStateDesc::StencilOp _op);

    [[nodiscard]] MTL::LoadAction GetMetalLoadOperation(RenderPassDesc::Attachment::LoadOperation _op);
    [[nodiscard]] MTL::StoreAction GetMetalStoreOperation(RenderPassDesc::Attachment::StoreOperation _op);

    [[nodiscard]] MTL::TriangleFillMode GetTriangleFillMode(RasterStateDesc::FillMode _mode);
    [[nodiscard]] MTL::CullMode GetCullMode(RasterStateDesc::CullMode _mode);
    [[nodiscard]] MTL::Winding GetWinding(RasterStateDesc::Front _mode);
    [[nodiscard]] MTL::PrimitiveType GetPrimitiveType(InputAssemblyDesc::PrimitiveTopology _topology);

    [[nodiscard]] MTL::Stages GetMetalStages(BarrierSyncStageFlags _stageFlags);
}