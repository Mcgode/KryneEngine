/**
 * @file
 * @author Max Godefroy
 * @date 30/10/2024.
 */

#include "Graphics/Metal/MetalResources.hpp"

#include "Graphics/Metal/Helpers/EnumConverters.hpp"
#include "Graphics/Metal/MetalArgumentBufferManager.hpp"
#include "Graphics/Metal/MetalConstants.hpp"
#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"

namespace KryneEngine
{
    MetalResources::MetalResources(AllocatorInstance _allocator)
        : m_buffers(_allocator)
        , m_textures(_allocator)
        , m_samplers(_allocator)
        , m_textureViews(_allocator)
        , m_bufferViews(_allocator)
        , m_renderTargetViews(_allocator)
        , m_renderPasses(_allocator)
        , m_libraries(_allocator)
        , m_graphicsPso(_allocator)
        , m_computePso(_allocator)
    {}

    MetalResources::~MetalResources() = default;

    void MetalResources::FlushPools()
    {
        m_buffers.FlushDeferredFrees();
        m_textures.FlushDeferredFrees();
        m_samplers.FlushDeferredFrees();
        m_textureViews.FlushDeferredFrees();
        m_bufferViews.FlushDeferredFrees();
        m_renderTargetViews.FlushDeferredFrees();
        m_renderPasses.FlushDeferredFrees();
        m_libraries.FlushDeferredFrees();
        m_graphicsPso.FlushDeferredFrees();
        m_computePso.FlushDeferredFrees();
    }

    AllocatorInstance MetalResources::GetAllocator() const
    {
        return m_textures.GetAllocator();
    }

    BufferHandle MetalResources::CreateBuffer(MTL::Device& _device, const BufferCreateDesc& _desc)
    {
        VERIFY_OR_RETURN(_desc.m_desc.m_size > 0, { GenPool::kInvalidHandle });
        VERIFY_OR_RETURN(BitUtils::EnumHasAny(_desc.m_usage, ~MemoryUsage::USAGE_TYPE_MASK), { GenPool::kInvalidHandle });

        const GenPool::Handle handle = m_buffers.Allocate();

        auto [bufferHot, bufferCold] = m_buffers.GetAll(handle);
        const MTL::ResourceOptions options = MetalConverters::GetResourceStorage(_desc.m_usage);
        bufferHot->m_buffer = _device.newBuffer(_desc.m_desc.m_size, options)->retain();
        KE_ASSERT_FATAL(bufferHot->m_buffer != nullptr);
        bufferCold->m_options = options;

        {
            const auto lock = m_residencySetLock.AutoLock();
            m_residencySet->addAllocation(bufferHot->m_buffer);
        }

#if !defined(KE_FINAL)
        NS::String* label = NS::String::string(_desc.m_desc.m_debugName.c_str(), NS::UTF8StringEncoding);
        bufferHot->m_buffer->setLabel(label);
#endif

        return { handle };
    }

    bool MetalResources::DestroyBuffer(BufferHandle _buffer)
    {
        BufferHotData hotData;
        if (m_buffers.Free(_buffer.m_handle, &hotData))
        {
            {
                const auto lock = m_residencySetLock.AutoLock();
                m_residencySet->removeAllocation(hotData.m_buffer);
            }
            hotData.m_buffer->release();
            return true;
        }
        return false;
    }

    TextureHandle MetalResources::CreateTexture(MTL::Device& _device, const TextureCreateDesc& _desc)
    {
        const GenPool::Handle handle = m_textures.Allocate();
        TextureHotData* hot = m_textures.Get(handle);

        NsPtr desc(MTL::TextureDescriptor::alloc()->init());
        desc->setWidth(_desc.m_desc.m_dimensions.x);
        desc->setHeight(_desc.m_desc.m_dimensions.y);
        desc->setDepth(_desc.m_desc.m_dimensions.z);
        desc->setPixelFormat(MetalConverters::ToPixelFormat(_desc.m_desc.m_format));
        desc->setArrayLength(_desc.m_desc.m_arraySize);
        desc->setTextureType(MetalConverters::GetTextureType(_desc.m_desc.m_type));
        desc->setMipmapLevelCount(_desc.m_desc.m_mipCount);

        desc->setResourceOptions(MetalConverters::GetResourceStorage(_desc.m_memoryUsage));
        desc->setStorageMode(MetalConverters::GetStorageMode(_desc.m_memoryUsage));
        desc->setUsage(MetalConverters::GetTextureUsage(_desc.m_memoryUsage));

        hot->m_texture = _device.newTexture(desc.get());
        hot->m_isSystemTexture = false;

        {
            const auto lock = m_residencySetLock.AutoLock();
            m_residencySet->addAllocation(hot->m_texture);
        }

#if !defined(KE_FINAL)
        NS::String* label = NS::String::string(_desc.m_desc.m_debugName.c_str(), NS::UTF8StringEncoding);
        hot->m_texture->setLabel(label);
#endif

        return { handle };
    }

