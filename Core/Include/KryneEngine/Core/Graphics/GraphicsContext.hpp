/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "Texture.hpp"

namespace KryneEngine
{
    namespace GraphicsCommon
    {
        struct ApplicationInfo;
    }

    struct BufferCopyParameters;
    struct BufferCreateDesc;
    struct BufferMapping;
    struct BufferMemoryBarrier;
    struct BufferSpan;
    struct Color;
    struct ComputePipelineDesc;
    struct DescriptorSetDesc;
    struct DescriptorSetWriteInfo;
    struct DrawIndexedInstancedDesc;
    struct DrawInstancedDesc;
    struct GlobalMemoryBarrier;
    struct GraphicsPipelineDesc;
    struct PipelineLayoutDesc;
    struct RenderTargetViewDesc;
    struct RenderPassDesc;
    struct TextureViewDesc;
    struct TextureMemoryBarrier;
    struct Viewport;

    class TracyGpuProfilerContext;
    class Window;

    using CommandListHandle = void*;

    class GraphicsContext
    {
    public:
        static GraphicsContext* Create(
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window,
            AllocatorInstance _allocator);
        static void Destroy(GraphicsContext* _context);

        [[nodiscard]] inline u64 GetFrameId() const
        {
            return m_frameId;
        }
        [[nodiscard]] virtual u8 GetFrameContextCount() const = 0;
        [[nodiscard]] inline u8 GetCurrentFrameContextIndex() const
        {
            return m_frameId % GetFrameContextCount();
        }

        bool EndFrame();
        inline void WaitForLastFrame() const { WaitForFrame(m_frameId - 1); }
        [[nodiscard]] virtual bool IsFrameExecuted(u64 _frameId) const = 0;

        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

        [[nodiscard]] const GraphicsCommon::ApplicationInfo& GetApplicationInfo() const { return m_appInfo; }
        [[nodiscard]] static const char* GetShaderFileExtension();

        [[nodiscard]] virtual bool HasDedicatedTransferQueue() const = 0;
        [[nodiscard]] virtual bool HasDedicatedComputeQueue() const = 0;

        [[nodiscard]] TracyGpuProfilerContext* GetProfilerContext() const { return m_profilerContext; }

    protected:

        GraphicsContext(
            AllocatorInstance _allocator,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window);

        GraphicsCommon::ApplicationInfo m_appInfo;
        AllocatorInstance m_allocator;
        Window* m_window;

        static constexpr u64 kInitialFrameId = 1;
        u64 m_frameId;

        TracyGpuProfilerContext* m_profilerContext = nullptr;

        virtual void InternalEndFrame() = 0;
        virtual void WaitForFrame(u64 _frameId) const = 0;

    public:
        /**
         * @brief Tries to resize the swap chain associated with the specified window.
         *
         * @details
         * This method adjusts the swap chain's dimensions to match the updated size of the provided window.
         * It is typically called in response to a window resize event or similar scenarios where the
         * rendering surface needs to accommodate a new size.
         *
         * The process may fail to resize the swap chain because there is another resizing ongoing.
         *
         * @param _window A pointer to the window associated with the swap chain to be resized.
         *
         * @return Returns true if the swap chain was successfully resized; otherwise, false.
         */
        virtual bool ResizeSwapChain(Window* _window) = 0;

        [[nodiscard]] virtual BufferHandle CreateBuffer(const BufferCreateDesc& _desc) = 0;
        [[nodiscard]] virtual bool NeedsStagingBuffer(BufferHandle _buffer) = 0;
        virtual bool DestroyBuffer(BufferHandle _bufferHandle) = 0;

        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureCreateDesc& _createDesc);
        [[nodiscard]] virtual eastl::vector<TextureMemoryFootprint> FetchTextureSubResourcesMemoryFootprints(
            const TextureDesc& _desc) = 0;
        [[nodiscard]] virtual BufferHandle CreateStagingBuffer(
            const TextureDesc& _createDesc,
            const eastl::span<const TextureMemoryFootprint>& _footprints) = 0;
        virtual bool DestroyTexture(TextureHandle _handle) = 0;

