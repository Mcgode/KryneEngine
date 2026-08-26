/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#pragma once

#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/BufferView.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"

namespace KryneEngine
{
    namespace GraphicsCommon
    {
        struct ApplicationInfo;
    }

    struct BufferCopyParameters;
    struct BufferCreateDesc;
    struct BufferMapping;
    struct BufferSpan;
    struct Color;
    struct ComputePipelineDesc;
    struct DescriptorSetDesc;
    struct DescriptorSetWriteInfo;
    struct DrawIndexedInstancedDesc;
    struct DrawInstancedDesc;
    struct GraphicsPipelineDesc;
    struct MemoryBarriers;
    struct PipelineLayoutDesc;
    struct RenderTargetViewDesc;
    struct RenderPassDesc;
    struct TextureViewDesc;
    struct Viewport;

    class TracyGpuProfilerContext;
    class Window;

    using CommandListHandle = void*;

    /**
     * @brief Central abstraction for all graphics operations in the engine.
     *
     * @details
     * `GraphicsContext` is an abstract base class providing a unified, low-overhead RHI (Rendering Hardware
     * Interface) across the graphics backends supported by the engine (Vulkan, DirectX 12, Metal).
     *
     * The abstraction is intentionally kept thin. Rather than hiding the complexity of modern graphics APIs
     * behind a high-level facade, `GraphicsContext` exposes the underlying concepts directly — memory barriers,
     * descriptor sets, pipeline layouts, staging buffers, render passes — while normalising the differences
     * between backends into a single, consistent API surface. This keeps the per-call overhead minimal and
     * preserves the caller's ability to make deliberate, performance-sensitive decisions.
     *
     * As a consequence, a significant share of responsibility is delegated to the caller:
     * - **Resource lifetime** is not tracked internally. A resource that is in use during a frame must not be
     *   destroyed until that frame has finished executing on the GPU (see #IsFrameExecuted).
     * - **Synchronisation** between GPU operations must be managed explicitly by inserting memory barriers
     *   (see #PlaceMemoryBarriers) and, where required by the backend, declaring pass resource usages
     *   (see #RenderPassNeedsUsageDeclaration, #DeclarePassTextureViewUsage, #DeclarePassBufferViewUsage).
     * - **Staging** for device-local resources must be handled by the caller: use #NeedsStagingBuffer to
     *   determine whether a buffer requires an intermediate upload buffer, and #CreateStagingBuffer /
     *   #SetTextureData for texture uploads.
     *
     * To achieve feature parity however, some abstractions had to be set up:
     * - **Descriptor sets** are partially abstracted away. The caller still needs to set up layouts and instantiate
     * sets, but the descriptor update process is semi-automated to maintain that parity.
     *
     * Resource creation and destruction operations are thread-safe, thanks to the underlying
     * `GenerationalPool` concurrent read-write support.
     *
     * Command lists are individually thread-local, but you can have multiple command lists active at the same time,
     * allowing for multi-threaded command recording. Commands lists are submitted in order of creation.
     */
    class GraphicsContext
    {
    public:
        /**
         * @brief Creates a new platform-specific `GraphicsContext` instance.
         *
         * @details
         * This factory method selects and initializes the appropriate graphics API implementation
         * (Vulkan, DirectX 12 or Metal) based on `_appInfo`, creates the associated swap chain for `_window`,
         * and performs an initial CPU/GPU clock calibration (see #CalibrateCpuGpuClocks).
         *
         * @param _appInfo Application and engine metadata, along with requested features and display options.
         * @param _window The application's main window, used to create the presentation surface/swap chain.
         * @param _allocator The memory allocator instance to use for all resources created by this context.
         *
         * @return A pointer to the newly created `GraphicsContext`. Ownership is transferred to the caller,
         * who is responsible for calling #Destroy once done with it.
         */
        static GraphicsContext* Create(
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window,
            AllocatorInstance _allocator);

        /**
         * @brief Destroys a `GraphicsContext` previously created with #Create.
         *
         * @param _context The context to destroy. May be `nullptr`, in which case this is a no-op.
         */
        static void Destroy(GraphicsContext* _context);

        /**
         * @brief Retrieves the current frame identifier.
         *
         * @details
         * The frame id starts at #kInitialFrameId and is incremented every time #EndFrame is called.
         *
         * @return The current frame identifier.
         */
        [[nodiscard]] inline u64 GetFrameId() const
        {
            return m_frameId;
        }

        /**
         * @brief Returns the number of frame contexts used for command buffer reuse and multi-buffering.
         */
        [[nodiscard]] virtual u8 GetFrameContextCount() const = 0;