    TextureHandle MetalResources::RegisterSystemTexture()
    {
        const GenPool::Handle handle = m_textures.Allocate();
        TextureHotData* hot = m_textures.Get(handle);
        hot->m_texture = nullptr;
        hot->m_isSystemTexture = true;
        return { handle };
    }

    bool MetalResources::UnregisterTexture(TextureHandle _handle)
    {
        TextureHotData textureHot;
        if (m_textures.Free(_handle.m_handle, &textureHot))
        {
            {
                const auto lock = m_residencySetLock.AutoLock();
                m_residencySet->removeAllocation(textureHot.m_texture);
            }
            textureHot.m_texture->release();
            return true;
        }
        return false;
    }

    void MetalResources::UpdateSystemTexture(TextureHandle _handle, MTL::Texture* _texture)
    {
        TextureHotData* textureHotData = m_textures.Get(_handle.m_handle);
        if (textureHotData != nullptr)
        {
            if (textureHotData->m_texture != nullptr)
                textureHotData->m_texture->release();
            textureHotData->m_texture = _texture->retain();
        }
    }

    SamplerHandle MetalResources::CreateSampler(MTL::Device& _device, const SamplerDesc& _desc)
    {
        const GenPool::Handle handle = m_samplers.Allocate();

        SamplerHotData* hot = m_samplers.Get(handle);

        NsPtr<MTL::SamplerDescriptor> descriptor { MTL::SamplerDescriptor::alloc()->init() };

        descriptor->setMinFilter(MetalConverters::GetMinMagFilter(_desc.m_minFilter));
        descriptor->setMagFilter(MetalConverters::GetMinMagFilter(_desc.m_magFilter));
        descriptor->setMipFilter(MetalConverters::GetMipFilter(_desc.m_mipFilter));
        descriptor->setRAddressMode(MetalConverters::GetAddressMode(_desc.m_addressModeW));
        descriptor->setSAddressMode(MetalConverters::GetAddressMode(_desc.m_addressModeU));
        descriptor->setTAddressMode(MetalConverters::GetAddressMode(_desc.m_addressModeV));
        descriptor->setMaxAnisotropy(eastl::max<u8>(_desc.m_anisotropy, 1u));
        descriptor->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
        descriptor->setLodMinClamp(_desc.m_lodMin);
        descriptor->setLodMaxClamp(_desc.m_lodMax);
        descriptor->setSupportArgumentBuffers(true);
        KE_ASSERT_FATAL_MSG(_desc.m_opType == SamplerDesc::OpType::Blend, "Metal doesn't support min max filters");
        KE_ASSERT_FATAL_MSG(_desc.m_lodBias == 0.f, "Metal doesn't support lod bias on samplers");

#if !defined(KE_FINAL)
        auto* string = NS::String::string(_desc.m_debugName.c_str(), NS::UTF8StringEncoding);
        descriptor->setLabel(string);
#endif

        hot->m_sampler = _device.newSamplerState(descriptor.get());

        return { handle };
    }

    bool MetalResources::DestroySampler(SamplerHandle _sampler)
    {
        SamplerHotData hot {};
        if (m_samplers.Free(_sampler.m_handle, &hot))
        {
            hot.m_sampler->release();
            return true;
        }
        return false;
    }

