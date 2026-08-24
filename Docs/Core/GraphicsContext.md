# GraphicsContext API Documentation

## Overview

The `GraphicsContext` class is the central abstraction for all graphics operations in the KryneEngine. It provides a unified API for resource management, rendering, and compute operations across different graphics APIs and platforms. The context handles resource creation/destruction, command execution, state management, and synchronization.

## Architecture

### Base Class
`GraphicsContext` is an abstract base class that defines the interface for all graphics operations. Platform-specific implementations include:
- `VkGraphicsContext` - Vulkan implementation
- `Dx12GraphicsContext` - DirectX 12 implementation
- `MetalGraphicsContext` - Metal (Apple) implementation

### Key Concepts
- **Command Lists**: Encapsulated as opaque handles (`CommandListHandle`) that record GPU commands
- **Resources**: Buffers, textures, samplers, and views are managed via handles
- **Frame Contexts**: Multiple frame contexts allow for command buffer reuse and double/triple buffering
- **Descriptor Sets**: Manage resource binding for pipelines

## GraphicsContext Creation

### Static Factory Methods
```cpp
static GraphicsContext* Create(
    const GraphicsCommon::ApplicationInfo& _appInfo,
    Window* _window,
    AllocatorInstance _allocator);
static void Destroy(GraphicsContext* _context);
```

#### Create Parameters
The context is created using a factory pattern that takes three main parameters:

1. **ApplicationInfo** - Configuration structure defining application metadata and feature flags
2. **Window** - Pointer to the application's main window/surface
3. **AllocatorInstance** - Memory allocator for resource management

#### ApplicationInfo Configuration

The `ApplicationInfo` struct provides comprehensive configuration options:

##### Basic Application Metadata
- `m_applicationName` - Application name (default: "Unnamed app")
- `m_applicationVersion` - Application version (default: Version{})
- `m_engineVersion` - Engine version (default: 1, 0, 0)
- `m_api` - Graphics API to use (Vulkan, DirectX 12, or Metal)

##### API Selection
```cpp
enum class Api {
    None,

    // Vulkan versions
    Vulkan_1_0,
    Vulkan_1_1,
    Vulkan_1_2,
    Vulkan_1_3,
    DirectX12_0,
    DirectX12_1,
    DirectX12_2,
    Metal_4,
};
```

##### Feature Flags
```cpp
struct Features {
    SoftEnable m_validationLayers = SoftEnable::TryEnable;  // Debug validation
    SoftEnable m_debugTags = SoftEnable::TryEnable;         // Debug markers
    SoftEnable m_gpuTimestamps = SoftEnable::TryEnable;     // Timestamp queries
    u32 m_gpuTimestampBufferCapacity = 4'096;              // Buffer size for timestamps

    // Required features
    bool m_graphics = true;    // Graphics queue
    bool m_present = true;     // Present queue
    bool m_transfer = true;    // Transfer queue
    bool m_compute = true;     // Compute queue

    // Advanced features
    bool m_transferQueue = true;      // Dedicated transfer queue
    bool m_asyncCompute = false;      // Async compute queues
    bool m_concurrentQueues = true;   // Concurrent queue usage
};
```

##### Display Options
```cpp
struct DisplayOptions {
    u16 m_width = 1280;      // Default window width
    u16 m_height = 720;      // Default window height

    SoftEnable m_sRgbPresent = SoftEnable::TryEnable;  // sRGB color space
    SoftEnable m_tripleBuffering = SoftEnable::TryEnable;  // Triple buffering

    bool m_fullscreen = false;                    // Fullscreen mode
    bool m_resizableWindow = false;               // Window resizing support
};
```

#### Example Creation

##### Default Configuration
```cpp
GraphicsContext* context = GraphicsContext::Create(
    GraphicsCommon::ApplicationInfo{},
    window,
    allocator);
```