        /**
         * @brief Retrieves the index of the frame context associated with the current frame.
         *
         * @details
         * This index cycles through `[0, GetFrameContextCount())` as frames progress, and can be used to
         * distribute per-frame resources (such as command buffer pools) across multiple threads safely.
         *
         * @return The current frame context index.
         */
        [[nodiscard]] inline u8 GetCurrentFrameContextIndex() const
        {
            return m_frameId % GetFrameContextCount();
        }

        /**
         * @brief Finalizes the current frame and moves on to the next one.
         *
         * @details
         * This submits any remaining recorded work, presents the swap chain image(s), and increments
         * #GetFrameId. It should be called once per application update loop iteration, after all the
         * frame's command lists have been recorded and ended.
         *
         * @return `true` if the frame was successfully ended, `false` otherwise.
         */
        bool EndFrame();

        /**
         * @brief Blocks the calling thread until the previous frame has finished executing on the GPU.
         *
         * @see IsFrameExecuted
         */
        inline void WaitForLastFrame() const { WaitForFrame(m_frameId - 1); }

        /**
         * @brief Checks whether a given frame has finished executing on the GPU.
         *
         * @details
         * This can be used to determine when it is safe to destroy or reuse a resource that was in use
         * during that frame, since resource lifetimes are not tracked internally by the context.
         *
         * @param _frameId The identifier of the frame to check, as returned by #GetFrameId.
         *
         * @return `true` if the frame has finished executing, `false` otherwise.
         */
        [[nodiscard]] virtual bool IsFrameExecuted(u64 _frameId) const = 0;

        /**
         * @brief Retrieves the memory allocator instance used for resource creation by this context.
         */
        [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }

        /**
         * @brief Retrieves the application info structure that was used to create this context.
         */
        [[nodiscard]] const GraphicsCommon::ApplicationInfo& GetApplicationInfo() const { return m_appInfo; }

        /**
         * @brief Retrieves the file extension used for precompiled shader files with the currently selected
         * graphics API (e.g. `.spv` for Vulkan, `.cso` for DirectX 12, or the Metal library extension).
         */
        [[nodiscard]] static const char* GetShaderFileExtension();

        /**
         * @brief Indicates whether the current device exposes a dedicated hardware transfer queue,
         * separate from the graphics queue.
         */
        [[nodiscard]] virtual bool HasDedicatedTransferQueue() const = 0;

        /**
         * @brief Indicates whether the current device exposes a dedicated hardware compute queue,
         * separate from the graphics queue (async compute).
         */
        [[nodiscard]] virtual bool HasDedicatedComputeQueue() const = 0;

        /**
         * @brief Retrieves the Tracy GPU profiler context associated with this graphics context, if any.
         *
         * @return A pointer to the `TracyGpuProfilerContext`, or `nullptr` if GPU profiling is not enabled.
         */
        [[nodiscard]] TracyGpuProfilerContext* GetProfilerContext() const { return m_profilerContext; }

    protected:

        /**
         * @brief Constructs the base `GraphicsContext` state.
         *
         * @param _allocator The memory allocator instance to use for all resources created by this context.
         * @param _appInfo Application and engine metadata, along with requested features and display options.
         * @param _window The application's main window, used to create the presentation surface/swap chain.
         */
        GraphicsContext(
            AllocatorInstance _allocator,
            const GraphicsCommon::ApplicationInfo& _appInfo,
            Window* _window);

        /// @brief Application and engine metadata, along with requested features and display options.
        GraphicsCommon::ApplicationInfo m_appInfo;
        /// @brief The memory allocator instance used for all resources created by this context.
        AllocatorInstance m_allocator;
        /// @brief The application's main window, associated with the presentation surface/swap chain.
        Window* m_window;

        /// @brief The initial value of #m_frameId when the context is created.
        static constexpr u64 kInitialFrameId = 1;
        /// @brief The identifier of the current frame. See #GetFrameId.
        u64 m_frameId;

        /// @brief The Tracy GPU profiler context, if GPU profiling is enabled. See #GetProfilerContext.
        TracyGpuProfilerContext* m_profilerContext = nullptr;

        /**
         * @brief Platform-specific implementation of frame finalization, called by #EndFrame.
         */
        virtual void InternalEndFrame() = 0;

        /**
         * @brief Platform-specific implementation that blocks until the given frame has finished executing.
         *
         * @param _frameId The identifier of the frame to wait for.
         */
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