    TextureViewHandle MetalResources::RegisterTextureView(const TextureViewDesc& _desc)
    {
        const TextureHotData* originalTexture = m_textures.Get(_desc.m_texture.m_handle);
        KE_ASSERT_FATAL(originalTexture != nullptr);

        const GenPool::Handle handle = m_textureViews.Allocate();
        TextureViewHotData* hot = m_textureViews.Get(handle);
        hot->m_texture = originalTexture->m_texture->newTextureView(
            MetalConverters::ToPixelFormat(_desc.m_format),
            MetalConverters::GetTextureType(_desc.m_viewType),
            { _desc.m_minMip, static_cast<u32>(_desc.m_maxMip + 1 - _desc.m_minMip) },
            { _desc.m_arrayStart, _desc.m_arrayRange },
            {
                MetalConverters::GetSwizzle(_desc.m_componentsMapping[0]),
                MetalConverters::GetSwizzle(_desc.m_componentsMapping[1]),
                MetalConverters::GetSwizzle(_desc.m_componentsMapping[2]),
                MetalConverters::GetSwizzle(_desc.m_componentsMapping[3])
            });
        KE_ASSERT_FATAL(hot->m_texture != nullptr);

#if !defined(KE_FINAL)
        KE_AUTO_RELEASE_POOL;
        NS::String* label = NS::String::string(_desc.m_debugName.c_str(), NS::UTF8StringEncoding);
        hot->m_texture->setLabel(label);
#endif

        return { handle };
    }

    bool MetalResources::UnregisterTextureView(TextureViewHandle _textureView)
    {
        TextureViewHotData hot {};
        if (m_textureViews.Free(_textureView.m_handle, &hot))
        {
            hot.m_texture->release();
            return true;
        }
        return false;
    }

    BufferViewHandle MetalResources::RegisterBufferView(const BufferViewDesc& _viewDesc)
    {
        const BufferHotData* originalBuffer = m_buffers.Get(_viewDesc.m_buffer.m_handle);
        KE_ASSERT_FATAL(originalBuffer != nullptr);

        const GenPool::Handle handle = m_bufferViews.Allocate();
        BufferViewHotData* hot = m_bufferViews.Get(handle);
        hot->m_buffer = originalBuffer->m_buffer->retain();
        hot->m_offset = _viewDesc.m_offset;

        return { handle };
    }

    bool MetalResources::UnregisterBufferView(BufferViewHandle _handle)
    {
        BufferViewHotData hot {};
        if (m_bufferViews.Free(_handle.m_handle, &hot))
        {
            hot.m_buffer->release();
            return true;
        }
        return false;
    }

    RenderTargetViewHandle MetalResources::RegisterRtv(const RenderTargetViewDesc& _desc)
    {
        MTL::Texture* texture = m_textures.Get(_desc.m_texture.m_handle)->m_texture;
        return InternalRegisterRtv(_desc, texture);
    }

    bool MetalResources::UnregisterRtv(RenderTargetViewHandle _handle)
    {
        RtvHotData hotData {};
        bool freed = m_renderTargetViews.Free(_handle.m_handle, &hotData);
        if (freed)
            hotData.m_texture->release();
        return freed;
    }

    RenderTargetViewHandle MetalResources::RegisterSystemRtv(
        const RenderTargetViewDesc& _desc)
    {
        return InternalRegisterRtv(_desc, nullptr);
    }

    void MetalResources::UpdateSystemTexture(RenderTargetViewHandle _handle, MTL::Texture* _newTexture)
    {
        RtvHotData* rtvHotData = m_renderTargetViews.Get(_handle.m_handle);
        if (rtvHotData != nullptr)
        {
            if (rtvHotData->m_texture != nullptr)
                rtvHotData->m_texture->release();
            rtvHotData->m_texture = _newTexture->retain();
        }
    }

    RenderTargetViewHandle MetalResources::InternalRegisterRtv(
        const RenderTargetViewDesc& _desc,
        MTL::Texture* _texture)
    {
        VERIFY_OR_RETURN(_desc.m_arrayRangeSize == 1, { GenPool::kInvalidHandle });

        const GenPool::Handle handle = m_renderTargetViews.Allocate();

        auto [rtvHot, rtvCold] = m_renderTargetViews.GetAll(handle);
        rtvHot->m_texture = _texture;
        rtvHot->m_isSystemTexture = _desc.m_texture == GenPool::kInvalidHandle;
        *rtvCold = RtvColdData {
            .m_pixelFormat = _desc.m_format,
            .m_slice = _desc.m_type == TextureTypes::Single3D ? u16(0u) : _desc.m_arrayRangeStart,
            .m_depthSlice = _desc.m_type == TextureTypes::Single3D ? _desc.m_depthStartSlice : u16(0u),
            .m_mipLevel = _desc.m_mipLevel,
            .m_plane = _desc.m_plane,
        };

        return { handle };
    }