##### Vulkan with Debug Features
```cpp
GraphicsCommon::ApplicationInfo appInfo {
    .m_applicationName = "MyGame",
    .m_applicationVersion = { 1, 2, 3 },
    .m_api = GraphicsCommon::Api::Vulkan_1_2,
    .m_features = {
        .m_validationLayers = GraphicsCommon::SoftEnable::ForceEnabled,
        .m_debugTags = GraphicsCommon::SoftEnable::ForceEnabled,
        .m_gpuTimestamps = GraphicsCommon::SoftEnable::ForceEnabled,
        .m_gpuTimestampBufferCapacity = 8192,
    },
    .m_displayOptions = {
        .m_width = 1920,
        .m_height = 1080,
        .m_tripleBuffering = GraphicsCommon::SoftEnable::ForceEnabled,
    },
};

GraphicsContext* context = GraphicsContext::Create(
    appInfo,
    window,
    allocator);
```

##### Direct X 12 with High Performance
```cpp
GraphicsCommon::ApplicationInfo appInfo {
    .m_api = GraphicsCommon::Api::DirectX12_1,
    .m_features = {
        .m_validationLayers = GraphicsCommon::SoftEnable::Disabled,
        .m_debugTags = GraphicsCommon::SoftEnable::TryEnable,
        // Disable validation for maximum performance
        .m_graphics = true,
        .m_present = true,
        .m_transfer = true,
        .m_compute = true,
    },
};

GraphicsContext* context = GraphicsContext::Create(
    appInfo,
    window,
    allocator);
```

##### Metal with Display Options
```cpp
GraphicsCommon::ApplicationInfo appInfo {
    .m_applicationName = "iOS/MacOS App",
    .m_api = GraphicsCommon::Api::Metal_4,
    .m_features = {
        .m_validationLayers = GraphicsCommon::SoftEnable::Disabled,
    },
    .m_displayOptions = {
        .m_width = 1280,
        .m_height = 720,
        .m_sRgbPresent = GraphicsCommon::SoftEnable::ForceEnabled,
        .m_fullscreen = false,
        .m_resizableWindow = true,
    },
};

GraphicsContext* context = GraphicsContext::Create(
    appInfo,
    window,
    allocator);
```

##### Custom Memory Allocator
```cpp
// Custom allocator implementation
MyCustomAllocator allocator;

GraphicsContext* context = GraphicsContext::Create(
    appInfo,
    window,
    allocator.GetInstance());
```

### Error Handling
- Creation may fail if:
  - No suitable graphics API can be initialized
  - Required queues (graphics, present, transfer, compute) are unavailable
  - Window surface creation fails
  - Device creation fails due to feature requests
- Use the returned pointer for operations; destruction will clean up resources

### Lifecycle
- Create once per application lifetime
- Use the same context for all graphics operations
- Destroy when application exits: `GraphicsContext::Destroy(context)`
- All methods are thread-safe for independent operations
- Frame contexts can be used for concurrent work

## Core Properties

### Frame Management
- `GetFrameId()` - Returns the current frame identifier
- `GetCurrentFrameContextIndex()` - Gets the index of the current frame context for multithreading
- `GetFrameContextCount()` - Returns the total number of frame contexts

### Allocator
- `GetAllocator()` - Returns the memory allocator instance for resource creation

### Application Info
- `GetApplicationInfo()` - Returns application configuration information

## Resource Management

### Buffers

#### Create Buffer
```cpp
[[nodiscard]] virtual BufferHandle CreateBuffer(const BufferCreateDesc& _desc) = 0;
```
Creates a buffer with specified parameters (size, usage, flags).

#### Destroy Buffer
```cpp
virtual bool DestroyBuffer(BufferHandle _bufferHandle) = 0;
```
Releases a buffer and its resources.

#### Mapping
```cpp
void MapBuffer(BufferMapping& _mapping) = 0;
void UnmapBuffer(BufferMapping& _mapping) = 0;
```
Provides CPU access to buffer memory via mapping.

#### Copying
```cpp
void CopyBuffer(CommandListHandle _commandList, const BufferCopyParameters& _params) = 0;
```
Copies data between buffers using GPU commands.