        /**
         * @brief Creates a GPU buffer according to the given description (size, usage, memory type, etc.).
         *
         * @param _desc The parameters describing the buffer to create.
         *
         * @return A handle to the newly created buffer.
         */
        [[nodiscard]] virtual BufferHandle CreateBuffer(const BufferCreateDesc& _desc) = 0;

        /**
         * @brief Checks whether a buffer requires an intermediate staging buffer to be written to from the CPU.
         *
         * @param _buffer The handle of the buffer to check.
         *
         * @return `true` if a staging buffer is required, `false` if the buffer can be mapped and written to directly.
         */
        [[nodiscard]] virtual bool NeedsStagingBuffer(BufferHandle _buffer) = 0;

        /**
         * @brief Destroys a buffer previously created with #CreateBuffer.
         *
         * @param _bufferHandle The handle of the buffer to destroy.
         *
         * @return `true` if the buffer was successfully destroyed.
         *
         * @note The buffer must not be in use by any pending frame (see #IsFrameExecuted) when destroyed.
         */
        virtual bool DestroyBuffer(BufferHandle _bufferHandle) = 0;

        /**
         * @brief Creates a texture according to the given description (format, dimensions, usage, etc.).
         *
         * @param _createDesc The parameters describing the texture to create.
         *
         * @return A handle to the newly created texture.
         */
        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureCreateDesc& _createDesc);

        /**
         * @brief Retrieves the memory footprint (size, alignment, row pitch, etc.) of each subresource of a texture.
         *
         * @param _desc The description of the texture whose subresources' memory footprints should be computed.
         *
         * @return A vector containing the memory footprint of each subresource of the texture.
         */
        [[nodiscard]] virtual eastl::vector<TextureMemoryFootprint> FetchTextureSubResourcesMemoryFootprints(
            const TextureDesc& _desc) = 0;

        /**
         * @brief Creates a staging buffer suitable for uploading the subresources of a texture.
         *
         * @param _createDesc The description of the texture for which the staging buffer is created.
         * @param _footprints The memory footprints of the texture's subresources, as returned by
         * #FetchTextureSubResourcesMemoryFootprints.
         *
         * @return A handle to the newly created staging buffer.
         */
        [[nodiscard]] virtual BufferHandle CreateStagingBuffer(
            const TextureDesc& _createDesc,
            const eastl::span<const TextureMemoryFootprint>& _footprints) = 0;

        /**
         * @brief Destroys a texture previously created with #CreateTexture.
         *
         * @param _handle The handle of the texture to destroy.
         *
         * @return `true` if the texture was successfully destroyed.
         *
         * @note The texture must not be in use by any pending frame (see #IsFrameExecuted) when destroyed.
         */
        virtual bool DestroyTexture(TextureHandle _handle) = 0;