    RenderPassHandle MetalResources::CreateRenderPassDescriptor(const RenderPassDesc& _desc)
    {
        const GenPool::Handle handle = m_renderPasses.Allocate();

        auto [hotData, coldData] = m_renderPasses.GetAll(handle);
        hotData->m_descriptor = MTL4::RenderPassDescriptor::alloc()->init();

        coldData->m_colorFormats.clear(true);

        hotData->m_systemRtvs.set_overflow_allocator(GetAllocator());
        for (auto i = 0u; i < _desc.m_colorAttachments.size(); i++)
        {
            MTL::RenderPassColorAttachmentDescriptor* attachment =
                hotData->m_descriptor->colorAttachments()->object(i);
            const RenderPassDesc::Attachment& attachmentDesc = _desc.m_colorAttachments[i];

            auto [rtvHotData, rtvColdData] = m_renderTargetViews.GetAll(attachmentDesc.m_rtv.m_handle);
            MTL::Texture* texture = rtvHotData->m_texture;
            KE_ASSERT_FATAL(texture != nullptr || rtvHotData->m_isSystemTexture);
            if (rtvHotData->m_isSystemTexture)
            {
                hotData->m_systemRtvs.push_back({ attachmentDesc.m_rtv, static_cast<u8>(i) });
            }

            attachment->setTexture(texture);
            attachment->setLoadAction(MetalConverters::GetMetalLoadOperation(attachmentDesc.m_loadOperation));
            attachment->setStoreAction(MetalConverters::GetMetalStoreOperation(attachmentDesc.m_storeOperation));
            attachment->setClearColor(MTL::ClearColor(
                attachmentDesc.m_clearColor.r,
                attachmentDesc.m_clearColor.g,
                attachmentDesc.m_clearColor.b,
                attachmentDesc.m_clearColor.a));

            coldData->m_colorFormats.push_back(rtvColdData->m_pixelFormat);
        }

        if (_desc.m_depthStencilAttachment.has_value())
        {
            const RenderPassDesc::DepthStencilAttachment& attachmentDesc = _desc.m_depthStencilAttachment.value();

            auto [rtvHot, rtvCold] = m_renderTargetViews.GetAll(attachmentDesc.m_rtv.m_handle);
            KE_ASSERT_FATAL(rtvHot != nullptr);

            if (BitUtils::EnumHasAny(rtvCold->m_plane, TexturePlane::Depth))
            {
                MTL::RenderPassDepthAttachmentDescriptor* attachment = hotData->m_descriptor->depthAttachment();
                attachment->setTexture(rtvHot->m_texture);
                attachment->setLoadAction(MetalConverters::GetMetalLoadOperation(attachmentDesc.m_loadOperation));
                attachment->setStoreAction(MetalConverters::GetMetalStoreOperation(attachmentDesc.m_storeOperation));
                attachment->setClearDepth(attachmentDesc.m_clearColor.r);
            }

            if (BitUtils::EnumHasAny(rtvCold->m_plane, TexturePlane::Stencil))
            {
                MTL::RenderPassStencilAttachmentDescriptor* attachment = hotData->m_descriptor->stencilAttachment();
                attachment->setTexture(rtvHot->m_texture);
                attachment->setLoadAction(MetalConverters::GetMetalLoadOperation(attachmentDesc.m_stencilLoadOperation));
                attachment->setStoreAction(MetalConverters::GetMetalStoreOperation(attachmentDesc.m_stencilStoreOperation));
                attachment->setClearStencil(attachmentDesc.m_stencilClearValue);
            }

            coldData->m_depthStencilFormat = rtvCold->m_pixelFormat;
        }
        else
        {
            coldData->m_depthStencilFormat = TextureFormat::NoFormat;
        }

        return { handle };
    }

    bool MetalResources::DestroyRenderPassDescriptor(RenderPassHandle _handle)
    {
        RenderPassHotData data;
        RenderPassColdData coldData;
        if (m_renderPasses.Free(_handle.m_handle, &data, &coldData))
        {
            data.m_descriptor->release();
            coldData.m_colorFormats.clear();
            return true;
        }
        return false;
    }