#### Staging Buffers
```cpp
bool NeedsStagingBuffer(BufferHandle _buffer) = 0;
[[nodiscard]] BufferHandle CreateStagingBuffer(
    const TextureDesc& _createDesc,
    const eastl::span<const TextureMemoryFootprint>& _footprints) = 0;
```
Manages staging buffers for texture data uploads.

### Textures

#### Create Texture
```cpp
[[nodiscard]] virtual TextureHandle CreateTexture(const TextureCreateDesc& _createDesc) = 0;
```
Creates a texture with specified format, dimensions, and usage.

#### Destroy Texture
```cpp
virtual bool DestroyTexture(TextureHandle _handle) = 0;
```
Releases a texture and its resources.

#### Texture Views
```cpp
[[nodiscard]] virtual TextureViewHandle CreateTextureView(const TextureViewDesc& _viewDesc) = 0;
virtual bool DestroyTextureView(TextureViewHandle _handle) = 0;
```
Creates and destroys texture views (subresources, mipmaps, arrays).

#### Texture Data Upload
```cpp
void SetTextureData(
    CommandListHandle _commandList,
    BufferHandle _stagingBuffer,
    TextureHandle _dstTexture,
    const TextureMemoryFootprint& _footprint,
    const SubResourceIndexing& _subResourceIndex,
    const void* _data) = 0;
```
Uploads texture data from CPU memory using a staging buffer.

```cpp
void SetTextureRegionData(
    CommandListHandle _commandList,
    BufferSpan _srcBuffer,
    TextureHandle _dstTexture,
    const TextureMemoryFootprint& _footprint,
    const SubResourceIndexing& _subresourceIndex,
    const uint3& _regionOffset,
    const uint3& _regionSize) = 0;
```
Uploads a specific texture region for partial updates.

#### Memory Footprints
```cpp
[[nodiscard]] virtual eastl::vector<TextureMemoryFootprint> FetchTextureSubResourcesMemoryFootprints(
    const TextureDesc& _desc) = 0;
```
Returns memory requirements for texture subresources.

### Views

#### Buffer Views
```cpp
[[nodiscard]] virtual BufferViewHandle CreateBufferView(const BufferViewDesc& _viewDesc) = 0;
virtual bool DestroyBufferView(BufferViewHandle _handle) = 0;
```

#### Render Target Views
```cpp
[[nodiscard]] virtual RenderTargetViewHandle CreateRenderTargetView(const RenderTargetViewDesc& _desc) = 0;
virtual bool DestroyRenderTargetView(RenderTargetViewHandle _handle) = 0;
```

#### Present Resources
```cpp
[[nodiscard]] virtual RenderTargetViewHandle GetPresentRenderTargetView(u8 _swapChainIndex) = 0;
[[nodiscard]] virtual TextureHandle GetPresentTexture(u8 _swapChainIndex) = 0;
[[nodiscard]] virtual u32 GetCurrentPresentImageIndex() const = 0;
[[nodiscard]] virtual uint2 GetPresentFrameBufferSize() = 0;
virtual bool ResizeSwapChain(Window* _window) = 0;
```
Manages swap chain render targets, present images, and swap chain resizing (`ResizeSwapChain` adjusts dimensions to match updated window size and may fail if another resizing operation is ongoing).

### Samplers
```cpp
[[nodiscard]] virtual SamplerHandle CreateSampler(const SamplerDesc& _samplerDesc) = 0;
virtual bool DestroySampler(SamplerHandle _sampler) = 0;
```

### Render Passes
```cpp
[[nodiscard]] virtual RenderPassHandle CreateRenderPass(const RenderPassDesc& _desc) = 0;
virtual bool DestroyRenderPass(RenderPassHandle _handle) = 0;
```

## Command List Management

### Begin/End
```cpp
CommandListHandle BeginGraphicsCommandList() = 0;
void EndGraphicsCommandList(CommandListHandle _commandList) = 0;
```
Creates and finalizes command lists with graphics work. Command lists are individually thread-local, but multiple command lists can be active at the same time, allowing for multi-threaded command recording. Command lists are submitted in order of creation.

