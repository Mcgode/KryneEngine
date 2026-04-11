/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include <EASTL/vector_set.h>

#include "Graphics/Vulkan/CommonStructures.hpp"
#include "Graphics/Vulkan/VkDescriptorSetManager.hpp"
#include "Graphics/Vulkan/VkFrameContext.hpp"
#include "Graphics/Vulkan/VkResources.hpp"
#include "Graphics/Vulkan/VkSurface.hpp"
#include "Graphics/Vulkan/VkSwapChain.hpp"
#include "Graphics/Vulkan/VkTypes.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"

namespace KryneEngine
{
    class VkDebugHandler;
    class Window;

    struct BufferMapping;
    struct BufferCopyParameters;
    struct BufferSpan;
    struct DrawIndexedInstancedDesc;
    struct DrawInstancedDesc;
    struct Viewport;

    class VkGraphicsContext final: public GraphicsContext
    {
        static_assert(sizeof(CommandList) == sizeof(CommandListHandle));

    public:
        explicit VkGraphicsContext(
            AllocatorInstance _allocator,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window);

        virtual ~VkGraphicsContext();

        [[nodiscard]] u8 GetFrameContextCount() const override { return m_frameContextCount; }

        [[nodiscard]] bool IsFrameExecuted(u64 _frameId) const override;

        [[nodiscard]] bool HasDedicatedTransferQueue() const override;
        [[nodiscard]] bool HasDedicatedComputeQueue() const override;

    protected:
        void InternalEndFrame() override;

        void WaitForFrame(u64 _frameId) const override;

    private:
        VkInstance m_instance;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;

        VkSurface m_surface;
        VkSwapChain m_swapChain;

        VkCommonStructures::QueueIndices m_queueIndices {};
        VkQueue m_graphicsQueue {};
        VkQueue m_transferQueue {};
        VkQueue m_computeQueue {};
        VkQueue m_presentQueue {};

        u32 m_optimalRowPitchAlignment;

        bool m_debugUtils = false;
        bool m_debugMarkers = false;
        bool m_supportsTimestampQueries = false;
        bool m_supportsTimestampCalibration = false;
        double m_gpuTimestampPeriod = 0;
        u64 m_cpuTimestampOffset = 0;
        VkTimeDomainKHR m_cpuTimeDomain;
        mutable u64 m_lastResolvedFrame = ~0ull;
        PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelExt = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelExt = nullptr;
        PFN_vkCmdInsertDebugUtilsLabelEXT m_vkCmdInsertDebugUtilsLabelExt = nullptr;
        PFN_vkGetCalibratedTimestampsKHR m_vkGetCalibratedTimestampsKHR = nullptr;

        bool m_synchronization2 = false;
        PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR = nullptr;

#if !defined(KE_override)
        eastl::shared_ptr<VkDebugHandler> m_debugHandler;
#endif

        u8 m_frameContextCount;
        DynamicArray<VkFrameContext> m_frameContexts;

        static bool PrepareValidationLayers(VkInstanceCreateInfo& _createInfo);

        eastl::vector<const char*> RetrieveRequiredExtensionNames(const GraphicsCommon::ApplicationInfo& _appInfo, bool _validationLayersEnabled);
        void _RetrieveOptionalExtensionNames(
                eastl::vector<const char *>& _currentList,
                const DynamicArray<VkExtensionProperties> &_availableExtensions,
                const GraphicsCommon::ApplicationInfo &_appInfo);

        static VkDebugUtilsMessengerCreateInfoEXT _PopulateDebugCreateInfo(void *_userData);

        void _SetupValidationLayersCallback();

        [[nodiscard]] eastl::vector_set<eastl::string> _GetRequiredDeviceExtensions() const;
        void _SelectPhysicalDevice();

        bool _SelectQueues(
            const GraphicsCommon::ApplicationInfo &_appInfo,
            const VkPhysicalDevice &_physicalDevice,
            const VkSurfaceKHR &_surface,
            VkCommonStructures::QueueIndices &_indices);

        void _CreateDevice();
        void _RetrieveQueues(const VkCommonStructures::QueueIndices &_queueIndices);

    public:
        bool ResizeSwapChain(Window* _window) override;

        [[nodiscard]] BufferHandle CreateBuffer(const BufferCreateDesc& _desc) override;
        [[nodiscard]] bool NeedsStagingBuffer(BufferHandle _buffer) override;
        bool DestroyBuffer(BufferHandle _bufferHandle) override;

        [[nodiscard]] TextureHandle CreateTexture(const TextureCreateDesc& _createDesc) override;
        [[nodiscard]] eastl::vector<TextureMemoryFootprint> FetchTextureSubResourcesMemoryFootprints(
            const TextureDesc& _desc) override;
        [[nodiscard]] BufferHandle CreateStagingBuffer(
            const TextureDesc& _createDesc,
            const eastl::span<const TextureMemoryFootprint>& _footprints) override;
        bool DestroyTexture(TextureHandle _handle) override;