    ShaderModuleHandle MetalResources::LoadLibrary(MTL::Device& _device, void* _bytecode, size_t _size)
    {
        const GenPool::Handle handle = m_libraries.Allocate();

        ShaderModuleHotData* hot = m_libraries.Get(handle);

        KE_AUTO_RELEASE_POOL;
        dispatch_data_t data = dispatch_data_create(_bytecode, _size, nullptr, {});
        hot->m_library = _device.newLibrary(data, nullptr)->retain();

        return { handle };
    }

    bool MetalResources::FreeLibrary(ShaderModuleHandle _library)
    {
        ShaderModuleHotData hot {};
        if (m_libraries.Free(_library.m_handle, &hot))
        {
            hot.m_library->release();
            return true;
        }
        return false;
    }

    GraphicsPipelineHandle MetalResources::CreateGraphicsPso(
        MTL::Device& _device,
        MetalArgumentBufferManager& _argBufferManager,
        const GraphicsPipelineDesc& _desc)
    {
        KE_AUTO_RELEASE_POOL;

        const MetalArgumentBufferManager::PipelineLayoutHotData* hotLayout =
            _argBufferManager.m_pipelineLayouts.Get(_desc.m_pipelineLayout.m_handle);
        KE_ASSERT_FATAL(hotLayout != nullptr);

        const GenPool::Handle handle = m_graphicsPso.Allocate();
        GraphicsPsoHotData* hot = m_graphicsPso.Get(handle);

        // Init descriptor
        NsPtr<MTL::RenderPipelineDescriptor> descriptor { MTL::RenderPipelineDescriptor::alloc()->init() };

        // Set up stages
        {
            for (const auto& stage: _desc.m_stages)
            {
                ShaderModuleHotData* libHot = m_libraries.Get(stage.m_shaderModule.m_handle);
                KE_ASSERT_FATAL(libHot != nullptr);
                const MTL::Function* function = libHot->m_library->newFunction(NS::String::string(
                    stage.m_entryPoint.c_str(), NS::UTF8StringEncoding));
                KE_ASSERT_FATAL(function != nullptr);

                switch (stage.m_stage)
                {
                case ShaderStage::Stage::Vertex:
                    descriptor->setVertexFunction(function);
                    break;
                case ShaderStage::Stage::Fragment:
                    descriptor->setFragmentFunction(function);
                    break;
                default:
                    KE_ERROR("Unsupported stage");
                    break;
                }
            }
        }

        // Set up vertex input
        if (!_desc.m_vertexInput.m_elements.empty())
        {
            NsPtr vertexDescriptor { MTL::VertexDescriptor::alloc()->init() };

            for (const auto& element: _desc.m_vertexInput.m_elements)
            {
                MTL::VertexAttributeDescriptor* attribute = vertexDescriptor->attributes()->object(element.m_location);
                attribute->setFormat(MetalConverters::GetVertexFormat(element.m_format));
                attribute->setOffset(element.m_offset);
                attribute->setBufferIndex(MetalConstants::kVertexStreamBuffersOffset + element.m_bindingIndex);

                KE_ASSERT(element.m_bindingIndex < MetalConstants::kMaxVertexBuffers);
            }

            for (const auto& binding: _desc.m_vertexInput.m_bindings)
            {
                MTL::VertexBufferLayoutDescriptor* layout = vertexDescriptor->layouts()->object(MetalConstants::kVertexStreamBuffersOffset + binding.m_binding);
                KE_ASSERT(binding.m_binding < MetalConstants::kMaxVertexBuffers);
                layout->setStride(binding.m_stride);
                switch (binding.m_inputRate)
                {
                case VertexInputRate::Vertex:
                    layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
                    break;
                case VertexInputRate::Instance:
                    layout->setStepFunction(MTL::VertexStepFunctionPerInstance);
                    break;
                }
            }

            descriptor->setVertexDescriptor(vertexDescriptor.get());
        }

        // Save input assembly data
        {
            hot->m_topology = _desc.m_inputAssembly.m_topology;
        }

        // Set up raster state
        {
            if (_desc.m_rasterState.m_depthBias)
            {
                hot->m_staticState.m_depthBias = _desc.m_rasterState.m_depthBiasConstantFactor;
                hot->m_staticState.m_depthBiasSlope = _desc.m_rasterState.m_depthBiasSlopeFactor;
                hot->m_staticState.m_depthBiasClamp = _desc.m_rasterState.m_depthBiasClampValue;
            }
            else
            {
                hot->m_staticState.m_depthBias = 0;
                hot->m_staticState.m_depthBiasSlope = 0;
                hot->m_staticState.m_depthBiasClamp = 0;
            }

            hot->m_staticState.m_fillMode = _desc.m_rasterState.m_fillMode;
            hot->m_staticState.m_cullMode = _desc.m_rasterState.m_cullMode;
            hot->m_staticState.m_front = _desc.m_rasterState.m_front;
            hot->m_staticState.m_depthClip = _desc.m_rasterState.m_depthClip;
        }

        const RenderPassColdData* passCold = m_renderPasses.GetCold(_desc.m_renderPass.m_handle);
        KE_ASSERT_FATAL(passCold->m_colorFormats.size() == _desc.m_colorBlending.m_attachments.size());

        // Set up color attachments and blend state
        {
            hot->m_staticState.m_blendFactor = _desc.m_colorBlending.m_blendFactor;
            hot->m_dynamicBlendFactor = _desc.m_colorBlending.m_dynamicBlendFactor;

            for (size_t i = 0; i < passCold->m_colorFormats.size(); i++)
            {
                const ColorAttachmentBlendDesc& blendDesc = _desc.m_colorBlending.m_attachments[i];
                MTL::RenderPipelineColorAttachmentDescriptor* attachment = descriptor->colorAttachments()->object(i);

                attachment->setPixelFormat(MetalConverters::ToPixelFormat(passCold->m_colorFormats[i]));
                attachment->setWriteMask(MetalConverters::GetColorWriteMask(blendDesc.m_writeMask));

                attachment->setBlendingEnabled(blendDesc.m_blendEnable);

                attachment->setRgbBlendOperation(MetalConverters::GetBlendOperation(blendDesc.m_colorOp));
                attachment->setSourceRGBBlendFactor(MetalConverters::GetBlendFactor(blendDesc.m_srcColor));
                attachment->setDestinationRGBBlendFactor(MetalConverters::GetBlendFactor(blendDesc.m_dstColor));

                attachment->setAlphaBlendOperation(MetalConverters::GetBlendOperation(blendDesc.m_alphaOp));
                attachment->setSourceAlphaBlendFactor(MetalConverters::GetBlendFactor(blendDesc.m_srcAlpha));
                attachment->setDestinationAlphaBlendFactor(MetalConverters::GetBlendFactor(blendDesc.m_dstAlpha));
            }
        }

        // Set depth stencil format
        if (passCold->m_depthStencilFormat != TextureFormat::NoFormat)
        {
            switch (passCold->m_depthStencilFormat)
            {
            case TextureFormat::D16:
            case TextureFormat::D24S8:
            case TextureFormat::D32F:
            case TextureFormat::D32FS8:
                descriptor->setDepthAttachmentPixelFormat(MetalConverters::ToPixelFormat(passCold->m_depthStencilFormat));
                break;
            default:
                KE_ERROR("Unsupported format");
            }
        }

        // Set up depth stencil state
        {
            DepthStencilStateDesc dsDesc = _desc.m_depthStencil;

            // Harmonize desc with metal behaviour, limiting depth state swaps
            {
                if (!dsDesc.m_depthTest)
                {
                    dsDesc.m_depthCompare = DepthStencilStateDesc::CompareOp::Always;
                    dsDesc.m_depthTest = true;
                }

                if (!dsDesc.m_stencilTest)
                {
                    dsDesc.m_front = {
                        .m_compareOp = DepthStencilStateDesc::CompareOp::Always,
                    };
                    dsDesc.m_back = dsDesc.m_front;
                    dsDesc.m_stencilTest = true;
                }

                hot->m_dynamicStencilRef = dsDesc.m_dynamicStencilRef;
                dsDesc.m_dynamicStencilRef = false;

                hot->m_staticState.m_stencilRefValue = dsDesc.m_stencilRef;
                dsDesc.m_stencilRef = 0;
            }

            hot->m_staticState.m_depthStencilHash = Hashing::Hash64(&dsDesc);

            NsPtr stateDesc { MTL::DepthStencilDescriptor::alloc()->init() };
            stateDesc->setDepthWriteEnabled(dsDesc.m_depthWrite);
            stateDesc->setDepthCompareFunction(MetalConverters::GetCompareOperation(dsDesc.m_depthCompare));

            NsPtr frontStencilDesc { MTL::StencilDescriptor::alloc()->init() };
            frontStencilDesc->setWriteMask(dsDesc.m_stencilWriteMask);
            frontStencilDesc->setReadMask(dsDesc.m_stencilReadMask);
            frontStencilDesc->setDepthStencilPassOperation(MetalConverters::GetStencilOperation(dsDesc.m_front.m_passOp));
            frontStencilDesc->setStencilFailureOperation(MetalConverters::GetStencilOperation(dsDesc.m_front.m_failOp));
            frontStencilDesc->setDepthFailureOperation(MetalConverters::GetStencilOperation(dsDesc.m_front.m_depthFailOp));
            frontStencilDesc->setStencilCompareFunction(MetalConverters::GetCompareOperation(dsDesc.m_front.m_compareOp));
            stateDesc->setFrontFaceStencil(frontStencilDesc.get());

            NsPtr backStencilDesc { MTL::StencilDescriptor::alloc()->init() };
            backStencilDesc->setWriteMask(dsDesc.m_stencilWriteMask);
            backStencilDesc->setReadMask(dsDesc.m_stencilReadMask);
            backStencilDesc->setDepthStencilPassOperation(MetalConverters::GetStencilOperation(dsDesc.m_back.m_passOp));
            backStencilDesc->setStencilFailureOperation(MetalConverters::GetStencilOperation(dsDesc.m_back.m_failOp));
            backStencilDesc->setDepthFailureOperation(MetalConverters::GetStencilOperation(dsDesc.m_back.m_depthFailOp));
            backStencilDesc->setStencilCompareFunction(MetalConverters::GetCompareOperation(dsDesc.m_back.m_compareOp));
            stateDesc->setBackFaceStencil(backStencilDesc.get());

            hot->m_depthStencilState = _device.newDepthStencilState(stateDesc.get());
        }

#if !defined(KE_FINAL)
        descriptor->setLabel(NS::String::string(_desc.m_debugName.c_str(), NS::UTF8StringEncoding));
#endif

        // Create PSO
        {
            NS::Error* error;
            hot->m_pso = _device.newRenderPipelineState(descriptor.get(), &error);
            KE_ASSERT_FATAL_MSG(hot->m_pso != nullptr, error->localizedDescription()->cString(NS::UTF8StringEncoding));
        }

        return { handle };
    }