### Render Passes
```cpp
void BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _handle) = 0;
void EndRenderPass(CommandListHandle _commandList) = 0;
```
Wraps rendering in a render pass with proper resource barriers.

### Compute Passes
```cpp
void BeginComputePass(CommandListHandle _commandList) = 0;
void EndComputePass(CommandListHandle _commandList) = 0;
```
Wraps compute work in a compute pass.

## Memory Barriers
```cpp
[[nodiscard]] static bool SupportsNonGlobalBarriers();
void PlaceMemoryBarriers(
    CommandListHandle _commandList,
    const eastl::span<const GlobalMemoryBarrier>& _globalMemoryBarriers,
    const eastl::span<const BufferMemoryBarrier>& _bufferMemoryBarriers,
    const eastl::span<const TextureMemoryBarrier>& _textureMemoryBarriers) = 0;
```
Manages GPU memory synchronization between operations.

### Pass Usage Declarations
```cpp
[[nodiscard]] static bool RenderPassNeedsUsageDeclaration();
[[nodiscard]] static bool ComputePassNeedsUsageDeclaration();
void DeclarePassTextureViewUsage(
    CommandListHandle _commandList,
    const eastl::span<const TextureViewHandle>& _textures,
    KryneEngine::TextureViewAccessType _accessType) = 0;
void DeclarePassBufferViewUsage(
    CommandListHandle _commandList,
    const eastl::span<const BufferViewHandle>& _buffers,
    BufferViewAccessType _accessType) = 0;
```

## Pipelines

### Graphics Pipelines
```cpp
[[nodiscard]] ShaderModuleHandle RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize) = 0;
[[nodiscard]] DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetDesc& _desc, u32* _bindingIndices) = 0;
[[nodiscard]] DescriptorSetHandle CreateDescriptorSet(DescriptorSetLayoutHandle _layout) = 0;
[[nodiscard]] PipelineLayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& _desc) = 0;
[[nodiscard]] GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc) = 0;
```

#### Destroy Resources
```cpp
bool DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline) = 0;
bool DestroyPipelineLayout(PipelineLayoutHandle _layout) = 0;
bool DestroyDescriptorSet(DescriptorSetHandle _set) = 0;
bool DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout) = 0;
bool FreeShaderModule(ShaderModuleHandle _module) = 0;
```

### Compute Pipelines
```cpp
[[nodiscard]] ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& _desc) = 0;
bool DestroyComputePipeline(ComputePipelineHandle _pipeline) = 0;
```

### Descriptor Set Updates
```cpp
void UpdateDescriptorSet(
    DescriptorSetHandle _descriptorSet,
    const eastl::span<const DescriptorSetWriteInfo>& _writes,
    bool _singleFrame) = 0;
```
Updates resource bindings. When `_singleFrame` is `false`, the update is also replicated to other frame contexts over the next `GetFrameContextCount() - 1` frames during `EndFrame()`. Updates are persistent.

## State Management

### Viewports and Scissors
```cpp
void SetViewport(CommandListHandle _commandList, const Viewport& _viewport) = 0;
void SetScissorsRect(CommandListHandle _commandList, const Rect& _rect) = 0;
```

### Vertex Buffers
```cpp
void SetVertexBuffers(CommandListHandle _commandList, const eastl::span<const BufferSpan>& _bufferViews) = 0;
```

### Index Buffers
```cpp
void SetIndexBuffer(CommandListHandle _commandList, const BufferSpan& _indexBufferView, bool _isU16) = 0;
```

### Push Constants
```cpp
void SetGraphicsPushConstant(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    const eastl::span<const u32>& _data,
    u32 _index,
    u32 _offset) = 0;
```

### Descriptor Sets
```cpp
void SetGraphicsDescriptorSets(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    const eastl::span<const DescriptorSetHandle>& _sets);
void SetGraphicsDescriptorSetsWithOffset(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    const eastl::span<const DescriptorSetHandle>& _sets,
    u32 _offset) = 0;
```

