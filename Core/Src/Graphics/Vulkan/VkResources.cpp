/**
 * @file
 * @author Max Godefroy
 * @date 19/02/2024.
 */

#include "Graphics/Vulkan/VkResources.hpp"

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkDebugHandler.hpp"
#include "Graphics/Vulkan/VkDescriptorSetManager.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Graphics/RenderPass.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"

namespace KryneEngine
{
    VkResources::VkResources(AllocatorInstance _allocator)
        : m_buffers(_allocator)
        , m_textures(_allocator)
        , m_imageViews(_allocator)
        , m_samplers(_allocator)
        , m_bufferViews(_allocator)
        , m_renderTargetViews(_allocator)
        , m_renderPasses(_allocator)
        , m_shaderModules(_allocator)
        , m_pipelineLayouts(_allocator)
        , m_pipelines(_allocator)
    {}

    VkResources::~VkResources() = default;

    void VkResources::FlushPools()
    {
        m_buffers.FlushDeferredFrees();
        m_textures.FlushDeferredFrees();
        m_imageViews.FlushDeferredFrees();
        m_samplers.FlushDeferredFrees();
        m_bufferViews.FlushDeferredFrees();
        m_renderTargetViews.FlushDeferredFrees();
        m_renderPasses.FlushDeferredFrees();
        m_shaderModules.FlushDeferredFrees();
        m_pipelineLayouts.FlushDeferredFrees();
        m_pipelines.FlushDeferredFrees();

    }

    void VkResources::InitAllocator(
        const GraphicsCommon::ApplicationInfo& _appInfo,
        VkDevice _device,
        VkPhysicalDevice _physicalDevice,
        VkInstance _instance)
    {
        KE_ZoneScopedFunction("VkResources::InitAllocator");

        const VmaAllocatorCreateInfo createInfo {
            .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
            .physicalDevice = _physicalDevice,
            .device = _device,
            .instance = _instance,
            .vulkanApiVersion = VkHelperFunctions::GetApiVersion(_appInfo.m_api),
        };

        vmaCreateAllocator(&createInfo, &m_allocator);
    }

    void VkResources::DestroyAllocator()
    {
        vmaDestroyAllocator(m_allocator);
    }

