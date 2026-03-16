/**
 * @file
 * @author Max Godefroy
 * @date 19/02/2024.
 */

#pragma once

#include <EASTL/shared_ptr.h>
#include <vk_mem_alloc.h>

#include "Graphics/Vulkan/VkHeaders.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

namespace KryneEngine
{
    namespace GraphicsCommon
    {
        struct ApplicationInfo;
    }

    struct BufferViewDesc;
    struct BufferCreateDesc;
    struct RenderTargetViewDesc;
    struct RenderPassDesc;
    struct TextureViewDesc;
    struct BufferCreateDesc;
    struct TextureViewDesc;
    struct RenderTargetViewDesc;

    class VkDebugHandler;
    class VkDescriptorSetManager;

    class VkResources
    {
    public:
        friend class VkGraphicsContext;

        struct BufferColdData
        {
            VmaAllocation m_allocation;
            VmaAllocationInfo m_info;
        };
        GenerationalPool<VkBuffer, BufferColdData> m_buffers;

        struct TextureColdData
        {
            VmaAllocation m_allocation;
            uint3 m_dimensions;
        };
        GenerationalPool<VkImage, TextureColdData> m_textures;

        GenerationalPool<VkImageView> m_imageViews;
        GenerationalPool<VkSampler> m_samplers;

        struct BufferSpan
        {
            VkBuffer m_buffer;
            size_t m_size;
            size_t m_offset;
        };

        GenerationalPool<BufferSpan> m_bufferViews;

        struct RTVColdData
        {
            VkFormat m_format;
            Size16x2 m_size;
        };
        GenerationalPool<VkImageView, RTVColdData> m_renderTargetViews;

        struct RenderPassData
        {
            VkRenderPass m_renderPass;
            VkFramebuffer m_framebuffer;
            Size16x2 m_size;
            eastl::vector<VkClearValue> m_clearValues;
        };
        GenerationalPool<RenderPassData> m_renderPasses;

        GenerationalPool<VkShaderModule> m_shaderModules;

        struct LayoutColdData
        {
            struct PushConstantData
            {
                u8 m_offset;
                ShaderVisibility m_visibility;
            };
            eastl::array<PushConstantData, 4> m_pushConstants;
        };
        GenerationalPool<VkPipelineLayout, LayoutColdData> m_pipelineLayouts;

        GenerationalPool<VkPipeline> m_pipelines;

#if !defined(KE_FINAL)
        eastl::shared_ptr<VkDebugHandler> m_debugHandler;
#endif

        // Default constructor and destructor, but moved implementation to cpp for .inl linking
        VkResources(AllocatorInstance _allocator);
        ~VkResources();

        void FlushPools();

        void InitAllocator(
            const GraphicsCommon::ApplicationInfo& _appInfo,
            VkDevice _device,
            VkPhysicalDevice _physicalDevice,
            VkInstance _instance);

        void DestroyAllocator();

        [[nodiscard]] BufferHandle CreateBuffer(const BufferCreateDesc& _desc, VkDevice _device);
        [[nodiscard]] BufferHandle CreateStagingBuffer(
            const TextureDesc& _createDesc,
            const eastl::span<const TextureMemoryFootprint>& _footprints,
            VkDevice _device);
        bool DestroyBuffer(BufferHandle _buffer);

        [[nodiscard]] TextureHandle RegisterTexture(VkImage _image, const uint3& _dimensions);
        [[nodiscard]] TextureHandle CreateTexture(const TextureCreateDesc& _desc, VkDevice _device);
        bool ReleaseTexture(TextureHandle _texture, VkDevice _device, bool _free = true);

        [[nodiscard]] TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc, VkDevice _device);
        bool DestroyTextureView(TextureViewHandle _textureView, VkDevice _device);

        [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc, VkDevice _device);
        bool DestroySampler(SamplerHandle _sampler, VkDevice _device);

        [[nodiscard]] BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc, VkDevice _device);
        bool DestroyBufferView(BufferViewHandle _handle, VkDevice _device);

        [[nodiscard]] RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc, VkDevice _device);
        bool FreeRenderTargetView(RenderTargetViewHandle _rtv, VkDevice _device);

        [[nodiscard]] RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc, VkDevice _device);
        bool DestroyRenderPass(RenderPassHandle _renderPass, VkDevice _device);

        [[nodiscard]] ShaderModuleHandle CreateShaderModule(void* _bytecodeData, u64 _bytecodeSize, VkDevice _device);
        bool DestroyShaderModule(ShaderModuleHandle _shaderModule, VkDevice _device);

        [[nodiscard]] PipelineLayoutHandle CreatePipelineLayout(
            const PipelineLayoutDesc& _desc,
            VkDevice _device,
            VkDescriptorSetManager* _setManager);
        bool DestroyPipelineLayout(PipelineLayoutHandle _pipeline, VkDevice _device);

        [[nodiscard]] GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc, VkDevice _device);
        bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline, VkDevice _device);

        [[nodiscard]] ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc, VkDevice _device);
        bool DestroyComputePipeline(ComputePipelineHandle _pipeline, VkDevice _device);

    private:
        VmaAllocator m_allocator;

        [[nodiscard]] VkImageView CreateImageView(
            VkDevice _device,
            VkImage _image,
            VkImageViewType _viewType,
            VkFormat _format,
            VkComponentMapping _componentMapping,
            VkImageAspectFlags _aspectFlags,
            u32 _mipStart,
            u32 _mipCount,
            u32 _arrayStart,
            u32 _arraySize);
    };
} // KryneEngine