        [[nodiscard]] virtual TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc);
        virtual bool DestroyTextureView(TextureViewHandle _handle) = 0;

        [[nodiscard]] virtual SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc) = 0;
        virtual bool DestroySampler(SamplerHandle _sampler) = 0;

        [[nodiscard]] virtual BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc) = 0;
        virtual bool DestroyBufferView(BufferViewHandle _handle) = 0;

        [[nodiscard]] virtual RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc) = 0;
        virtual bool DestroyRenderTargetView(RenderTargetViewHandle _handle) = 0;

        [[nodiscard]] virtual RenderTargetViewHandle GetPresentRenderTargetView(u8 _swapChainIndex) = 0;
        [[nodiscard]] virtual TextureHandle GetPresentTexture(u8 _swapChainIndex) = 0;
        [[nodiscard]] virtual u32 GetCurrentPresentImageIndex() const = 0;
        [[nodiscard]] virtual uint2 GetPresentFrameBufferSize() = 0;

        [[nodiscard]] virtual RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) = 0;
        virtual bool DestroyRenderPass(RenderPassHandle _handle) = 0;

        virtual CommandListHandle BeginGraphicsCommandList() = 0;
        virtual void EndGraphicsCommandList(CommandListHandle _commandList) = 0;

        virtual void BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _handle) = 0;
        virtual void EndRenderPass(CommandListHandle _commandList) = 0;

        virtual void BeginComputePass(CommandListHandle _commandList) = 0;
        virtual void EndComputePass(CommandListHandle _commandList) = 0;

        virtual void SetTextureData(
            CommandListHandle _commandList,
            BufferHandle _stagingBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subResourceIndex,
            const void* _data) = 0;
        virtual void SetTextureRegionData(
            CommandListHandle _commandList,
            BufferSpan _srcBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subresourceIndex,
            const uint3& _regionOffset,
            const uint3& _regionSize) = 0;

        virtual void MapBuffer(BufferMapping& _mapping) = 0;
        virtual void UnmapBuffer(BufferMapping& _mapping) = 0;

        virtual void CopyBuffer(CommandListHandle _commandList, const BufferCopyParameters& _params) = 0;

        [[nodiscard]] static bool SupportsNonGlobalBarriers();
        virtual void PlaceMemoryBarriers(
            CommandListHandle _commandList,
            const eastl::span<const GlobalMemoryBarrier>& _globalMemoryBarriers,
            const eastl::span<const BufferMemoryBarrier>& _bufferMemoryBarriers,
            const eastl::span<const TextureMemoryBarrier>& _textureMemoryBarriers) = 0;

        [[nodiscard]] static bool RenderPassNeedsUsageDeclaration();
        [[nodiscard]] static bool ComputePassNeedsUsageDeclaration();
        virtual void DeclarePassTextureViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const TextureViewHandle>& _textures,
            KryneEngine::TextureViewAccessType _accessType) = 0;
        virtual void DeclarePassBufferViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const BufferViewHandle>& _buffers,
            BufferViewAccessType _accessType) = 0;

        [[nodiscard]] virtual ShaderModuleHandle RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize) = 0;
        [[nodiscard]] virtual DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetDesc& _desc, u32* _bindingIndices) = 0;
        [[nodiscard]] virtual DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout) = 0;
        [[nodiscard]] virtual PipelineLayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& _desc) = 0;
        [[nodiscard]] virtual GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc) = 0;
        virtual bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline) = 0;
        virtual bool DestroyPipelineLayout(PipelineLayoutHandle _layout) = 0;
        virtual bool DestroyDescriptorSet(DescriptorSetHandle _set) = 0;
        virtual bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout) = 0;
        virtual bool FreeShaderModule(ShaderModuleHandle _module) = 0;

        [[nodiscard]] virtual ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc) = 0;
        virtual bool DestroyComputePipeline(ComputePipelineHandle _pipeline) = 0;

        virtual void UpdateDescriptorSet(
            DescriptorSetHandle _descriptorSet,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame) = 0;

        virtual void SetViewport(CommandListHandle _commandList, const Viewport& _viewport) = 0;
        virtual void SetScissorsRect(CommandListHandle _commandList, const Rect& _rect) = 0;
        virtual void SetIndexBuffer(CommandListHandle _commandList, const BufferSpan& _indexBufferView, bool _isU16) = 0;
        virtual void SetVertexBuffers(CommandListHandle _commandList, const eastl::span<const BufferSpan>& _bufferViews) = 0;
        virtual void SetGraphicsPipeline(CommandListHandle _commandList, GraphicsPipelineHandle _graphicsPipeline) = 0;
        virtual void SetGraphicsPushConstant(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            const eastl::span<const u32>& _data,
            u32 _index,
            u32 _offset) = 0;
        virtual void SetGraphicsDescriptorSetsWithOffset(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets,
            u32 _offset) = 0;
        void SetGraphicsDescriptorSets(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets)
        {
            SetGraphicsDescriptorSetsWithOffset(_commandList, _layout, _sets, 0);
        }

        virtual void DrawInstanced(CommandListHandle _commandList, const DrawInstancedDesc& _desc) = 0;
        virtual void DrawIndexedInstanced(CommandListHandle _commandList, const DrawIndexedInstancedDesc& _desc) = 0;

        virtual void SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline) = 0;
        virtual void SetComputeDescriptorSetsWithOffset(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const DescriptorSetHandle> _sets,
            u32 _offset) = 0;
        void SetComputeDescriptorSets(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const DescriptorSetHandle> _sets)
        {
            SetComputeDescriptorSetsWithOffset(_commandList, _layout, _sets, 0);
        }
        virtual void SetComputePushConstant(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const u32> _data) = 0;

        virtual void Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3 _threadGroupSize) = 0;

        /**
         * @brief Inserts a debug marker into the command list to assist with GPU profiling and debugging.
         *
         * @details
         * The debug marker can be used to annotate specific regions of the command list with a name
         * and optional color to make debugging or performance analysis easier. This allows graphics
         * tools to display meaningful annotations in the GPU command timeline.
         *
         * Make sure to pop the marker using `PopDebugMarker`
         *
         * @param _commandList The handle to the command list where the debug marker will be pushed.
         * @param _markerName A null-terminated string representing the name of the debug marker.
         * @param _color The color to associate with the debug marker. It is used for visualization in compatible debugging tools.
         *
         * @note
         * This API might not utilize color information on platforms that do not support it (e.g., Metal).
         */
        virtual void PushDebugMarker(
            CommandListHandle _commandList,
            const eastl::string_view& _markerName,
            const Color& _color) = 0;

        /**
         * @brief Removes the most recently pushed debug marker from the command list.
         *
         * @details
         * This function is used to end a region of GPU commands that was previously annotated with a debug marker using
         * `PushDebugMarker`. It ensures that debug markers are properly balanced for profiling and debugging purposes.
         *
         * @param _commandList The handle to the command list from which the debug marker should be removed.
         */
        virtual void PopDebugMarker(
            CommandListHandle _commandList) = 0;

        /**
         * @brief Inserts a debug marker directly into the command list for GPU debugging and profiling purposes.
         *
         * @details
         * This function adds an inline annotation within the command list to aid in GPU debugging and performance analysis.
         * The marker is associated with a name and an optional color that can be used by compatible debugging tools
         * to display meaningful visual annotations in the GPU command timeline.
         *
         * @param _commandList The handle to the command list where the debug marker will be inserted.
         * @param _markerName A null-terminated string describing the marker to be inserted.
         * @param _color The color to associate with the debug marker for better visualization in compatible tools.
         *
         * @note
         * Unlike `PushDebugMarker` and `PopDebugMarker`, this does not create a region but rather a single-point annotation.
         * This API might not utilize color information on platforms that do not support it (e.g., Metal).
         *
         * @warning
         * Due to API restriction (see Metal), this should only be used during compute or render passes
         */
        virtual void InsertDebugMarker(
            CommandListHandle _commandList,
            const eastl::string_view& _markerName,
            const Color& _color) = 0;

        /**
         * @brief Calibrates the time synchronization between CPU and GPU clocks.
         *
         * @details
         * This function ensures accurate timing and synchronization between the CPU and GPU. It is particularly useful
         * for profiling or scenarios where precise time alignment between the two processing units is necessary for
         * debugging or performance analysis.
         *
         * This is automatically called on context creation, and should be called again sparringly, as it has a
         * non-insignificant performance overhead.
         * Calling it every N frames for synchronicity should be fine.
         *
         * @note
         * The implementation of this function is platform-specific and may use various APIs or techniques depending on
         * the underlying hardware and driver support.
         */
        virtual void CalibrateCpuGpuClocks() = 0;

        virtual TimestampHandle PutTimestamp(CommandListHandle _commandList) = 0;
        virtual u64 GetResolvedTimestamp(TimestampHandle _timestamp) const = 0;
        virtual eastl::span<const u64> GetResolvedTimestamps(u64 _frameId) const = 0;
    };
}