    BufferHandle VkResources::CreateBuffer(const BufferCreateDesc& _desc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateBuffer");

        VERIFY_OR_RETURN(_desc.m_desc.m_size > 0, { GenPool::kInvalidHandle });
        VERIFY_OR_RETURN(BitUtils::EnumHasAny(_desc.m_usage, ~MemoryUsage::USAGE_TYPE_MASK), { GenPool::kInvalidHandle });

        using namespace VkHelperFunctions;

        const VkBufferCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .flags = 0,
            .size = _desc.m_desc.m_size,
            .usage = RetrieveBufferUsage(_desc.m_usage),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VmaAllocationCreateInfo allocationInfo {};
        const MemoryUsage usageType = _desc.m_usage & MemoryUsage::USAGE_TYPE_MASK;
        if (usageType == MemoryUsage::GpuOnly_UsageType)
        {
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        }
        else if (usageType == MemoryUsage::StageOnce_UsageType)
        {
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }
        else if (usageType == MemoryUsage::StageEveryFrame_UsageType)
        {
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                   | VMA_ALLOCATION_CREATE_MAPPED_BIT
                                   | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
        }
        else if (usageType == MemoryUsage::Readback_UsageType)
        {
            allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                   | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        const GenPool::Handle handle = m_buffers.Allocate();

        VkBuffer* buffer = m_buffers.Get(handle);
        BufferColdData* coldData = m_buffers.GetCold(handle);
        VkAssert(vmaCreateBuffer(
            m_allocator,
            &createInfo,
            &allocationInfo,
            buffer,
            &coldData->m_allocation,
            &coldData->m_info));


#if !defined(KE_FINAL)
        m_debugHandler->SetName(
            _device,
            VK_OBJECT_TYPE_BUFFER,
            reinterpret_cast<u64>(*buffer),
            _desc.m_desc.m_debugName);
        ZoneText(_desc.m_desc.m_debugName.c_str(), _desc.m_desc.m_debugName.size());
#endif

        return { handle };
    }

    BufferHandle VkResources::CreateStagingBuffer(
        const TextureDesc& _createDesc,
        const eastl::span<const TextureMemoryFootprint>& _footprints,
        VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateStagingBuffer");

        const u64 bufferWidth = _footprints.back().m_offset + _footprints.back().m_lineByteAlignedSize
            * _footprints.back().m_height
            * _footprints.back().m_depth;

        const VkBufferCreateInfo bufferCreateInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .flags = 0,
            .size = bufferWidth,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        const VmaAllocationCreateInfo allocationCreateInfo {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        };

        const GenPool::Handle handle = m_buffers.Allocate();

        VkBuffer buffer;
        BufferColdData coldData {};
        VkAssert(vmaCreateBuffer(
            m_allocator,
            &bufferCreateInfo,
            &allocationCreateInfo,
            &buffer,
            &coldData.m_allocation,
            &coldData.m_info));

#if !defined(KE_FINAL)
        m_debugHandler->SetName(
            _device,
            VK_OBJECT_TYPE_BUFFER,
            reinterpret_cast<u64>(buffer),
            (_createDesc.m_debugName + "_Staging").c_str());
        ZoneTextF("%s_Staging", _createDesc.m_debugName.c_str());
#endif

        *m_buffers.Get(handle) = buffer;
        *m_buffers.GetCold(handle) = coldData;

        return { handle };
    }

    bool VkResources::DestroyBuffer(BufferHandle _buffer)
    {
        KE_ZoneScopedFunction("VkResources::DestroyBuffer");

        VkBuffer buffer;
        BufferColdData coldData {};

        if (m_buffers.Free(_buffer.m_handle, &buffer, &coldData))
        {
            vmaDestroyBuffer(m_allocator, buffer, coldData.m_allocation);
            return true;
        }

        return false;
    }

    TextureHandle VkResources::RegisterTexture(VkImage _image, const uint3& _dimensions)
    {
        KE_ZoneScopedFunction("VkResources::RegisterTexture");

        const auto handle = m_textures.Allocate();
        *m_textures.Get(handle) = _image;
        *m_textures.GetCold(handle) = { nullptr, _dimensions };
        return { handle };
    }

    TextureHandle VkResources::CreateTexture(const TextureCreateDesc& _desc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateTexture");

        using namespace VkHelperFunctions;

        const VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = 0,
            .imageType = RetrieveImageType(_desc.m_desc.m_type),
            .format = ToVkFormat(_desc.m_desc.m_format),
            .extent = {
                _desc.m_desc.m_dimensions.x,
                _desc.m_desc.m_dimensions.y,
                _desc.m_desc.m_dimensions.z,
            },
            .mipLevels = _desc.m_desc.m_mipCount,
            .arrayLayers = _desc.m_desc.m_arraySize,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .usage = RetrieveImageUsage(_desc.m_memoryUsage),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        const VmaAllocationCreateInfo allocationCreateInfo {
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };

        VkImage image;
        VmaAllocation allocation;
        VkAssert(vmaCreateImage(
            m_allocator,
            &imageCreateInfo,
            &allocationCreateInfo,
            &image,
            &allocation,
            nullptr));

        const GenPool::Handle handle = m_textures.Allocate();
        *m_textures.Get(handle) = image;
        *m_textures.GetCold(handle) = { allocation, _desc.m_desc.m_dimensions };

#if !defined(KE_FINAL)
        m_debugHandler->SetName(
            _device,
            VK_OBJECT_TYPE_IMAGE,
            reinterpret_cast<u64>(image),
            _desc.m_desc.m_debugName.c_str());
        ZoneText(_desc.m_desc.m_debugName.c_str(), _desc.m_desc.m_debugName.size());
#endif

        return { handle };
    }

    bool VkResources::ReleaseTexture(TextureHandle _texture, VkDevice _device, bool _free)
    {
        KE_ZoneScopedFunction("VkResources::ReleaseTexture");

        VkImage image;
        TextureColdData coldData;
        if (m_textures.Free(_texture.m_handle, &image, &coldData))
        {
            if (_free)
            {
                vmaDestroyImage(m_allocator, image, coldData.m_allocation);
            }
            return true;
        }
        return false;
    }

    TextureViewHandle VkResources::CreateTextureView(const TextureViewDesc& _viewDesc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateTextureView");

        auto* image = m_textures.Get(_viewDesc.m_texture.m_handle);
        if (image == nullptr)
        {
            return { GenPool::kInvalidHandle };
        }

        const GenPool::Handle srvHandle = m_imageViews.Allocate();

        VkImageView imageView = CreateImageView(
            _device,
            *image,
            VkHelperFunctions::RetrieveImageViewType(_viewDesc.m_viewType),
            VkHelperFunctions::ToVkFormat(_viewDesc.m_format),
            VkHelperFunctions::ToVkComponentMapping(_viewDesc.m_componentsMapping),
            VkHelperFunctions::RetrieveAspectMask(_viewDesc.m_plane),
            _viewDesc.m_minMip,
            _viewDesc.m_maxMip - _viewDesc.m_minMip + 1,
            _viewDesc.m_arrayStart,
            _viewDesc.m_arrayRange);

#if !defined(KE_FINAL)
        {
            const u64 handle = reinterpret_cast<u64>(imageView);
            m_debugHandler->SetName(_device, VK_OBJECT_TYPE_IMAGE_VIEW, handle, _viewDesc.m_debugName.c_str());
            ZoneText(_viewDesc.m_debugName.c_str(), _viewDesc.m_debugName.size());
        }
#endif

        *m_imageViews.Get(srvHandle) = imageView;

        return { srvHandle };
    }

    bool VkResources::DestroyTextureView(TextureViewHandle _textureView, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroyTextureView");

        VkImageView imageView;

        if (m_imageViews.Free(_textureView.m_handle, &imageView))
        {
            vkDestroyImageView(_device, imageView, nullptr);
            return true;
        }
        return false;
    }

    SamplerHandle VkResources::CreateSampler(const SamplerDesc& _samplerDesc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateSampler");

        VkSamplerCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = _samplerDesc.m_magFilter == SamplerDesc::Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
            .minFilter = _samplerDesc.m_minFilter == SamplerDesc::Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
            .mipmapMode = _samplerDesc.m_mipFilter == SamplerDesc::Filter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VkHelperFunctions::ToVkAddressMode(_samplerDesc.m_addressModeU),
            .addressModeV = VkHelperFunctions::ToVkAddressMode(_samplerDesc.m_addressModeV),
            .addressModeW = VkHelperFunctions::ToVkAddressMode(_samplerDesc.m_addressModeW),
            .mipLodBias = _samplerDesc.m_lodBias,
            .anisotropyEnable = _samplerDesc.m_anisotropy > 0,
            .maxAnisotropy = static_cast<float>(_samplerDesc.m_anisotropy),
            .compareEnable = _samplerDesc.m_opType != SamplerDesc::OpType::Blend,
            .minLod = _samplerDesc.m_lodMin,
            .maxLod = _samplerDesc.m_lodMax,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = false,
        };

        switch (_samplerDesc.m_opType)
        {
        case SamplerDesc::OpType::Blend:
            createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            break;
        case SamplerDesc::OpType::Minimum:
            createInfo.compareOp = VK_COMPARE_OP_LESS;
            break;
        case SamplerDesc::OpType::Maximum:
            createInfo.compareOp = VK_COMPARE_OP_GREATER;
            break;
        }

        const GenPool::Handle handle = m_samplers.Allocate();
        VkAssert(vkCreateSampler(_device, &createInfo, nullptr, m_samplers.Get(handle)));
        return { handle };
    }