### Compute Descriptor Sets & Push Constants
```cpp
void SetComputeDescriptorSets(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    eastl::span<const DescriptorSetHandle> _sets);
void SetComputeDescriptorSetsWithOffset(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    eastl::span<const DescriptorSetHandle> _sets,
    u32 _offset) = 0;
void SetComputePushConstant(
    CommandListHandle _commandList,
    PipelineLayoutHandle _layout,
    eastl::span<const u32> _data) = 0;
```

### Pipeline Binding
```cpp
void SetGraphicsPipeline(CommandListHandle _commandList, GraphicsPipelineHandle _graphicsPipeline) = 0;
void SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline) = 0;
```

## Draw Operations

### Instanced Drawing
```cpp
void DrawInstanced(CommandListHandle _commandList, const DrawInstancedDesc& _desc) = 0;
void DrawIndexedInstanced(CommandListHandle _commandList, const DrawIndexedInstancedDesc& _desc) = 0;
```

### Compute Dispatch
```cpp
void Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3 _threadGroupSize) = 0;
```

## Debug Markers

### Push Debug Marker
```cpp
void PushDebugMarker(
    CommandListHandle _commandList,
    const eastl::string_view& _markerName,
    const Color& _color) = 0;
```
Pushes a debug marker and begins a named region in the command list.

### Pop Debug Marker
```cpp
void PopDebugMarker(CommandListHandle _commandList) = 0;
```
Pops the most recent debug marker, ending the named region.

### Insert Debug Marker
```cpp
void InsertDebugMarker(
    CommandListHandle _commandList,
    const eastl::string_view& _markerName,
    const Color& _color) = 0;
```
Inserts a single-point debug marker for annotation without creating a region.

> **Warning**: Due to API restrictions (such as in Metal), this should only be used during compute or render passes.

## Synchronization

### Frame Control
```cpp
bool EndFrame();
void WaitForLastFrame() const;
```

### Timestamp Queries
```cpp
virtual TimestampHandle PutTimestamp(CommandListHandle _commandList) = 0;
virtual u64 GetResolvedTimestamp(TimestampHandle _timestamp) const = 0;
virtual eastl::span<const u64> GetResolvedTimestamps(u64 _frameId) const = 0;
```

### Clock Calibration
```cpp
void CalibrateCpuGpuClocks() = 0;
```

Calibrates the time synchronization between CPU and GPU clocks. Automatically called on context creation; recommended to call periodically (e.g., every N frames) for best results.

## Queue Information
```cpp
[[nodiscard]] virtual bool HasDedicatedTransferQueue() const = 0;
[[nodiscard]] virtual bool HasDedicatedComputeQueue() const = 0;
```

## Utility Methods
```cpp
[[nodiscard]] TracyGpuProfilerContext* GetProfilerContext() const;
[[nodiscard]] static const char* GetShaderFileExtension();
```

## Platform-Specific Behavior

### Debug Markers
- **Vulkan/12**: Full color and name support
- **Metal**: May not utilize color information

### Timestamp Queries
Available in all implementations, but behavior may vary between platforms.

### Render Passes
- Some platforms require explicit resource usage declarations
- Platform-specific memory barrier handling

### Clock Calibration
Implementation varies by platform and may have performance implications.

## Thread Safety
- Command lists are thread-local; use frame contexts for multithreaded work
- Resource creation and destruction operations are thread-safe due to `GenerationalPool` concurrent read-write support, 
with a few caveats.
- Resource destruction must account for resource lifetime, as it is not tracked internally. If a resource is used during
a frame, the resource should not be destroyed until after the frame is completed. This can be checked with the
`IsFrameExecuted` method.

## Error Handling
- Methods marked with `[[nodiscard]]` return handles or status values
- Specific error handling is platform-dependent through the underlying graphics API
- Debug validation layers provide additional runtime diagnostics