        /**
         * @brief Creates a view into a texture (e.g. a set of specific subresources, mip levels, or array slices).
         *
         * @param _viewDesc The parameters describing the texture view to create.
         *
         * @return A handle to the newly created texture view.
         */
        [[nodiscard]] virtual TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc);

        /**
         * @brief Destroys a texture view previously created with #CreateTextureView.
         *
         * @param _handle The handle of the texture view to destroy.
         *
         * @return `true` if the texture view was successfully destroyed.
         */
        virtual bool DestroyTextureView(TextureViewHandle _handle) = 0;

        /**
         * @brief Creates a sampler object according to the given description (filtering, addressing modes, etc.).
         *
         * @param _samplerDesc The parameters describing the sampler to create.
         *
         * @return A handle to the newly created sampler.
         */
        [[nodiscard]] virtual SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc) = 0;

        /**
         * @brief Destroys a sampler previously created with #CreateSampler.
         *
         * @param _sampler The handle of the sampler to destroy.
         *
         * @return `true` if the sampler was successfully destroyed.
         */
        virtual bool DestroySampler(SamplerHandle _sampler) = 0;

        /**
         * @brief Creates a view into a buffer, for use as a shader resource.
         *
         * @param _viewDesc The parameters describing the buffer view to create.
         *
         * @return A handle to the newly created buffer view.
         */
        [[nodiscard]] virtual BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc) = 0;

        /**
         * @brief Destroys a buffer view previously created with #CreateBufferView.
         *
         * @param _handle The handle of the buffer view to destroy.
         *
         * @return `true` if the buffer view was successfully destroyed.
         */
        virtual bool DestroyBufferView(BufferViewHandle _handle) = 0;

        /**
         * @brief Creates a render target view, used to bind a texture (or subresource) as a render pass attachment.
         *
         * @param _desc The parameters describing the render target view to create.
         *
         * @return A handle to the newly created render target view.
         */
        [[nodiscard]] virtual RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc) = 0;

        /**
         * @brief Destroys a render target view previously created with #CreateRenderTargetView.
         *
         * @param _handle The handle of the render target view to destroy.
         *
         * @return `true` if the render target view was successfully destroyed.
         */
        virtual bool DestroyRenderTargetView(RenderTargetViewHandle _handle) = 0;

        /**
         * @brief Retrieves the render target view for a given swap chain image index.
         *
         * @param _swapChainIndex The index of the swap chain image, typically #GetCurrentPresentImageIndex.
         *
         * @return A handle to the render target view of the present image.
         */
        [[nodiscard]] virtual RenderTargetViewHandle GetPresentRenderTargetView(u8 _swapChainIndex) = 0;

        /**
         * @brief Retrieves the texture handle for a given swap chain image index.
         *
         * @param _swapChainIndex The index of the swap chain image, typically #GetCurrentPresentImageIndex.
         *
         * @return A handle to the texture of the present image.
         */
        [[nodiscard]] virtual TextureHandle GetPresentTexture(u8 _swapChainIndex) = 0;

        /**
         * @brief Retrieves the index of the swap chain image to be used for the current frame's presentation.
         *
         * @return The current present image index, to be used with #GetPresentRenderTargetView
         * and #GetPresentTexture.
         */
        [[nodiscard]] virtual u32 GetCurrentPresentImageIndex() const = 0;

        /**
         * @brief Retrieves the current size of the presentation frame buffer (i.e. the swap chain images).
         *
         * @return The width and height of the frame buffer, in pixels.
         */
        [[nodiscard]] virtual uint2 GetPresentFrameBufferSize() = 0;

        /**
         * @brief Creates a render pass object, describing a set of attachments and their load/store operations.
         *
         * @param _desc The parameters describing the render pass to create.
         *
         * @return A handle to the newly created render pass.
         */
        [[nodiscard]] virtual RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) = 0;

        /**
         * @brief Destroys a render pass previously created with #CreateRenderPass.
         *
         * @param _handle The handle of the render pass to destroy.
         *
         * @return `true` if the render pass was successfully destroyed.
         */
        virtual bool DestroyRenderPass(RenderPassHandle _handle) = 0;

        /**
         * @brief Begins recording a new command list in the graphics queue.
         *
         * @note
         * When the frame is submitted, the command list order is the same as the order in which their recording began.
         *
         * @return A handle to the newly opened command list, to be used with subsequent recording calls and
         * finalized with #EndGraphicsCommandList.
         */
        virtual CommandListHandle BeginGraphicsCommandList() = 0;

        /**
         * @brief Finalizes a command list previously opened with #BeginGraphicsCommandList to be submitted.
         *
         * @param _commandList The handle of the command list to end.
         */
        virtual void EndGraphicsCommandList(CommandListHandle _commandList) = 0;

        /**
         * @brief Begins a render pass, applying its attachments' load operations and resource barriers.
         *
         * @param _commandList The command list in which to record the render pass begin.
         * @param _handle The render pass to begin, previously created with #CreateRenderPass.
         *
         * @return The encoder for all the render pass related commands.
         */
        [[nodiscard]] virtual RenderCommandEncoderHandle BeginRenderPass(
            CommandListHandle _commandList,
            RenderPassHandle _handle) = 0;

        /**
         * @brief Ends the render pass previously started with #BeginRenderPass, applying store operations.
         *
         * @param _renderCommandEncoder The command encoder associated with the render pass.
         */
        virtual void EndRenderPass(RenderCommandEncoderHandle _renderCommandEncoder) = 0;

        /**
         * @brief Begins a compute pass in the given command list.
         *
         * @param _commandList The command list in which to record the compute pass begin.
         */
        virtual void BeginComputePass(CommandListHandle _commandList) = 0;

        /**
         * @brief Ends the compute pass previously started with #BeginComputePass.
         *
         * @param _commandList The command list in which to record the compute pass end.
         */
        virtual void EndComputePass(CommandListHandle _commandList) = 0;

        virtual TransferCommandEncoderHandle BeginTransferPass(CommandListHandle _commandList) = 0;
        virtual void EndTransferPass(TransferCommandEncoderHandle _utilEncoder) = 0;

        /**
         * @brief Uploads texture data for a single subresource from a staging buffer.
         *
         * @param _transferEncoder The command encoder in which to record the copy operation.
         * @param _stagingBuffer The staging buffer containing the source data, previously created with
         * #CreateStagingBuffer and filled via #MapBuffer/#UnmapBuffer.
         * @param _dstTexture The destination texture to upload the data to.
         * @param _footprint The memory footprint of the subresource being uploaded.
         * @param _subResourceIndex The index of the subresource to upload.
         * @param _data A pointer to the raw data to copy into the staging buffer before the upload.
         */
        virtual void SetTextureData(
            TransferCommandEncoderHandle _transferEncoder,
            BufferHandle _stagingBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subResourceIndex,
            const void* _data) = 0;

        /**
         * @brief Uploads a specific region of a texture subresource from a buffer, for partial updates.
         *
         * @param _transferEncoder The command encoder in which to record the copy operation.
         * @param _srcBuffer The source buffer span containing the data to upload.
         * @param _dstTexture The destination texture to upload the data to.
         * @param _footprint The memory footprint of the subresource being updated.
         * @param _subresourceIndex The index of the subresource to update.
         * @param _regionOffset The offset, in texels, of the region to update within the subresource.
         * @param _regionSize The size, in texels, of the region to update.
         */
        virtual void SetTextureRegionData(
            TransferCommandEncoderHandle _transferEncoder,
            BufferSpan _srcBuffer,
            TextureHandle _dstTexture,
            const TextureMemoryFootprint& _footprint,
            const SubResourceIndexing& _subresourceIndex,
            const uint3& _regionOffset,
            const uint3& _regionSize) = 0;

        /**
         * @brief Maps a buffer's memory for CPU access.
         *
         * @param _mapping In/out parameter describing the buffer to map; on success, filled with a pointer to
         * the mapped memory.
         */
        virtual void MapBuffer(BufferMapping& _mapping) = 0;

        /**
         * @brief Unmaps a buffer's memory previously mapped with #MapBuffer.
         *
         * @param _mapping The mapping information previously obtained from #MapBuffer.
         */
        virtual void UnmapBuffer(BufferMapping& _mapping) = 0;

        /**
         * @brief Copies data between buffers using a GPU command.
         *
         * @param _transferEncoder The command encoder in which to record the copy operation.
         * @param _params The parameters describing the source, destination and range of the copy.
         */
        virtual void CopyBuffer(TransferCommandEncoderHandle _transferEncoder, const BufferCopyParameters& _params) = 0;

        /**
         * @brief Indicates whether the current graphics API supports per-resource (non-global) memory barriers.
         *
         * @return `true` if buffer- and texture-specific barriers are supported, `false` if only global
         * barriers are available.
         */
        [[nodiscard]] static bool SupportsNonGlobalBarriers();

        /**
         * @brief Inserts GPU memory barriers to synchronize resource access between operations.
         *
         * @param _commandEncoder The command list in which to record the barriers.
         * @param _barriers The memory barriers to insert.
         *
         * @see SupportsNonGlobalBarriers
         */
        virtual void PlaceMemoryBarriers(CommandEncoderHandle _commandEncoder, const MemoryBarriers& _barriers) = 0;

        [[deprecated]] void PlaceMemoryBarriers(CommandListHandle _commandList, const MemoryBarriers& _barriers)
        {
            PlaceMemoryBarriers(CommandEncoderHandle { _commandList }, _barriers);
        }

        /**
         * @brief Indicates whether the current graphics API requires explicit resource usage declaration
         * before a render pass (see #DeclarePassTextureViewUsage and #DeclarePassBufferViewUsage).
         */
        [[nodiscard]] static bool RenderPassNeedsUsageDeclaration();

        /**
         * @brief Indicates whether the current graphics API requires explicit resource usage declaration
         * before a compute pass (see #DeclarePassTextureViewUsage and #DeclarePassBufferViewUsage).
         */
        [[nodiscard]] static bool ComputePassNeedsUsageDeclaration();

        /**
         * @brief Declares how a set of texture views will be accessed during the upcoming render or compute pass.
         *
         * @param _commandList The command list in which to record the declaration.
         * @param _textures The texture views whose usage is being declared.
         * @param _accessType The type of access (e.g. read, write) that will be performed on the texture views.
         *
         * @see RenderPassNeedsUsageDeclaration, ComputePassNeedsUsageDeclaration
         */
        virtual void DeclarePassTextureViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const TextureViewHandle>& _textures,
            KryneEngine::TextureViewAccessType _accessType) = 0;

        /**
         * @brief Declares how a set of buffer views will be accessed during the upcoming render or compute pass.
         *
         * @param _commandList The command list in which to record the declaration.
         * @param _buffers The buffer views whose usage is being declared.
         * @param _accessType The type of access (e.g. read, write) that will be performed on the buffer views.
         *
         * @see RenderPassNeedsUsageDeclaration, ComputePassNeedsUsageDeclaration
         */
        virtual void DeclarePassBufferViewUsage(
            CommandListHandle _commandList,
            const eastl::span<const BufferViewHandle>& _buffers,
            BufferViewAccessType _accessType) = 0;

        /**
         * @brief Registers a compiled shader bytecode blob with the graphics API, creating a shader module.
         *
         * @param _bytecodeData A pointer to the compiled shader bytecode.
         * @param _bytecodeSize The size, in bytes, of the shader bytecode.
         *
         * @return A handle to the newly created shader module.
         */
        [[nodiscard]] virtual ShaderModuleHandle RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize) = 0;

        /**
         * @brief Creates a descriptor set layout, describing the bindings available in a descriptor set.
         *
         * @param _desc The parameters describing the descriptor set layout to create.
         * @param _bindingIndices Output array, filled with the resolved binding index for each binding
         * described in `_desc`.
         *
         * @return A handle to the newly created descriptor set layout.
         */
        [[nodiscard]] virtual DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetDesc& _desc, u32* _bindingIndices) = 0;

        /**
         * @brief Creates a descriptor set instance from a given descriptor set layout.
         *
         * @param _layout The descriptor set layout to instantiate, previously created with #CreateDescriptorSetLayout.
         *
         * @return A handle to the newly created descriptor set.
         */
        [[nodiscard]] virtual DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout) = 0;

        /**
         * @brief Creates a pipeline layout, describing the set of descriptor set layouts and push constant
         * ranges usable by a pipeline.
         *
         * @param _desc The parameters describing the pipeline layout to create.
         *
         * @return A handle to the newly created pipeline layout.
         */
        [[nodiscard]] virtual PipelineLayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& _desc) = 0;

        /**
         * @brief Creates a graphics pipeline state object.
         *
         * @param _desc The parameters describing the graphics pipeline to create (shaders, blend/depth/
         * rasterizer states, pipeline layout, render pass compatibility, etc.).
         *
         * @return A handle to the newly created graphics pipeline.
         */
        [[nodiscard]] virtual GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc) = 0;

        /**
         * @brief Destroys a graphics pipeline previously created with #CreateGraphicsPipeline.
         *
         * @param _pipeline The handle of the graphics pipeline to destroy.
         *
         * @return `true` if the graphics pipeline was successfully destroyed.
         */
        virtual bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline) = 0;

        /**
         * @brief Destroys a pipeline layout previously created with #CreatePipelineLayout.
         *
         * @param _layout The handle of the pipeline layout to destroy.
         *
         * @return `true` if the pipeline layout was successfully destroyed.
         */
        virtual bool DestroyPipelineLayout(PipelineLayoutHandle _layout) = 0;

        /**
         * @brief Destroys a descriptor set previously created with #CreateDescriptorSet.
         *
         * @param _set The handle of the descriptor set to destroy.
         *
         * @return `true` if the descriptor set was successfully destroyed.
         */
        virtual bool DestroyDescriptorSet(DescriptorSetHandle _set) = 0;

        /**
         * @brief Destroys a descriptor set layout previously created with #CreateDescriptorSetLayout.
         *
         * @param _layout The handle of the descriptor set layout to destroy.
         *
         * @return `true` if the descriptor set layout was successfully destroyed.
         */
        virtual bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout) = 0;

        /**
         * @brief Frees a shader module previously created with #RegisterShaderModule.
         *
         * @param _module The handle of the shader module to free.
         *
         * @return `true` if the shader module was successfully freed.
         */
        virtual bool FreeShaderModule(ShaderModuleHandle _module) = 0;

        /**
         * @brief Creates a compute pipeline state object.
         *
         * @param _desc The parameters describing the compute pipeline to create (shader, pipeline layout, etc.).
         *
         * @return A handle to the newly created compute pipeline.
         */
        [[nodiscard]] virtual ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc) = 0;

        /**
         * @brief Destroys a compute pipeline previously created with #CreateComputePipeline.
         *
         * @param _pipeline The handle of the compute pipeline to destroy.
         *
         * @return `true` if the compute pipeline was successfully destroyed.
         */
        virtual bool DestroyComputePipeline(ComputePipelineHandle _pipeline) = 0;

        /**
         * @brief Updates the resource bindings of a descriptor set.
         *
         * @details
         * The writes are applied to the descriptor set for the current frame context. When `_singleFrame` is `false`,
         * the update is also replicated to the other frame contexts over the next few `GetFrameContextCount() - 1`
         * frames. This replication process is triggered during `EndFrame()`.
         *
         * Updates are persistent and must be explicitly overwritten to change.
         *
         * This mechanism allows for per-frame-context descriptor set variants that are persistent. This is especially
         * useful when you need to work with frame-context dependent resources.
         *
         * As a rule of thumb, descriptor set updates are not free, and should be avoided when possible. Though they
         * are preferable to re-creating descriptor sets.
         *
         * @param _descriptorSet The descriptor set to update.
         * @param _writes The list of write operations to apply to the descriptor set's bindings.
         * @param _singleFrame If `true`, the update is applied only to the current frame context. If `false`,
         * the update is replicated to all other frame contexts progressively via `EndFrame()`.
         */
        virtual void UpdateDescriptorSet(
            DescriptorSetHandle _descriptorSet,
            const eastl::span<const DescriptorSetWriteInfo>& _writes,
            bool _singleFrame) = 0;

        /**
         * @brief Sets the viewport used for rasterization in the given render command encoder.
         *
         * @param _renderEncoder The command encoder in which to record the viewport state.
         * @param _viewport The viewport dimensions and depth range to set.
         */
        virtual void SetViewport(RenderCommandEncoderHandle _renderEncoder, const Viewport& _viewport) = 0;

        /**
         * @brief Sets the scissor rectangle used to clip rasterization in the given render command encoder.
         *
         * @param _renderEncoder The command encoder in which to record the scissor rectangle.
         * @param _rect The scissor rectangle to set.
         */
        virtual void SetScissorsRect(RenderCommandEncoderHandle _renderEncoder, const Rect& _rect) = 0;

        /**
         * @brief Binds an index buffer for use by subsequent indexed draw calls.
         *
         * @param _renderEncoder The command encoder in which to record the index buffer binding.
         * @param _indexBufferView The buffer span to bind as the index buffer.
         * @param _isU16 Whether the index buffer contains 16-bit indices (`true`) or 32-bit indices (`false`).
         */
        virtual void SetIndexBuffer(
            RenderCommandEncoderHandle _renderEncoder,
            const BufferSpan& _indexBufferView,
            bool _isU16) = 0;

        /**
         * @brief Binds one or more vertex buffers for use by subsequent draw calls.
         *
         * @param _renderEncoder The command encoder in which to record the vertex buffer bindings.
         * @param _bufferViews The buffer spans to bind as vertex buffers, in binding slot order.
         */
        virtual void SetVertexBuffers(
            RenderCommandEncoderHandle _renderEncoder,
            const eastl::span<const BufferSpan>& _bufferViews) = 0;

        /**
         * @brief Binds a graphics pipeline for use by subsequent draw calls.
         *
         * @param _renderEncoder The command encoder in which to record the pipeline binding.
         * @param _graphicsPipeline The graphics pipeline to bind, previously created with #CreateGraphicsPipeline.
         */
        virtual void SetGraphicsPipeline(
            RenderCommandEncoderHandle _renderEncoder,
            GraphicsPipelineHandle _graphicsPipeline) = 0;

        /**
         * @brief Sets push constant data for the graphics pipeline stages.
         *
         * @param _renderEncoder The command encoder in which to record the push constant update.
         * @param _layout The pipeline layout describing the push constant range to update.
         * @param _data The raw push constant data to set.
         * @param _index The index of the push constant range in the pipeline layout.
         * @param _offset The offset, in 32-bit words, within the push constant range at which to start writing.
         */
        virtual void SetGraphicsPushConstant(
            RenderCommandEncoderHandle _renderEncoder,
            PipelineLayoutHandle _layout,
            const eastl::span<const u32>& _data,
            u32 _index,
            u32 _offset) = 0;

        /**
         * @brief Binds a set of descriptor sets for use by the graphics pipeline, starting at a given offset.
         *
         * @param _renderEncoder The command encoder in which to record the descriptor set bindings.
         * @param _layout The pipeline layout describing the descriptor set slots to bind to.
         * @param _sets The descriptor sets to bind.
         * @param _offset The index of the first descriptor set slot (in `_layout`) to bind to.
         */
        virtual void SetGraphicsDescriptorSetsWithOffset(
            RenderCommandEncoderHandle _renderEncoder,
            PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets,
            u32 _offset) = 0;

        /**
         * @brief Binds a set of descriptor sets for use by the graphics pipeline, starting at slot 0.
         *
         * @param _renderEncoder The command encoder in which to record the descriptor set bindings.
         * @param _layout The pipeline layout describing the descriptor set slots to bind to.
         * @param _sets The descriptor sets to bind.
         *
         * @see SetGraphicsDescriptorSetsWithOffset
         */
        void SetGraphicsDescriptorSets(
            const RenderCommandEncoderHandle _renderEncoder,
            const PipelineLayoutHandle _layout,
            const eastl::span<const DescriptorSetHandle>& _sets)
        {
            SetGraphicsDescriptorSetsWithOffset(_renderEncoder, _layout, _sets, 0);
        }

        /**
         * @brief Records a non-indexed, (potentially) instanced draw call.
         *
         * @param _renderEncoder The command list in which to record the draw call.
         * @param _desc The parameters describing the draw call (vertex/instance counts and offsets).
         */
        virtual void DrawInstanced(RenderCommandEncoderHandle _renderEncoder, const DrawInstancedDesc& _desc) = 0;

        /**
         * @brief Records an indexed, (potentially) instanced draw call.
         *
         * @param _renderEncoder The command list in which to record the draw call.
         * @param _desc The parameters describing the draw call (index/instance counts and offsets).
         */
        virtual void DrawIndexedInstanced(
            RenderCommandEncoderHandle _renderEncoder,
            const DrawIndexedInstancedDesc& _desc) = 0;

        /**
         * @brief Binds a compute pipeline for use by subsequent dispatch calls.
         *
         * @param _commandList The command list in which to record the pipeline binding.
         * @param _pipeline The compute pipeline to bind, previously created with #CreateComputePipeline.
         */
        virtual void SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline) = 0;

        /**
         * @brief Binds a set of descriptor sets for use by the compute pipeline, starting at a given offset.
         *
         * @param _commandList The command list in which to record the descriptor set bindings.
         * @param _layout The pipeline layout describing the descriptor set slots to bind to.
         * @param _sets The descriptor sets to bind.
         * @param _offset The index of the first descriptor set slot (in `_layout`) to bind to.
         */
        virtual void SetComputeDescriptorSetsWithOffset(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const DescriptorSetHandle> _sets,
            u32 _offset) = 0;

        /**
         * @brief Binds a set of descriptor sets for use by the compute pipeline, starting at slot 0.
         *
         * @param _commandList The command list in which to record the descriptor set bindings.
         * @param _layout The pipeline layout describing the descriptor set slots to bind to.
         * @param _sets The descriptor sets to bind.
         *
         * @see SetComputeDescriptorSetsWithOffset
         */
        void SetComputeDescriptorSets(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const DescriptorSetHandle> _sets)
        {
            SetComputeDescriptorSetsWithOffset(_commandList, _layout, _sets, 0);
        }

        /**
         * @brief Sets push constant data for the compute pipeline stage.
         *
         * @param _commandList The command list in which to record the push constant update.
         * @param _layout The pipeline layout describing the push constant range to update.
         * @param _data The raw push constant data to set.
         */
        virtual void SetComputePushConstant(
            CommandListHandle _commandList,
            PipelineLayoutHandle _layout,
            eastl::span<const u32> _data) = 0;

        /**
         * @brief Records a compute dispatch call.
         *
         * @param _commandList The command list in which to record the dispatch call.
         * @param _threadGroupCount The number of thread groups to dispatch, along each axis.
         * @param _threadGroupSize The size of a single thread group, along each axis, as expected by the
         * bound compute shader.
         */
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

        /**
         * @brief Records a GPU timestamp query in the given command list.
         *
         * @param _commandList The command list in which to record the timestamp.
         *
         * @return A handle to the recorded timestamp, to be resolved later with #GetResolvedTimestamp
         * or #GetResolvedTimestamps.
         */
        virtual TimestampHandle PutTimestamp(CommandListHandle _commandList) = 0;

        /**
         * @brief Retrieves the resolved GPU timestamp value for a previously recorded timestamp query.
         *
         * @param _timestamp The handle of the timestamp to resolve, as returned by #PutTimestamp.
         *
         * @return The resolved timestamp value.
         *
         * @note The frame in which the timestamp was recorded must have finished executing
         * (see #IsFrameExecuted) for the value to be available.
         */
        virtual u64 GetResolvedTimestamp(TimestampHandle _timestamp) const = 0;

        /**
         * @brief Retrieves all resolved GPU timestamp values recorded during a given frame.
         *
         * @param _frameId The identifier of the frame whose timestamps should be retrieved, as returned by
         * #GetFrameId.
         *
         * @return A span containing the resolved timestamp values for the given frame.
         */
        virtual eastl::span<const u64> GetResolvedTimestamps(u64 _frameId) const = 0;
    };
}