    bool VkResources::DestroySampler(SamplerHandle _sampler, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroySampler");

        VkSampler sampler;
        if (m_samplers.Free(_sampler.m_handle, &sampler))
        {
            vkDestroySampler(_device, sampler, nullptr);
            return true;
        }
        return false;
    }

    BufferViewHandle VkResources::CreateBufferView(const BufferViewDesc & _viewDesc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateBufferView");

        const VkBuffer* buffer = m_buffers.Get(_viewDesc.m_buffer.m_handle);
        const auto handle = m_bufferViews.Allocate();

        BufferSpan* bufferView = m_bufferViews.Get(handle);
        bufferView->m_buffer = *buffer;
        bufferView->m_offset = _viewDesc.m_offset;
        bufferView->m_size = _viewDesc.m_size;

        return { handle };
    }

    bool VkResources::DestroyBufferView(BufferViewHandle _handle, VkDevice _device)
    {
        return m_bufferViews.Free(_handle.m_handle);
    }

    RenderTargetViewHandle VkResources::CreateRenderTargetView(
            const RenderTargetViewDesc &_desc,
            const VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateRenderTargetView");

        auto* image = m_textures.Get(_desc.m_texture.m_handle);
        if (image == nullptr)
        {
            return { GenPool::kInvalidHandle };
        }

        const auto rtvHandle = m_renderTargetViews.Allocate();

        const VkFormat format = VkHelperFunctions::ToVkFormat(_desc.m_format);

        VkImageView imageView = CreateImageView(
            _device,
            *image,
            VkHelperFunctions::RetrieveImageViewType(_desc.m_type),
            format,
            { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,  VK_COMPONENT_SWIZZLE_IDENTITY },
            VkHelperFunctions::RetrieveAspectMask(_desc.m_plane),
            _desc.m_mipLevel,
            1,
            _desc.m_arrayRangeStart,
            _desc.m_arrayRangeSize);

#if !defined(KE_FINAL)
        {
            const u64 handle = (u64)static_cast<VkImageView>(imageView);
            m_debugHandler->SetName(_device, VK_OBJECT_TYPE_IMAGE_VIEW, handle, _desc.m_debugName.data());
            ZoneText(_desc.m_debugName.c_str(), _desc.m_debugName.size());
        }
#endif

        *m_renderTargetViews.Get(rtvHandle) = imageView;
        *m_renderTargetViews.GetCold(rtvHandle) = {
            format,
            {
                .m_width = (u16) m_textures.GetCold(_desc.m_texture.m_handle)->m_dimensions.x,
                .m_height = (u16) m_textures.GetCold(_desc.m_texture.m_handle)->m_dimensions.y,
            }
        };

        return { rtvHandle };
    }