        [[nodiscard]] TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc) override;
        bool DestroyTextureView(TextureViewHandle _handle) override;

        [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc) override;
        bool DestroySampler(SamplerHandle _sampler) override;

        [[nodiscard]] BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc) override;
        bool DestroyBufferView(BufferViewHandle _handle) override;

        [[nodiscard]] RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc) override;
        bool DestroyRenderTargetView(RenderTargetViewHandle _handle) override;

        [[nodiscard]] RenderTargetViewHandle GetPresentRenderTargetView(u8 _index) override;
        [[nodiscard]] TextureHandle GetPresentTexture(u8 _swapChainIndex) override;
        [[nodiscard]] u32 GetCurrentPresentImageIndex() const override;
        [[nodiscard]] uint2 GetPresentFrameBufferSize() override;

        [[nodiscard]] RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) override;
        bool DestroyRenderPass(RenderPassHandle _handle) override;

        CommandListHandle BeginGraphicsCommandList() override;
        void EndGraphicsCommandList(CommandListHandle _commandList) override;

        void BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _renderPass) override;
        void EndRenderPass(CommandListHandle _commandList) override;

        void BeginComputePass(CommandListHandle _commandList) override {};
        void EndComputePass(CommandListHandle _commandList) override {};

        void SetTextureData(
            CommandListHandle _commandList,
            BufferHandle _stagingBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subResourceIndex,
            const void* _data) override;
        void SetTextureRegionData(
            CommandListHandle _commandList,
            BufferSpan _srcBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subresourceIndex,
            const uint3& _regionOffset,
            const uint3& _regionSize) override;

        void MapBuffer(BufferMapping& _mapping) override;
        void UnmapBuffer(BufferMapping& _mapping) override;
        void CopyBuffer(CommandListHandle _commandList, const BufferCopyParameters& _params) override;

        void PlaceMemoryBarriers(
            CommandListHandle _commandList,
            const eastl::span<const GlobalMemoryBarrier>& _globalMemoryBarriers,
            const eastl::span<const BufferMemoryBarrier>& _bufferMemoryBarriers,
            const eastl::span<const TextureMemoryBarrier>& _textureMemoryBarriers) override;

        void DeclarePassTextureViewUsage(
            CommandListHandle,
            const eastl::span<const TextureViewHandle>&,
            TextureViewAccessType) override {}
        void DeclarePassBufferViewUsage(
            CommandListHandle,
            const eastl::span<const BufferViewHandle>&,
            BufferViewAccessType) override {}

        [[nodiscard]] ShaderModuleHandle RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize) override;
        [[nodiscard]] DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetDesc& _desc, u32* _bindingIndices) override;
        [[nodiscard]] DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout) override;
        [[nodiscard]] PipelineLayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& _desc) override;
        [[nodiscard]] GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc) override;
        bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline) override;
        bool DestroyPipelineLayout(PipelineLayoutHandle _layout) override;
        bool DestroyDescriptorSet(DescriptorSetHandle _set) override;
        bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout) override;
        bool FreeShaderModule(ShaderModuleHandle _module) override;

        [[nodiscard]] ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc) override;
        bool DestroyComputePipeline(ComputePipelineHandle _pipeline) override;

        void UpdateDescriptorSet(
            DescriptorSetHandle _descriptorSet,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame) override;

        void SetViewport(CommandListHandle _commandList, const Viewport& _viewport) override;
        void SetScissorsRect(CommandListHandle _commandList, const Rect& _rect) override;
        void SetIndexBuffer(CommandListHandle _commandList, const BufferSpan& _indexBufferView, bool _isU16) override;
        void SetVertexBuffers(CommandListHandle _commandList, const eastl::span<const BufferSpan>& _bufferViews) override;
        void SetGraphicsPipeline(CommandListHandle _commandList, GraphicsPipelineHandle _graphicsPipeline) override;
        void SetGraphicsPushConstant(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            const eastl::span<const u32>& _data,
            u32 _index,
            u32 _offset) override;
        void SetGraphicsDescriptorSetsWithOffset(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets,
            u32 _offset) override;
        void DrawInstanced(CommandListHandle _commandList, const DrawInstancedDesc& _desc) override;
        void DrawIndexedInstanced(CommandListHandle _commandList, const DrawIndexedInstancedDesc& _desc) override;

    void SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline) override;
    void SetComputeDescriptorSetsWithOffset(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const DescriptorSetHandle> _sets,
        u32 _offset) override;
    void SetComputePushConstant(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const u32> _data) override;

    void Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3) override;

    void PushDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color& _color) override;
    void PopDebugMarker(
        CommandListHandle _commandList) override;
    void InsertDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color& _color) override;

    void CalibrateCpuGpuClocks() override;
    TimestampHandle PutTimestamp(CommandListHandle _commandList) override;
    u64 GetResolvedTimestamp(TimestampHandle _timestamp) const override;
    eastl::span<const u64> GetResolvedTimestamps(u64 _frameId) const override;

private:
        VkResources m_resources;
        VkDescriptorSetManager m_descriptorSetManager;
    };
}