    bool MetalResources::DestroyGraphicsPso(GraphicsPipelineHandle _pso)
    {
        GraphicsPsoHotData hot {};
        if (m_graphicsPso.Free(_pso.m_handle, &hot))
        {
            hot.m_depthStencilState->release();
            hot.m_pso->release();
            return true;
        }
        return false;
    }

    ComputePipelineHandle MetalResources::CreateComputePso(
        MTL::Device& _device,
        MetalArgumentBufferManager& _argBufferManager,
        const ComputePipelineDesc& _desc)
    {
        KE_AUTO_RELEASE_POOL;

        MTL::ComputePipelineDescriptor* descriptor = MTL::ComputePipelineDescriptor::alloc()->init();

        {
            ShaderModuleHotData* libHot = m_libraries.Get(_desc.m_computeStage.m_shaderModule.m_handle);
            KE_ASSERT_FATAL(libHot != nullptr);
            MTL::Library* library = libHot->m_library;
            MTL::Function* function = library->newFunction(NS::String::string(_desc.m_computeStage.m_entryPoint.c_str(), NS::UTF8StringEncoding));
            descriptor->setComputeFunction(function);
        }

#if !defined(KE_FINAL)
        descriptor->setLabel(NS::String::string(_desc.m_debugName.c_str(), NS::UTF8StringEncoding));
#endif

        GenPool::Handle handle = m_computePso.Allocate();
        ComputePsoHotData& hot = *m_computePso.Get(handle);

        {
            NS::Error* error;
            hot.m_pso = _device.newComputePipelineState(descriptor, MTL::PipelineOptionNone, nullptr, &error);
            KE_ASSERT_FATAL_MSG(hot.m_pso != nullptr, error->localizedDescription()->cString(NS::UTF8StringEncoding));
        }

        return { handle };
    }

    bool MetalResources::DestroyComputePso(ComputePipelineHandle _pso)
    {
        ComputePsoHotData hot {};
        if (m_computePso.Free(_pso.m_handle, &hot))
        {
            hot.m_pso->release();
            return true;
        }
        return false;
    }
} // namespace KryneEngine