    bool VkResources::FreeRenderTargetView(RenderTargetViewHandle _rtv, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::FreeRenderTargetView");

        VkImageView imageView;
        if (m_renderTargetViews.Free(_rtv.m_handle, &imageView))
        {
            vkDestroyImageView(_device, imageView, nullptr);
            return true;
        }
        return false;
    }

    RenderPassHandle VkResources::CreateRenderPass(const RenderPassDesc &_desc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateRenderPass");

        constexpr auto convertLoadOp = [](RenderPassDesc::Attachment::LoadOperation _op)
        {
            switch (_op)
            {
                case RenderPassDesc::Attachment::LoadOperation::Load:
                    return VK_ATTACHMENT_LOAD_OP_LOAD;
                case RenderPassDesc::Attachment::LoadOperation::Clear:
                    return VK_ATTACHMENT_LOAD_OP_CLEAR;
                case RenderPassDesc::Attachment::LoadOperation::DontCare:
                    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }
            KE_ERROR("Unreachable code");
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        };
        constexpr auto convertStoreOp = [](RenderPassDesc::Attachment::StoreOperation _op)
        {
            switch (_op)
            {
                case RenderPassDesc::Attachment::StoreOperation::Store:
                case RenderPassDesc::Attachment::StoreOperation::Resolve:
                    return VK_ATTACHMENT_STORE_OP_STORE;
                case RenderPassDesc::Attachment::StoreOperation::DontCare:
                    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }
            KE_ERROR("Unreachable code");
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        };

        eastl::fixed_vector<
                VkAttachmentDescription,
                RenderPassDesc::kMaxSupportedColorAttachments + 1, // Include optional depth attachment
                false> attachments {};
        eastl::fixed_vector<
                VkImageView,
                RenderPassDesc::kMaxSupportedColorAttachments + 1, // Include optional depth attachment
                false> rtvs {};
        eastl::fixed_vector<
                VkAttachmentReference,
                RenderPassDesc::kMaxSupportedColorAttachments,
                false> attachmentReferences {};
        eastl::vector<VkClearValue> clearValues;

        Size16x2 size {};

        for (const auto& attachment: _desc.m_colorAttachments)
        {
            auto* rtvColdData = m_renderTargetViews.GetCold(attachment.m_rtv.m_handle);
            VERIFY_OR_RETURN(rtvColdData != nullptr, { GenPool::kInvalidHandle });

            if (size.m_width == 0)
            {
                size = rtvColdData->m_size;
            }
            else
            {
                KE_ASSERT(size.m_width == rtvColdData->m_size.m_width && size.m_height == rtvColdData->m_size.m_height);
            }

            VkAttachmentDescription desc {
                .flags = 0,
                .format = rtvColdData->m_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = convertLoadOp(attachment.m_loadOperation),
                .storeOp = convertStoreOp(attachment.m_storeOperation),
                .initialLayout = VkHelperFunctions::ToVkLayout(attachment.m_initialLayout),
                .finalLayout = VkHelperFunctions::ToVkLayout(attachment.m_finalLayout),
            };

            VkAttachmentReference ref {
                u32(attachments.size()),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };

            attachments.push_back(desc);
            rtvs.push_back(*m_renderTargetViews.Get(attachment.m_rtv.m_handle));
            attachmentReferences.push_back(ref);

            VkClearColorValue clearColorValue;
            memcpy(&clearColorValue.float32, &attachment.m_clearColor.x, sizeof(clearColorValue.float32));
            clearValues.push_back({ .color = clearColorValue });
        }

        VkAttachmentReference depthRef;
        if (_desc.m_depthStencilAttachment.has_value())
        {
            const auto& attachment = _desc.m_depthStencilAttachment.value();

            auto* rtvColdData = m_renderTargetViews.GetCold(attachment.m_rtv.m_handle);
            VERIFY_OR_RETURN(rtvColdData != nullptr, { GenPool::kInvalidHandle });

            if (size.m_width == 0)
            {
                size = rtvColdData->m_size;
            }
            else
            {
                KE_ASSERT(size.m_width == rtvColdData->m_size.m_width && size.m_height == rtvColdData->m_size.m_height);
            }

            VkAttachmentDescription desc {
                .flags = 0,
                .format = rtvColdData->m_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = convertLoadOp(attachment.m_loadOperation),
                .storeOp = convertStoreOp(attachment.m_storeOperation),
                .stencilLoadOp = convertLoadOp(attachment.m_loadOperation),
                .initialLayout = VkHelperFunctions::ToVkLayout(attachment.m_initialLayout),
                .finalLayout = VkHelperFunctions::ToVkLayout(attachment.m_finalLayout)
            };

            depthRef = {
                    u32(attachments.size()),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            };

            attachments.push_back(desc);
            rtvs.push_back(*m_renderTargetViews.Get(attachment.m_rtv.m_handle));
            clearValues.push_back(VkClearValue {
                .depthStencil = {
                    .depth = attachment.m_clearColor.r,
                    .stencil = attachment.m_stencilClearValue
                }
            });
        }

        VkSubpassDescription subpassDescription {
                .flags = 0,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = static_cast<u32>(attachmentReferences.size()),
                .pColorAttachments = attachmentReferences.data(),
                .pDepthStencilAttachment = _desc.m_depthStencilAttachment.has_value() ? &depthRef : nullptr
        };

        VkRenderPassCreateInfo createInfo {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .flags = 0,
                .attachmentCount = static_cast<u32>(attachments.size()),
                .pAttachments = attachments.data(),
                .subpassCount = 1,
                .pSubpasses = &subpassDescription
        };

        const auto handle = m_renderPasses.Allocate();
        auto& renderPassData = *m_renderPasses.Get(handle);
        VkAssert(vkCreateRenderPass(_device, &createInfo, nullptr, &renderPassData.m_renderPass));
        renderPassData.m_size = size;
        renderPassData.m_clearValues = eastl::move(clearValues);

        VkFramebufferCreateInfo framebufferCreateInfo {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .flags = 0,
                .renderPass = renderPassData.m_renderPass,
                .attachmentCount = static_cast<u32>(rtvs.size()),
                .pAttachments = rtvs.data(),
                .width = size.m_width,
                .height = size.m_height,
                .layers = 1
        };
        VkAssert(vkCreateFramebuffer(_device, &framebufferCreateInfo, nullptr, &renderPassData.m_framebuffer));

#if !defined(KE_FINAL)
        {
            const u64 objectHandle = (u64)static_cast<VkRenderPass>(renderPassData.m_renderPass);
            const eastl::string name = _desc.m_debugName + "/RenderPass";
            m_debugHandler->SetName(_device, VK_OBJECT_TYPE_RENDER_PASS, objectHandle, name.c_str());
        }
        {
            const u64 objectHandle = (u64)static_cast<VkFramebuffer>(renderPassData.m_framebuffer);
            const eastl::string name = _desc.m_debugName + "/Framebuffer";
            m_debugHandler->SetName(_device, VK_OBJECT_TYPE_FRAMEBUFFER, objectHandle, name.c_str());
        }
        ZoneTextF("%s/RenderPass + %s/Framebuffer", _desc.m_debugName.c_str(), _desc.m_debugName.c_str());
#endif

        return { handle };
    }

