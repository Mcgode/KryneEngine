/**
 * @file
 * @author Max Godefroy
 * @date 11/03/2023.
 */

#pragma once

#include <EASTL/unique_ptr.h>

#include "Graphics/DirectX12/Dx12DescriptorSetManager.hpp"
#include "Graphics/DirectX12/Dx12FrameContext.hpp"
#include "Graphics/DirectX12/Dx12Headers.hpp"
#include "Graphics/DirectX12/Dx12Resources.h"
#include "Graphics/DirectX12/Dx12SwapChain.hpp"
#include "Graphics/DirectX12/Dx12Types.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class Window;
    class Dx12SwapChain;

    struct BufferCopyParameters;
    struct BufferMapping;
    struct BufferSpan;
    struct DrawIndexedInstancedDesc;
    struct DrawInstancedDesc;
    struct Viewport;

    class Dx12GraphicsContext final: public GraphicsContext
    {
    public:
        explicit Dx12GraphicsContext(
            AllocatorInstance _allocator,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window);

        virtual ~Dx12GraphicsContext();

        [[nodiscard]] u8 GetFrameContextCount() const override { return m_frameContextCount; }

        [[nodiscard]] bool IsFrameExecuted(u64 _frameId) const override;

        [[nodiscard]] bool HasDedicatedTransferQueue() const override;
        [[nodiscard]] bool HasDedicatedComputeQueue() const override;

    protected:
        void InternalEndFrame() override;
        void WaitForFrame(u64 _frameId) const override;

    private:
        ComPtr<ID3D12Device> m_device;

        ComPtr<ID3D12CommandQueue> m_directQueue;
        ComPtr<ID3D12CommandQueue> m_computeQueue;
        ComPtr<ID3D12CommandQueue> m_copyQueue;

        Dx12SwapChain m_swapChain;

        u8 m_frameContextCount;
        DynamicArray<Dx12FrameContext> m_frameContexts;
        ComPtr<ID3D12Fence> m_frameFence;
        HANDLE m_frameFenceEvent;

        DWORD m_validationLayerMessageCallbackHandle = 0;

        bool m_enhancedBarriersEnabled = false;

        void _CreateDevice(IDXGIFactory4* _factory4);
        void _FindAdapter(IDXGIFactory4* _factory, IDXGIAdapter1** _adapter);

        void _CreateCommandQueues();

    public:
        [[nodiscard]] BufferHandle CreateBuffer(const BufferCreateDesc& _desc) override;
        [[nodiscard]] bool NeedsStagingBuffer(BufferHandle _buffer) override;
        bool DestroyBuffer(BufferHandle _buffer) override;

        [[nodiscard]] TextureHandle CreateTexture(const TextureCreateDesc& _createDesc) override;
        [[nodiscard]] eastl::vector<TextureMemoryFootprint> FetchTextureSubResourcesMemoryFootprints(
            const TextureDesc& _desc) override;
        [[nodiscard]] BufferHandle CreateStagingBuffer(
            const TextureDesc& _createDesc,
            const eastl::span<const TextureMemoryFootprint>& _footprints) override;
        bool DestroyTexture(TextureHandle _texture) override;

        [[nodiscard]] TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc) override;
        bool DestroyTextureView(TextureViewHandle _textureView) override;

        [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc) override;
        bool DestroySampler(SamplerHandle _sampler) override;

        [[nodiscard]] BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc) override;
        bool DestroyBufferView(BufferViewHandle _handle) override;

        [[nodiscard]] RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc) override;
        bool DestroyRenderTargetView(RenderTargetViewHandle _rtv) override;

        [[nodiscard]] RenderTargetViewHandle GetPresentRenderTargetView(u8 _index) override;
        [[nodiscard]] TextureHandle GetPresentTexture(u8 _swapChainIndex) override;
        [[nodiscard]] u32 GetCurrentPresentImageIndex() const override;

        RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) override;
        bool DestroyRenderPass(RenderPassHandle _renderPass) override;

        CommandListHandle BeginGraphicsCommandList() override;
        void EndGraphicsCommandList(CommandListHandle _commandList) override;

        void BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _renderPass) override;
        void EndRenderPass(CommandListHandle _commandList) override;

        void BeginComputePass(CommandListHandle _commandList) override {}
        void EndComputePass(CommandListHandle _commandList) override {}

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

        void SetViewport(CommandListHandle  _commandList, const Viewport& _viewport) override;
        void SetScissorsRect(CommandListHandle  _commandList, const Rect& _rect) override;
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

        void Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3 _threadGroupSize) override;

        void PushDebugMarker(
            CommandListHandle _commandList,
            const eastl::string_view &_markerName,
            const Color &_color) override;
        void PopDebugMarker(CommandListHandle _commandList) override;
        void InsertDebugMarker(
            CommandListHandle _commandList,
            const eastl::string_view &_markerName,
            const Color &_color) override;

        void CalibrateCpuGpuClocks() override;
        TimestampHandle PutTimestamp(CommandListHandle _commandList) override;
        u64 GetResolvedTimestamp(TimestampHandle _timestamp) const override;
        eastl::span<const u64> GetResolvedTimestamps(u64 _frameId) const override;

    private:
        Dx12Resources m_resources;
        Dx12DescriptorSetManager m_descriptorSetManager;
        RenderPassHandle m_currentRenderPass;
    };
} // KryneEngine