/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2024.
 */

#pragma once

#include "Graphics/Metal/MetalArgumentBufferManager.hpp"
#include "Graphics/Metal/MetalHeaders.hpp"
#include "Graphics/Metal/MetalResources.hpp"
#include "Graphics/Metal/MetalSwapChain.hpp"
#include "Graphics/Metal/MetalTypes.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Graphics/ShaderPipeline.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

namespace KryneEngine
{
    class ByteUploader;
    struct DrawIndexedInstancedDesc;
    struct DrawInstancedDesc;
    struct RenderPassDesc;
    struct RenderTargetViewDesc;
    struct TextureCreateDesc;
    struct TextureViewDesc;
    struct Viewport;

    class MetalFrameContext;
    class MetalSwapChain;
    class Window;

    class MetalGraphicsContext final: public GraphicsContext
    {
    public:
        MetalGraphicsContext(
            AllocatorInstance _allocator,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window);

        ~MetalGraphicsContext();

        [[nodiscard]] u8 GetFrameContextCount() const override { return m_frameContextCount; }

        [[nodiscard]] bool IsFrameExecuted(u64 _frameId) const override;

        [[nodiscard]] bool HasDedicatedTransferQueue() const override;
        [[nodiscard]] bool HasDedicatedComputeQueue() const override;

    private:
        NsPtr<MTL::Device> m_device;
        MetalSwapChain m_swapChain;

        NsPtr<MTL4::CommandQueue> m_graphicsQueue;
        NsPtr<MTL4::CommandQueue> m_computeQueue;
        NsPtr<MTL4::CommandQueue> m_ioQueue;

        u8 m_frameContextCount;
        DynamicArray<MetalFrameContext> m_frameContexts;

        bool m_calibrateCpuGpuClocks = false;
        bool m_supportsDrawBoundarySampling = false;
        bool m_supportsDispatchBoundarySampling = false;
        bool m_supportsBlitBoundarySampling = false;
        TimestampConversion m_timestampConversion;
        mutable u64 m_lastResolvedFrameId = ~0ull;

    protected:
        void InternalEndFrame() override;
        void WaitForFrame(u64 _frameId) const override;

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

        [[nodiscard]] RenderTargetViewHandle GetPresentRenderTargetView(u8 _swapChainIndex) override;
        [[nodiscard]] TextureHandle GetPresentTexture(u8 _swapChainIndex) override;
        [[nodiscard]] u32 GetCurrentPresentImageIndex() const override;
        [[nodiscard]] uint2 GetPresentFrameBufferSize() override;

        [[nodiscard]] RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) override;
        bool DestroyRenderPass(RenderPassHandle _handle) override;

        CommandListHandle BeginGraphicsCommandList() override;
        void EndGraphicsCommandList(CommandListHandle _commandList) override;

        [[nodiscard]] RenderCommandEncoderHandle BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _handle) override;
        void EndRenderPass(RenderCommandEncoderHandle _renderCommandEncoder) override;

        ComputeCommandEncoderHandle BeginComputePass(CommandListHandle _commandList) override;
        void EndComputePass(ComputeCommandEncoderHandle _computeEncoder) override;

        TransferCommandEncoderHandle BeginTransferPass(CommandListHandle _commandList) override;
        void EndTransferPass(TransferCommandEncoderHandle _utilEncoder) override;

        void SetTextureData(
            TransferCommandEncoderHandle _transferEncoder,
            BufferHandle _stagingBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subResourceIndex,
            const void* _data) override;
        void SetTextureRegionData(
            TransferCommandEncoderHandle _transferEncoder,
            BufferSpan _srcBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subresourceIndex,
            const uint3& _regionOffset,
            const uint3& _regionSize) override;

        void MapBuffer(BufferMapping& _mapping) override;
        void UnmapBuffer(BufferMapping& _mapping) override;

        void CopyBuffer(TransferCommandEncoderHandle _transferEncoder, const BufferCopyParameters& _params) override;;

        void PlaceMemoryBarriers(CommandEncoderHandle _commandEncoder, const MemoryBarriers& _barriers) override;

        void DeclarePassTextureViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const TextureViewHandle>& _textures,
            TextureViewAccessType _accessType) override;
        void DeclarePassBufferViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const BufferViewHandle>& _buffers,
            BufferViewAccessType _accessType) override;

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

        void SetViewport(RenderCommandEncoderHandle _renderEncoder, const Viewport& _viewport) override;
        void SetScissorsRect(RenderCommandEncoderHandle _renderEncoder, const Rect& _rect) override;
        void SetIndexBuffer(
            RenderCommandEncoderHandle _renderEncoder, const BufferSpan& _indexBufferView, bool _isU16) override;
        void SetVertexBuffers(
            RenderCommandEncoderHandle _renderEncoder, const eastl::span<const BufferSpan>& _bufferViews) override;
        void SetGraphicsPipeline(
            RenderCommandEncoderHandle _renderEncoder, GraphicsPipelineHandle _graphicsPipeline) override;
        void SetGraphicsPushConstant(
            RenderCommandEncoderHandle _renderEncoder,
            PipelineLayoutHandle _layout,
            const eastl::span<const u32>& _data,
            u32 _index,
            u32 _offset) override;
        void SetGraphicsDescriptorSetsWithOffset(
            RenderCommandEncoderHandle _renderEncoder,
            PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets,
            u32 _offset) override;
        void DrawInstanced(RenderCommandEncoderHandle _renderEncoder, const DrawInstancedDesc& _desc) override;
        void DrawIndexedInstanced(RenderCommandEncoderHandle _renderEncoder, const DrawIndexedInstancedDesc& _desc) override;

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

        void Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3 _threadGroupSize) override;

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
        MetalResources m_resources;
        MetalArgumentBufferManager m_argumentBufferManager;
        ByteUploader* m_byteUploader = nullptr;
    };
} // KryneEngine