    bool VkResources::DestroyRenderPass(RenderPassHandle _renderPass, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroyRenderPass");

        RenderPassData data;
        if (!m_renderPasses.Free(_renderPass.m_handle, &data))
        {
            return false;
        }

        data.m_clearValues.clear();
        vkDestroyFramebuffer(_device, data.m_framebuffer, nullptr);
        vkDestroyRenderPass(_device, data.m_renderPass, nullptr);

        return true;
    }

    ShaderModuleHandle VkResources::CreateShaderModule(void* _bytecodeData, u64 _bytecodeSize, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateShaderModule");

        VERIFY_OR_RETURN(Alignment::IsAligned<u64>(_bytecodeSize, 4u), { GenPool::kInvalidHandle });

        const VkShaderModuleCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = _bytecodeSize,
            .pCode = static_cast<u32*>(_bytecodeData),
        };

        const GenPool::Handle handle = m_shaderModules.Allocate();
        VkAssert(vkCreateShaderModule(_device, &createInfo, nullptr, m_shaderModules.Get(handle)));
        return { handle };
    }

    bool VkResources::DestroyShaderModule(ShaderModuleHandle _shaderModule, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroyShaderModule");

        VkShaderModule module;
        if (m_shaderModules.Free(_shaderModule.m_handle, &module))
        {
            vkDestroyShaderModule(_device, module, nullptr);
            return true;
        }
        return false;
    }

    PipelineLayoutHandle VkResources::CreatePipelineLayout(
        const PipelineLayoutDesc& _desc, VkDevice _device, VkDescriptorSetManager* _setManager)
    {
        KE_ZoneScopedFunction("VkResources::CreatePipelineLayout");

        const GenPool::Handle handle = m_pipelineLayouts.Allocate();
        auto [pLayout, pColdData] = m_pipelineLayouts.GetAll(handle);

        DynamicArray<VkDescriptorSetLayout> setLayouts(_desc.m_descriptorSets.size());
        for (auto i = 0u; i < setLayouts.Size(); i++)
        {
            setLayouts[i] = _setManager->GetDescriptorSetLayout(_desc.m_descriptorSets[i]);
        }

        *pColdData = LayoutColdData{};
        DynamicArray<VkPushConstantRange> pushConstants(_desc.m_pushConstants.size());
        for (auto i = 0u; i < pushConstants.Size(); i++)
        {
            const PushConstantDesc& pushConstant = _desc.m_pushConstants[i];
            pushConstants[i] = {
                .stageFlags = VkHelperFunctions::ToVkShaderStageFlags(pushConstant.m_visibility),
                .offset = static_cast<u32>(pushConstant.m_offset * sizeof(u32)),
                .size = pushConstant.m_sizeInBytes,
            };
            pColdData->m_pushConstants[i] = { pushConstant.m_offset, pushConstant.m_visibility };
        }

        const VkPipelineLayoutCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<u32>(setLayouts.Size()),
            .pSetLayouts = setLayouts.Data(),
            .pushConstantRangeCount = static_cast<u32>(pushConstants.Size()),
            .pPushConstantRanges = pushConstants.Data(),
        };

        VkAssert(vkCreatePipelineLayout(
            _device,
            &createInfo,
            nullptr,
            pLayout));

        return { handle };
    }

    bool VkResources::DestroyPipelineLayout(PipelineLayoutHandle _pipeline, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroyPipelineLayout");

        VkPipelineLayout layout;
        if (m_pipelineLayouts.Free(_pipeline.m_handle, &layout))
        {
            vkDestroyPipelineLayout(_device, layout, nullptr);
            return true;
        }
        return false;
    }

    GraphicsPipelineHandle VkResources::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::CreateGraphicsPipeline");

        // Shader stages

        DynamicArray<VkPipelineShaderStageCreateInfo> shaderStages(_desc.m_stages.size());
        for (auto i = 0u; i < shaderStages.Size(); i++)
        {
            const ShaderStage& stage = _desc.m_stages[i];

            VkShaderModule* pModule = m_shaderModules.Get(stage.m_shaderModule.m_handle);
            VERIFY_OR_RETURN(pModule != nullptr, { GenPool::kInvalidHandle });

            shaderStages[i] = VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VkHelperFunctions::ToVkShaderStageFlagBits(stage.m_stage),
                .module = *pModule,
                .pName = stage.m_entryPoint.c_str(),
            };
        }

        // Vertex input

        DynamicArray<VkVertexInputBindingDescription> vertexInputBindings(_desc.m_vertexInput.m_bindings.size());
        for (auto i = 0u; i < vertexInputBindings.Size(); i++)
        {
            VkVertexInputRate inputRate;
            switch ( _desc.m_vertexInput.m_bindings[i].m_inputRate)
            {
            case VertexInputRate::Vertex:
                inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                break;
            case VertexInputRate::Instance:
                inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                break;
            }

            vertexInputBindings[i] = {
                .binding = _desc.m_vertexInput.m_bindings[i].m_binding,
                .stride = _desc.m_vertexInput.m_bindings[i].m_stride,
                .inputRate = inputRate,
            };
        }

        DynamicArray<VkVertexInputAttributeDescription> vertexInputAttributes(_desc.m_vertexInput.m_elements.size());
        for (auto i = 0; i < vertexInputAttributes.Size(); i++)
        {
            const VertexLayoutElement& element = _desc.m_vertexInput.m_elements[i];
            vertexInputAttributes[i] = {
                .location = element.m_location,
                .binding = element.m_bindingIndex,
                .format = VkHelperFunctions::ToVkFormat(element.m_format),
                .offset = element.m_offset,
            };
        }

        const VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = static_cast<u32>(vertexInputBindings.Size()),
            .pVertexBindingDescriptions = vertexInputBindings.Data(),
            .vertexAttributeDescriptionCount = static_cast<u32>(vertexInputAttributes.Size()),
            .pVertexAttributeDescriptions = vertexInputAttributes.Data(),
        };

        // Input assembly

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VkHelperFunctions::ToVkPrimitiveTopology(_desc.m_inputAssembly.m_topology),
            .primitiveRestartEnable = _desc.m_inputAssembly.m_cutStripAtSpecialIndex
        };

        // Viewport state

        const VkPipelineViewportStateCreateInfo viewportStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        // Raster state

        VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = !_desc.m_rasterState.m_depthClip,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VkHelperFunctions::ToVkPolygonMode(_desc.m_rasterState.m_fillMode),
            .cullMode = VkHelperFunctions::ToVkCullModeFlags(_desc.m_rasterState.m_cullMode),
            .frontFace = VkHelperFunctions::ToVkFrontFace(_desc.m_rasterState.m_front),
            .depthBiasEnable = _desc.m_rasterState.m_depthBias,
            .depthBiasConstantFactor = _desc.m_rasterState.m_depthBiasConstantFactor,
            .depthBiasClamp = _desc.m_rasterState.m_depthBiasClampValue,
            .depthBiasSlopeFactor = _desc.m_rasterState.m_depthBiasSlopeFactor,
            .lineWidth = 1.f
        };

        // Multisample state

        // TODO: Multi-sampling support
        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        // Depth stencil state

        const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = _desc.m_depthStencil.m_depthTest,
            .depthWriteEnable = _desc.m_depthStencil.m_depthWrite,
            .depthCompareOp = VkHelperFunctions::ToVkCompareOp(_desc.m_depthStencil.m_depthCompare),
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = _desc.m_depthStencil.m_stencilTest,
            .front = {
                .failOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_front.m_failOp),
                .passOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_front.m_passOp),
                .depthFailOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_front.m_depthFailOp),
                .compareOp = VkHelperFunctions::ToVkCompareOp(_desc.m_depthStencil.m_front.m_compareOp),
                .compareMask = _desc.m_depthStencil.m_stencilReadMask,
                .writeMask = _desc.m_depthStencil.m_stencilWriteMask,
                .reference = _desc.m_depthStencil.m_stencilRef,
            },
            .back = {
                .failOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_back.m_failOp),
                .passOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_back.m_passOp),
                .depthFailOp = VkHelperFunctions::ToVkStencilOp(_desc.m_depthStencil.m_back.m_depthFailOp),
                .compareOp = VkHelperFunctions::ToVkCompareOp(_desc.m_depthStencil.m_back.m_compareOp),
                .compareMask = _desc.m_depthStencil.m_stencilReadMask,
                .writeMask = _desc.m_depthStencil.m_stencilWriteMask,
                .reference = _desc.m_depthStencil.m_stencilRef,
            },
            .minDepthBounds = 0.f,
            .maxDepthBounds = 1.f,
        };

        // Color blend state

        DynamicArray<VkPipelineColorBlendAttachmentState> attachments(_desc.m_colorBlending.m_attachments.size());
        for (auto i = 0; i < attachments.Size(); i++)
        {
            const ColorAttachmentBlendDesc& attachmentBlendDesc = _desc.m_colorBlending.m_attachments[i];
            attachments[i] = {
                .blendEnable = attachmentBlendDesc.m_blendEnable,
                .srcColorBlendFactor = VkHelperFunctions::ToVkBlendFactor(attachmentBlendDesc.m_srcColor),
                .dstColorBlendFactor = VkHelperFunctions::ToVkBlendFactor(attachmentBlendDesc.m_dstColor),
                .colorBlendOp = VkHelperFunctions::ToVkBlendOp(attachmentBlendDesc.m_colorOp),
                .srcAlphaBlendFactor = VkHelperFunctions::ToVkBlendFactor(attachmentBlendDesc.m_srcAlpha),
                .dstAlphaBlendFactor = VkHelperFunctions::ToVkBlendFactor(attachmentBlendDesc.m_dstAlpha),
                .alphaBlendOp = VkHelperFunctions::ToVkBlendOp(attachmentBlendDesc.m_alphaOp),
                .colorWriteMask = VkHelperFunctions::ToVkColorComponentFlags(attachmentBlendDesc.m_writeMask),
            };
        }

        const VkPipelineColorBlendStateCreateInfo blendStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = _desc.m_colorBlending.m_logicOp != ColorBlendingDesc::LogicOp::None,
            .logicOp = VkHelperFunctions::ToVkLogicOp(_desc.m_colorBlending.m_logicOp),
            .attachmentCount = static_cast<u32>(attachments.Size()),
            .pAttachments = attachments.Data(),
            .blendConstants = {
                _desc.m_colorBlending.m_blendFactor.r,
                _desc.m_colorBlending.m_blendFactor.g,
                _desc.m_colorBlending.m_blendFactor.b,
                _desc.m_colorBlending.m_blendFactor.a,
            }
        };

        // Dynamic state

        eastl::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };

        if (_desc.m_colorBlending.m_dynamicBlendFactor)
        {
            dynamicStates.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        }

        if (_desc.m_depthStencil.m_dynamicStencilRef)
        {
            dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        }

        const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };

        // Layout

        VkPipelineLayout* pLayout = m_pipelineLayouts.Get(_desc.m_pipelineLayout.m_handle);
        VERIFY_OR_RETURN(pLayout != nullptr, { GenPool::kInvalidHandle });

        // Render Pass

        RenderPassData* pRenderPassData = m_renderPasses.Get(_desc.m_renderPass.m_handle);
        VERIFY_OR_RETURN(pRenderPassData != nullptr, { GenPool::kInvalidHandle });

        // Pipeline creation

        const VkGraphicsPipelineCreateInfo pipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<u32>(shaderStages.Size()),
            .pStages = shaderStages.Data(),
            .pVertexInputState = &vertexInputCreateInfo,
            .pInputAssemblyState = &inputAssemblyCreateInfo,
            .pViewportState = &viewportStateCreateInfo,
            .pRasterizationState = &rasterStateCreateInfo,
            .pMultisampleState = &multisampleStateCreateInfo,
            .pDepthStencilState = &depthStencilStateCreateInfo,
            .pColorBlendState = &blendStateCreateInfo,
            .pDynamicState = &dynamicStateCreateInfo,
            .layout = *pLayout,
            .renderPass = pRenderPassData->m_renderPass,
            .subpass = 0,
        };

        const GenPool::Handle handle = m_pipelines.Allocate();
        VkAssert(vkCreateGraphicsPipelines(
            _device,
            VK_NULL_HANDLE,
            1,
            &pipelineCreateInfo,
            nullptr,
            m_pipelines.Get(handle)));

        return { handle };
    }

    bool VkResources::DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline, VkDevice _device)
    {
        KE_ZoneScopedFunction("VkResources::DestroyGraphicsPipeline");

        VkPipeline pipeline;
        if (m_pipelines.Free(_pipeline.m_handle, &pipeline))
        {
            vkDestroyPipeline(_device, pipeline, nullptr);
            return true;
        }
        return false;
    }

    ComputePipelineHandle VkResources::CreateComputePipeline(const ComputePipelineDesc& _desc, VkDevice _device)
    {
        VkShaderModule* pModule = m_shaderModules.Get(_desc.m_computeStage.m_shaderModule.m_handle);
        VkPipelineLayout* pLayout = m_pipelineLayouts.Get(_desc.m_pipelineLayout.m_handle);

        KE_ASSERT_FATAL(pModule != nullptr && pLayout != nullptr);

        const VkComputePipelineCreateInfo pipelineCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = VkPipelineShaderStageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = *pModule,
                .pName = _desc.m_computeStage.m_entryPoint.c_str(),
            },
            .layout = *pLayout,
        };

        const GenPool::Handle handle = m_pipelines.Allocate();

        VkAssert(vkCreateComputePipelines(
            _device,
            VK_NULL_HANDLE,
            1,
            &pipelineCreateInfo,
            nullptr,
            m_pipelines.Get(handle)
            ));

        return { handle };
    }

    bool VkResources::DestroyComputePipeline(ComputePipelineHandle _pipeline, VkDevice _device)
    {
        VkPipeline pipeline;
        if (m_pipelines.Free(_pipeline.m_handle, &pipeline))
        {
            vkDestroyPipeline(_device, pipeline, nullptr);
            return true;
        }
        return false;
    }

    VkImageView VkResources::CreateImageView(
        VkDevice _device,
        VkImage _image,
        VkImageViewType _viewType,
        VkFormat _format,
        VkComponentMapping _componentMapping,
        VkImageAspectFlags _aspectFlags,
        u32 _mipStart,
        u32 _mipCount,
        u32 _arrayStart,
        u32 _arraySize)
    {
        KE_ZoneScopedFunction("VkResources::CreateImageView");

        VkImageViewCreateInfo imageViewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = 0,
            .image = _image,
            .viewType = _viewType,
            .format = _format,
            .components = _componentMapping,
            .subresourceRange = {
                .aspectMask = _aspectFlags,
                .baseMipLevel = _mipStart,
                .levelCount = _mipCount,
                .baseArrayLayer = _arrayStart,
                .layerCount = _arraySize,
            }
        };

        VkImageView imageView = nullptr;
        VkAssert(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &imageView));

        return imageView;
    }
}
