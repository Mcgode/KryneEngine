/**
 * @file
 * @author Max Godefroy
 * @date 11/03/2023.
 */

#include "Graphics/DirectX12/Dx12GraphicsContext.hpp"

#include <bit>
#include <D3D12MemAlloc.h>
#include <dxgidebug.h>

#include "Graphics/DirectX12/Dx12DescriptorSetManager.hpp"
#include "Graphics/DirectX12/Dx12SwapChain.hpp"
#include "Graphics/DirectX12/HelperFunctions.hpp"
#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"
#include "KryneEngine/Core/Window/Window.hpp"

namespace KryneEngine
{
    Dx12GraphicsContext::Dx12GraphicsContext(
        AllocatorInstance _allocator,
        const GraphicsCommon::ApplicationInfo& _appInfo,
        Window* _window)
        : GraphicsContext(_allocator, _appInfo, _window)
        , m_swapChain(_allocator)
        , m_frameContexts(_allocator)
        , m_resources(_allocator)
        , m_descriptorSetManager(_allocator)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::Dx12GraphicsContext");

        KE_ASSERT(m_appInfo.IsDirectX12Api());

        ComPtr<IDXGIFactory4> factory4;
        UINT dxgiFactoryFlags = 0;

#if !defined(KE_FINAL)
        if (m_appInfo.m_features.m_validationLayers)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();

                // Enable additional debug layers.
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        Dx12Assert(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory4)));

        _CreateDevice(factory4.Get());
        _CreateCommandQueues();

        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.Init(
                m_appInfo,
                _window,
                factory4.Get(),
                m_device.Get(),
                m_directQueue.Get(),
                m_resources);

            m_frameContextCount = m_swapChain.m_renderTargetViews.Size();
        }
        else
        {
            // If no display, remain on double buffering.
            m_frameContextCount = 2;
        }

        m_resources.InitHeaps(m_device.Get());

        m_descriptorSetManager.Init(m_device.Get(), m_frameContextCount, m_frameId % m_frameContextCount);

        m_frameContexts.Resize(m_frameContextCount);
        m_frameContexts.InitAll(m_device.Get(),
                                m_directQueue != nullptr,
                                m_computeQueue != nullptr,
                                m_copyQueue != nullptr);

        // Create the frame fence
        Dx12Assert(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence)));
#if !defined(KE_FINAL)
        Dx12SetName(m_frameFence.Get(), L"Frame fence");
#endif
        m_frameFenceEvent = CreateEvent(nullptr, false, false, nullptr);
        KE_ASSERT(m_frameFenceEvent != nullptr);
    }

    Dx12GraphicsContext::~Dx12GraphicsContext()
    {
#if !defined(KE_FINAL)
        if (m_validationLayerMessageCallbackHandle != 0)
        {
            ComPtr<ID3D12InfoQueue1> infoQueue;
            if (SUCCEEDED(m_device->QueryInterface<ID3D12InfoQueue1>(&infoQueue)))
            {
                infoQueue->UnregisterMessageCallback(m_validationLayerMessageCallbackHandle);
            }
        }
#endif

        CloseHandle(m_frameFenceEvent);
        SafeRelease(m_frameFence);

        m_frameContexts.Clear();

        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.Destroy(m_resources);
        }

        SafeRelease(m_copyQueue);
        SafeRelease(m_computeQueue);
        SafeRelease(m_directQueue);

        SafeRelease(m_device);

        if (m_appInfo.m_features.m_validationLayers)
        {
            IDXGIDebug* debugDev;
            Dx12Assert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDev)));
            Dx12Assert(debugDev->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL));
        }
    }

    bool Dx12GraphicsContext::IsFrameExecuted(KryneEngine::u64 _frameId) const
    {
        return m_frameFence->GetCompletedValue() >= _frameId;
    }

    bool Dx12GraphicsContext::HasDedicatedTransferQueue() const
    {
        return m_copyQueue != nullptr;
    }

    bool Dx12GraphicsContext::HasDedicatedComputeQueue() const
    {
        return m_computeQueue != nullptr;
    }

    void Dx12GraphicsContext::InternalEndFrame()
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::EndFrame");

        const u8 frameIndex = m_frameId % m_frameContextCount;

        auto& frameContext = m_frameContexts[frameIndex];

        // Execute the command lists
        ID3D12CommandQueue* queue = nullptr;
        {
            KE_ZoneScopedFunction("Execute queues");

            const auto executeCommands = [&](ID3D12CommandQueue* _queue, Dx12FrameContext::CommandAllocationSet& _allocationSet)
            {
                KE_ZoneScoped("Execute command lists queue");

                if (_queue != nullptr && !_allocationSet.m_usedCommandLists.empty())
                {
                    queue = _queue;
                    auto** commandLists = reinterpret_cast<ID3D12CommandList**>(_allocationSet.m_usedCommandLists.data());
                    _queue->ExecuteCommandLists(_allocationSet.m_usedCommandLists.size(), commandLists);
                }
            };

            executeCommands(m_copyQueue.Get(), frameContext.m_copyCommandAllocationSet);
            executeCommands(m_computeQueue.Get(), frameContext.m_computeCommandAllocationSet);
            executeCommands(m_directQueue.Get(), frameContext.m_directCommandAllocationSet);
        }

        // Present the frame (if applicable)
        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.Present();
        }

        // Increment fence signal
        if (queue != nullptr)
        {
            Dx12Assert(queue->Signal(m_frameFence.Get(), m_frameId));
        }
        else
        {
            // If there was no submitted command list, simply wait for the previous frame and set the frame as completed.
            WaitForFrame(m_frameId - 1);
            Dx12Assert(m_frameFence->Signal(m_frameId));
        }
        frameContext.m_frameId = m_frameId;
        frameContext.m_directCommandAllocationSet.Reset();
        frameContext.m_computeCommandAllocationSet.Reset();
        frameContext.m_copyCommandAllocationSet.Reset();

        FrameMark;

        // Retrieve next frame index
        const u8 nextFrameIndex = (m_frameId + 1) % m_frameContextCount;

        // Wait for the previous frame with this index.
        WaitForFrame(m_frameContexts[nextFrameIndex].m_frameId);

        m_resources.FlushPools();

        // Duplicate descriptor in multi-frame heaps
        m_descriptorSetManager.NextFrame(m_device.Get(), m_resources, nextFrameIndex);
    }

    void Dx12GraphicsContext::WaitForFrame(u64 _frameId) const
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::WaitForFrame");

        if (m_frameFence->GetCompletedValue() < _frameId)
        {
            Dx12Assert(m_frameFence->SetEventOnCompletion(_frameId, m_frameFenceEvent));
            WaitForSingleObject(m_frameFenceEvent, INFINITE);
        }
    }

    void Dx12GraphicsContext::_CreateDevice(IDXGIFactory4* _factory4)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::_CreateDevice");

        ComPtr<IDXGIAdapter1> hardwareAdapter;
        _FindAdapter(_factory4, &hardwareAdapter);

        Dx12Assert(D3D12CreateDevice(hardwareAdapter.Get(),
                                     Dx12Converters::GetFeatureLevel(m_appInfo),
                                     IID_PPV_ARGS(&m_device)));
#if !defined(KE_FINAL)
        Dx12SetName(m_device.Get(), L"Device");
#endif

        m_resources.InitAllocator(m_device.Get(), hardwareAdapter.Get());

#if !defined(KE_FINAL)
        if (m_appInfo.m_features.m_validationLayers)
        {
            ComPtr<ID3D12InfoQueue1> infoQueue;
            if (SUCCEEDED(m_device->QueryInterface<ID3D12InfoQueue1>(&infoQueue)))
            {
                Dx12Assert(infoQueue->RegisterMessageCallback(
                        DebugLayerMessageCallback,
                        D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                        this,
                        &m_validationLayerMessageCallbackHandle));
            }
        }
#endif

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
            Dx12Assert(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)));
            m_enhancedBarriersEnabled = options12.EnhancedBarriersSupported;
        }
    }

    void Dx12GraphicsContext::_FindAdapter(IDXGIFactory4 *_factory, IDXGIAdapter1 **_adapter)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::_FindAdapter");

        *_adapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIFactory6> factory6;

        if (Dx12Verify(_factory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (u32 adapterIndex = 0;
                 SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex,
                                                                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                IID_PPV_ARGS(&adapter)));
                 adapterIndex++)
            {
                DXGI_ADAPTER_DESC1 adapterDesc;
                adapter->GetDesc1(&adapterDesc);

                if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't use software adapter
                    continue;
                }

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                                                Dx12Converters::GetFeatureLevel(m_appInfo),
                                                _uuidof(ID3D12Device),
                                                nullptr)))
                {
                    break;
                }
            }
        }

        *_adapter = adapter.Detach();
    }

    void Dx12GraphicsContext::_CreateCommandQueues()
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::_CreateCommandQueue");

        const auto& features = m_appInfo.m_features;

        if (features.m_graphics)
        {
            D3D12_COMMAND_QUEUE_DESC directQueueDesc {};
            directQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            directQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            directQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            directQueueDesc.NodeMask = 0;

            Dx12Assert(m_device->CreateCommandQueue(&directQueueDesc, IID_PPV_ARGS(&m_directQueue)));
#if !defined(KE_FINAL)
            Dx12SetName(m_directQueue.Get(), L"Direct queue");
#endif

        }

        if ((!features.m_graphics || features.m_asyncCompute) && features.m_compute )
        {
            D3D12_COMMAND_QUEUE_DESC computeQueueDesc {};
            computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            computeQueueDesc.NodeMask = 0;

            Dx12Assert(m_device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_computeQueue)));
#if !defined(KE_FINAL)
            Dx12SetName(m_computeQueue.Get(), L"Compute queue");
#endif
        }

        if ((!features.m_graphics && !features.m_compute || features.m_transferQueue) && features.m_transfer)
        {
            D3D12_COMMAND_QUEUE_DESC copyQueueDesc {};
            copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            copyQueueDesc.NodeMask = 0;

            Dx12Assert(m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyQueue)));
#if !defined(KE_FINAL)
            Dx12SetName(m_copyQueue.Get(), L"Copy queue");
#endif
        }
    }

    BufferHandle Dx12GraphicsContext::CreateBuffer(const BufferCreateDesc& _desc)
    {
        return m_resources.CreateBuffer(_desc);
    }

    bool Dx12GraphicsContext::NeedsStagingBuffer(BufferHandle _buffer)
    {
        D3D12MA::Allocation** pAllocation = m_resources.m_buffers.GetCold(_buffer.m_handle);
        VERIFY_OR_RETURN(pAllocation != nullptr, false);
        D3D12MA::Allocation* allocation = *pAllocation;

        return allocation->GetHeap()->GetDesc().Properties.Type != D3D12_HEAP_TYPE_UPLOAD;
    }

    bool Dx12GraphicsContext::DestroyBuffer(BufferHandle _buffer) {
        return m_resources.DestroyBuffer(_buffer);
    }

    TextureHandle Dx12GraphicsContext::CreateTexture(const TextureCreateDesc& _createDesc)
    {
        return m_resources.CreateTexture(_createDesc, m_device.Get());
    }

    eastl::vector<TextureMemoryFootprint> Dx12GraphicsContext::FetchTextureSubResourcesMemoryFootprints(const TextureDesc& _desc)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::FetchTextureSubResourcesMemoryFootprints");

        D3D12_RESOURCE_DESC resourceDesc {
            .Dimension = Dx12Converters::GetTextureResourceDimension(_desc.m_type),
            .Alignment = 0,
            .Width = _desc.m_dimensions.x,
            .Height = _desc.m_dimensions.y,
            .DepthOrArraySize = static_cast<u16>(_desc.m_type == TextureTypes::Single3D
                                                     ? _desc.m_dimensions.z
                                                     : _desc.m_arraySize),
            .MipLevels = _desc.m_mipCount,
            .Format = Dx12Converters::ToDx12Format(_desc.m_format),
            .SampleDesc = { .Count = 1, .Quality = 0 },
        };

        const u32 numSubResources = _desc.m_arraySize * _desc.m_mipCount;

        DynamicArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(m_allocator);
        footprints.Resize(numSubResources);

        m_device->GetCopyableFootprints(&resourceDesc, 0, numSubResources, 0, footprints.Data(), nullptr, nullptr, nullptr);

        eastl::vector<TextureMemoryFootprint> finalFootprints(m_allocator);
        for (const auto& footprint: footprints)
        {
            finalFootprints.push_back(TextureMemoryFootprint {
                .m_offset = footprint.Offset,
                .m_width = footprint.Footprint.Width,
                .m_height = footprint.Footprint.Height,
                .m_depth = static_cast<u16>(footprint.Footprint.Depth),
                .m_rowPitchAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT,
                .m_lineByteAlignedSize = footprint.Footprint.RowPitch,
                .m_format = Dx12Converters::FromDx12Format(footprint.Footprint.Format),
            });
        }

        return finalFootprints;
    }

    BufferHandle Dx12GraphicsContext::CreateStagingBuffer(
        const TextureDesc& _createDesc, const eastl::span<const TextureMemoryFootprint>& _footprints)
    {
        return m_resources.CreateStagingBuffer(_createDesc, _footprints);
    }

    bool Dx12GraphicsContext::DestroyTexture(TextureHandle _texture)
    {
        return m_resources.ReleaseTexture(_texture, true);
    }

    TextureViewHandle Dx12GraphicsContext::CreateTextureView(const TextureViewDesc& _viewDesc)
    {
        return m_resources.CreateTextureView(_viewDesc, m_device.Get());
    }

    bool Dx12GraphicsContext::DestroyTextureView(TextureViewHandle _textureView)
    {
        return m_resources.DestroyTextureView(_textureView);
    }

    SamplerHandle Dx12GraphicsContext::CreateSampler(const SamplerDesc& _samplerDesc)
    {
        return m_resources.CreateSampler(_samplerDesc, m_device.Get());
    }

    bool Dx12GraphicsContext::DestroySampler(SamplerHandle _sampler)
    {
        return m_resources.DestroySampler(_sampler);
    }

    BufferViewHandle Dx12GraphicsContext::CreateBufferView(const BufferViewDesc & _viewDesc)
    {
        return m_resources.CreateBufferView(_viewDesc, m_device.Get());
    }

    bool Dx12GraphicsContext::DestroyBufferView(BufferViewHandle _handle)
    {
        return m_resources.DestroyBufferView(_handle);
    }

    RenderTargetViewHandle Dx12GraphicsContext::CreateRenderTargetView(const RenderTargetViewDesc& _desc)
    {
        return m_resources.CreateRenderTargetView(_desc, m_device.Get());
    }

    bool Dx12GraphicsContext::DestroyRenderTargetView(RenderTargetViewHandle _rtv)
    {
        return m_resources.FreeRenderTargetView(_rtv);
    }

    RenderTargetViewHandle Dx12GraphicsContext::GetPresentRenderTargetView(u8 _index)
    {
        return m_swapChain.m_renderTargetViews[_index];
    }

    TextureHandle Dx12GraphicsContext::GetPresentTexture(u8 _swapChainIndex)
    {
        return (m_appInfo.m_features.m_present)
                   ? m_swapChain.m_renderTargetTextures[_swapChainIndex]
                   : TextureHandle { GenPool::kInvalidHandle };
    }

    u32 Dx12GraphicsContext::GetCurrentPresentImageIndex() const
    {
        return m_swapChain.GetBackBufferIndex();
    }

    RenderPassHandle Dx12GraphicsContext::CreateRenderPass(const RenderPassDesc& _desc)
    {
        return m_resources.CreateRenderPass(_desc);
    }

    bool Dx12GraphicsContext::DestroyRenderPass(RenderPassHandle _renderPass)
    {
        return m_resources.FreeRenderPass(_renderPass);
    }

    CommandListHandle Dx12GraphicsContext::BeginGraphicsCommandList()
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::BeginGraphicsCommand");

        const u8 frameIndex = m_frameId % m_frameContextCount;
        CommandList list = m_frameContexts[frameIndex].BeginDirectCommandList();
        m_descriptorSetManager.OnBeginGraphicsCommandList(list, frameIndex);
        return list;
    }

    void Dx12GraphicsContext::EndGraphicsCommandList(CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::EndGraphicsCommand");

        auto commandList = reinterpret_cast<CommandList>(_commandList);
        m_frameContexts[m_frameId % m_frameContextCount].EndDirectCommandList(commandList);
    }

    void Dx12GraphicsContext::BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _renderPass)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::BeginRenderPass");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        const auto* desc = m_resources.m_renderPasses.Get(_renderPass.m_handle);
        VERIFY_OR_RETURN_VOID(desc != nullptr);

        constexpr auto convertLoadOperation = [](RenderPassDesc::Attachment::LoadOperation _op)
        {
            switch (_op)
            {
                case RenderPassDesc::Attachment::LoadOperation::Load:
                    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                case RenderPassDesc::Attachment::LoadOperation::Clear:
                    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                case RenderPassDesc::Attachment::LoadOperation::DontCare:
                    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
            }
            KE_ERROR("Unreachable code");
            return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        };

        constexpr auto convertStoreOperation = [](RenderPassDesc::Attachment::StoreOperation _op)
        {
            switch (_op)
            {
                case RenderPassDesc::Attachment::StoreOperation::Store:
                    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                case RenderPassDesc::Attachment::StoreOperation::DontCare:
                    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
                case RenderPassDesc::Attachment::StoreOperation::Resolve:
                    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
            }
            KE_ERROR("Unreachable code");
            return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        };

        eastl::fixed_vector<D3D12_RESOURCE_BARRIER, RenderPassDesc::kMaxSupportedColorAttachments + 1, false> barriers;
        const auto addBarrier = [&barriers](const RenderPassDesc::Attachment& _desc, ID3D12Resource* _resource, bool _isDepthTarget = false)
        {
            const auto oldState = Dx12Converters::ToDx12ResourceState(_desc.m_initialLayout);
            const auto newState = _isDepthTarget ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;

            if (newState != oldState)
            {
                barriers.push_back(D3D12_RESOURCE_BARRIER{
                        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                                .pResource = _resource,
                                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                .StateBefore = oldState,
                                .StateAfter = newState,
                        }
                });
            }
        };

        eastl::fixed_vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC, RenderPassDesc::kMaxSupportedColorAttachments, false> colorAttachments;
        for (const auto& attachment: desc->m_colorAttachments)
        {
            CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_R32G32B32A32_FLOAT, attachment.m_clearColor.GetPtr());

            D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess {
                convertLoadOperation(attachment.m_loadOperation),
                { clearValue }
            };

            D3D12_RENDER_PASS_ENDING_ACCESS endingAccess {
                convertStoreOperation(attachment.m_storeOperation)
            };

            const GenPool::Handle handle = attachment.m_rtv.m_handle;
            VERIFY_OR_RETURN_VOID((handle.m_index & Dx12Resources::kDsvFlag) == 0);
            auto* rtvData = m_resources.m_renderTargetViews.Get(handle);
            VERIFY_OR_RETURN_VOID(rtvData != nullptr);

            colorAttachments.push_back(D3D12_RENDER_PASS_RENDER_TARGET_DESC { rtvData->m_cpuHandle, beginningAccess, endingAccess });

            addBarrier(attachment, *m_resources.m_textures.Get(rtvData->m_resource.m_handle));
        }

        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthStencilDesc;
        if (desc->m_depthStencilAttachment.has_value())
        {
            const auto& attachment = desc->m_depthStencilAttachment.value();

            CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, attachment.m_clearColor[0], attachment.m_stencilClearValue);

            D3D12_RENDER_PASS_BEGINNING_ACCESS depthBeginningAccess {
                    convertLoadOperation(attachment.m_loadOperation),
                    { clearValue }
            };
            D3D12_RENDER_PASS_ENDING_ACCESS depthEndingAccess {
                convertStoreOperation(attachment.m_storeOperation)
            };

            D3D12_RENDER_PASS_BEGINNING_ACCESS stencilBeginningAccess {
                    convertLoadOperation(attachment.m_stencilLoadOperation),
                    { clearValue }
            };
            D3D12_RENDER_PASS_ENDING_ACCESS stencilEndingAccess {
                convertStoreOperation(attachment.m_stencilStoreOperation)
            };

            GenPool::Handle handle = attachment.m_rtv.m_handle;
            VERIFY_OR_RETURN_VOID((handle.m_index & Dx12Resources::kDsvFlag) != 0);
            handle.m_index &= ~Dx12Resources::kDsvFlag;
            auto* rtvData = m_resources.m_depthStencilViews.Get(handle);
            VERIFY_OR_RETURN_VOID(rtvData != nullptr);

            depthStencilDesc = {
                    rtvData->m_cpuHandle,
                    depthBeginningAccess,
                    stencilBeginningAccess,
                    depthEndingAccess,
                    stencilEndingAccess
            };

            addBarrier(attachment, *m_resources.m_textures.Get(rtvData->m_resource.m_handle), true);
        }

        commandList->ResourceBarrier(barriers.size(), barriers.data());

        commandList->BeginRenderPass(
                colorAttachments.size(),
                colorAttachments.data(),
                desc->m_depthStencilAttachment.has_value() ? &depthStencilDesc : nullptr,
                D3D12_RENDER_PASS_FLAG_NONE);

        m_currentRenderPass = _renderPass;
    }

    void Dx12GraphicsContext::EndRenderPass(CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::EndRenderPass");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        const auto* desc = m_resources.m_renderPasses.Get(m_currentRenderPass.m_handle);
        VERIFY_OR_RETURN_VOID(desc != nullptr);

        commandList->EndRenderPass();

        eastl::fixed_vector<TextureMemoryBarrier, RenderPassDesc::kMaxSupportedColorAttachments + 1, false> barriers;
        const auto addBarrier = [&barriers](const RenderPassDesc::Attachment& _desc, TextureHandle _texture, bool _isDepthTarget = false)
        {
            const auto oldLayout = _isDepthTarget ? TextureLayout::DepthStencilAttachment : TextureLayout::ColorAttachment;
            if (oldLayout != _desc.m_finalLayout)
            {
                TextureMemoryBarrier barrier {
                    .m_stagesSrc = BarrierSyncStageFlags::All,
                    .m_stagesDst = BarrierSyncStageFlags::All,
                    .m_accessSrc = BarrierAccessFlags::ColorAttachment,
                    .m_texture = _texture,
                    .m_layoutSrc = oldLayout,
                    .m_layoutDst = _desc.m_finalLayout,
                };

                switch (_desc.m_finalLayout)
                {
                    case TextureLayout::Unknown:
                        barrier.m_accessDst = BarrierAccessFlags::None;
                        break;
                    case TextureLayout::Common:
                        barrier.m_accessDst = BarrierAccessFlags::All;
                        break;
                    case TextureLayout::Present:
                        barrier.m_accessDst = BarrierAccessFlags::AllRead;
                        break;
                    case TextureLayout::GenericRead:
                        barrier.m_accessDst = BarrierAccessFlags::AllRead;
                        break;
                    case TextureLayout::ColorAttachment:
                        barrier.m_accessDst = BarrierAccessFlags::ColorAttachment;
                        break;
                    case TextureLayout::DepthStencilAttachment:
                        barrier.m_accessDst = BarrierAccessFlags::DepthStencilWrite;
                        break;
                    case TextureLayout::DepthStencilReadOnly:
                        barrier.m_accessDst = BarrierAccessFlags::DepthStencilRead;
                        break;
                    case TextureLayout::UnorderedAccess:
                        barrier.m_accessDst = BarrierAccessFlags::UnorderedAccess;
                        break;
                    case TextureLayout::ShaderResource:
                        barrier.m_accessDst = BarrierAccessFlags::ShaderResource;
                        break;
                    case TextureLayout::TransferSrc:
                        barrier.m_accessDst = BarrierAccessFlags::TransferSrc;
                        break;
                    case TextureLayout::TransferDst:
                        barrier.m_accessDst = BarrierAccessFlags::TransferDst;
                        break;
                    case TextureLayout::ResolveSrc:
                        barrier.m_accessDst = BarrierAccessFlags::ResolveSrc;
                        break;
                    case TextureLayout::ResolveDst:
                        barrier.m_accessDst = BarrierAccessFlags::ResolveDst;
                        break;
                    case TextureLayout::ShadingRate:
                        barrier.m_accessDst = BarrierAccessFlags::ShadingRate;
                        break;
                }

                barriers.push_back(barrier);
            }
        };

        for (const auto& attachment: desc->m_colorAttachments)
        {
            auto* rtvData = m_resources.m_renderTargetViews.Get(attachment.m_rtv.m_handle);
            VERIFY_OR_RETURN_VOID(rtvData != nullptr);

            addBarrier(attachment, rtvData->m_resource);
        }

        if (desc->m_depthStencilAttachment.has_value())
        {
            const auto& attachment = desc->m_depthStencilAttachment.value();

            GenPool::Handle handle = attachment.m_rtv.m_handle;
            VERIFY_OR_RETURN_VOID((handle.m_index & Dx12Resources::kDsvFlag) != 0);
            handle.m_index &= ~Dx12Resources::kDsvFlag;
            auto* rtvData = m_resources.m_renderTargetViews.Get(handle);
            VERIFY_OR_RETURN_VOID(rtvData != nullptr);

            addBarrier(attachment, rtvData->m_resource, true);
        }

        PlaceMemoryBarriers(commandList, {}, {}, barriers);

        m_currentRenderPass = GenPool::kInvalidHandle;
    }

    void Dx12GraphicsContext::SetTextureData(
        CommandListHandle _commandList,
        BufferHandle _stagingBuffer,
        TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subResourceIndex,
        const void* _data)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetTextureData");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        ID3D12Resource** stagingTexture = m_resources.m_buffers.Get(_stagingBuffer.m_handle);
        ID3D12Resource** dstTexture = m_resources.m_textures.Get(_dstTexture.m_handle);

        VERIFY_OR_RETURN_VOID(stagingTexture != nullptr);
        VERIFY_OR_RETURN_VOID(dstTexture != nullptr);

        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {
            .Offset = _footprint.m_offset,
            .Footprint = {
                .Format = Dx12Converters::ToDx12Format(_footprint.m_format),
                .Width = _footprint.m_width,
                .Height = _footprint.m_height,
                .Depth = _footprint.m_depth,
                .RowPitch = _footprint.m_lineByteAlignedSize,
            },
        };

        const s64 inputRowPitch = footprint.Footprint.Width * GetTextureBytesPerPixel(footprint.Footprint.Format);
        const D3D12_SUBRESOURCE_DATA data {
            .pData = _data,
            .RowPitch = inputRowPitch,
            .SlicePitch = inputRowPitch * static_cast<s64>(footprint.Footprint.Height),
        };

        {
            void* bufferData;
            (*stagingTexture)->Map(0, nullptr, &bufferData);

            const D3D12_MEMCPY_DEST copyInfo {
                .pData = (u8*)bufferData + footprint.Offset,
                .RowPitch = footprint.Footprint.RowPitch,
                .SlicePitch = footprint.Footprint.RowPitch * footprint.Footprint.Height,
            };
            MemcpySubresource(
                &copyInfo,
                &data,
                footprint.Footprint.RowPitch,
                footprint.Footprint.Height,
                footprint.Footprint.Depth);

            (*stagingTexture)->Unmap(0, nullptr);
        }

        const u32 subResourceIndex = D3D12CalcSubresource(
            _subResourceIndex.m_mipIndex,
            _subResourceIndex.m_arraySlice,
            Dx12Converters::RetrievePlaneSlice(_subResourceIndex.m_planes, _subResourceIndex.m_planeSlice),
            _subResourceIndex.m_mipCount,
            _subResourceIndex.m_arraySize);

        const CD3DX12_TEXTURE_COPY_LOCATION Dst(*dstTexture, subResourceIndex);
        const CD3DX12_TEXTURE_COPY_LOCATION Src(*stagingTexture, footprint);
        commandList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
    }

    void Dx12GraphicsContext::SetTextureRegionData(
        CommandListHandle _commandList,
        BufferSpan _srcBuffer,
        TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subresourceIndex,
        const uint3& _regionOffset,
        const uint3& _regionSize)
    {
        auto commandList = static_cast<CommandList>(_commandList);

        ID3D12Resource** srcBuffer = m_resources.m_buffers.Get(_srcBuffer.m_buffer.m_handle);
        ID3D12Resource** dstTexture = m_resources.m_textures.Get(_dstTexture.m_handle);

        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {
            .Offset = _footprint.m_offset,
            .Footprint = {
                .Format = Dx12Converters::ToDx12Format(_footprint.m_format),
                .Width = _footprint.m_width,
                .Height = _footprint.m_height,
                .Depth = _footprint.m_depth,
                .RowPitch = _footprint.m_lineByteAlignedSize,
            },
        };
        const CD3DX12_TEXTURE_COPY_LOCATION src(*srcBuffer, footprint);

        const u32 subResourceIndex = D3D12CalcSubresource(
            _subresourceIndex.m_mipIndex,
            _subresourceIndex.m_arraySlice,
            Dx12Converters::RetrievePlaneSlice(_subresourceIndex.m_planes, _subresourceIndex.m_planeSlice),
            _subresourceIndex.m_mipCount,
            _subresourceIndex.m_arraySize);
        const CD3DX12_TEXTURE_COPY_LOCATION dst(*dstTexture, subResourceIndex);

        const D3D12_BOX box {
            static_cast<u32>(_srcBuffer.m_offset), 0, 0,
            static_cast<u32>(_srcBuffer.m_offset + _srcBuffer.m_size), 1, 1 };
        commandList->CopyTextureRegion(
            &dst,
            _regionOffset.x,
            _regionOffset.y,
            _regionOffset.z,
            &src,
            &box);
    }

    void Dx12GraphicsContext::MapBuffer(BufferMapping& _mapping)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::MapBuffer");

        D3D12MA::Allocation** pAllocation = m_resources.m_buffers.GetCold(_mapping.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(pAllocation != nullptr);
        D3D12MA::Allocation* allocation = *pAllocation;

        const auto bufferSize = reinterpret_cast<u64>(allocation->GetPrivateData());

        KE_ASSERT_MSG(_mapping.m_ptr == nullptr, "Structure still holds a mapping");
        KE_ASSERT(bufferSize >= _mapping.m_offset);
        KE_ASSERT(_mapping.m_size == ~0 || bufferSize >= _mapping.m_offset + _mapping.m_size);
        _mapping.m_size = eastl::min(_mapping.m_size, bufferSize - _mapping.m_offset);

        D3D12_RANGE range { 0, 0 };
        if (!_mapping.m_pureWrite)
        {
            range = { _mapping.m_offset, _mapping.m_offset + _mapping.m_size };
        }

        void* ptr;
        Dx12Assert(allocation->GetResource()->Map(0, &range, &ptr));
        _mapping.m_ptr = static_cast<std::byte*>(ptr) + _mapping.m_offset;
    }

    void Dx12GraphicsContext::UnmapBuffer(BufferMapping& _mapping)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::UnmapBuffer");

        ID3D12Resource** pBuffer = m_resources.m_buffers.Get(_mapping.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(pBuffer != nullptr);
        KE_ASSERT_MSG(_mapping.m_ptr != nullptr, "Structure holds no mapping");

        const D3D12_RANGE range { _mapping.m_offset, _mapping.m_offset + _mapping.m_size };
        (*pBuffer)->Unmap(0, &range);
        _mapping.m_ptr = nullptr;
    }

    void Dx12GraphicsContext::CopyBuffer(CommandListHandle _commandList, const BufferCopyParameters& _params)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::CopyBuffer");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        ID3D12Resource** bufferSrc = m_resources.m_buffers.Get(_params.m_bufferSrc.m_handle);
        ID3D12Resource** bufferDst = m_resources.m_buffers.Get(_params.m_bufferDst.m_handle);
        VERIFY_OR_RETURN_VOID(bufferSrc != nullptr && bufferDst != nullptr);

        commandList->CopyBufferRegion(
            *bufferDst,
            _params.m_offsetDst,
            *bufferSrc,
            _params.m_offsetSrc,
            _params.m_copySize);
    }

    void Dx12GraphicsContext::PlaceMemoryBarriers(
        CommandListHandle _commandList,
        const eastl::span<const GlobalMemoryBarrier>& _globalMemoryBarriers,
        const eastl::span<const BufferMemoryBarrier>& _bufferMemoryBarriers,
        const eastl::span<const TextureMemoryBarrier>& _textureMemoryBarriers)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::PlaceMemoryBarriers");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        using namespace Dx12Converters;

        if (m_enhancedBarriersEnabled)
        {
            eastl::fixed_vector<D3D12_BARRIER_GROUP, 3> barrierGroups;

            DynamicArray<D3D12_GLOBAL_BARRIER> globalMemoryBarriers(m_allocator, _globalMemoryBarriers.size());
            DynamicArray<D3D12_BUFFER_BARRIER> bufferMemoryBarriers(m_allocator, _bufferMemoryBarriers.size());
            DynamicArray<D3D12_TEXTURE_BARRIER> textureMemoryBarriers(m_allocator, _textureMemoryBarriers.size());

            if (!_globalMemoryBarriers.empty())
            {
                for (auto i = 0u; i < globalMemoryBarriers.Size(); i++)
                {
                    const GlobalMemoryBarrier& barrier = _globalMemoryBarriers[i];

                    globalMemoryBarriers[i] = D3D12_GLOBAL_BARRIER{
                        .SyncBefore = ToDx12BarrierSync(barrier.m_stagesSrc),
                        .SyncAfter = ToDx12BarrierSync(barrier.m_stagesDst),
                        .AccessBefore = ToDx12BarrierAccess(barrier.m_accessSrc),
                        .AccessAfter = ToDx12BarrierAccess(barrier.m_accessDst),
                    };
                }

                barrierGroups.push_back(D3D12_BARRIER_GROUP{
                    .Type = D3D12_BARRIER_TYPE_GLOBAL,
                    .NumBarriers = (u32)globalMemoryBarriers.Size(),
                    .pGlobalBarriers = globalMemoryBarriers.Data(),
                });
            }

            if (!_bufferMemoryBarriers.empty())
            {
                for (auto i = 0u; i < bufferMemoryBarriers.Size(); i++)
                {
                    const BufferMemoryBarrier& barrier = _bufferMemoryBarriers[i];
                    ID3D12Resource** buffer = m_resources.m_buffers.Get(barrier.m_buffer.m_handle);

                    bufferMemoryBarriers[i] = D3D12_BUFFER_BARRIER{
                        .SyncBefore = ToDx12BarrierSync(barrier.m_stagesSrc),
                        .SyncAfter = ToDx12BarrierSync(barrier.m_stagesDst),
                        .AccessBefore = ToDx12BarrierAccess(barrier.m_accessSrc),
                        .AccessAfter = ToDx12BarrierAccess(barrier.m_accessDst),
                        .pResource = buffer != nullptr ? *buffer : nullptr,
                        .Offset = barrier.m_offset,
                        .Size = barrier.m_size,
                    };
                }

                barrierGroups.push_back(D3D12_BARRIER_GROUP{
                    .Type = D3D12_BARRIER_TYPE_BUFFER,
                    .NumBarriers = (u32)bufferMemoryBarriers.Size(),
                    .pBufferBarriers = bufferMemoryBarriers.Data(),
                });
            }

            if (!_textureMemoryBarriers.empty())
            {
                for (auto i = 0u; i < textureMemoryBarriers.Size(); i++)
                {
                    const TextureMemoryBarrier& barrier = _textureMemoryBarriers[i];
                    ID3D12Resource** texture = m_resources.m_textures.Get(barrier.m_texture.m_handle);

                    textureMemoryBarriers[i] = D3D12_TEXTURE_BARRIER{
                        .SyncBefore = ToDx12BarrierSync(barrier.m_stagesSrc),
                        .SyncAfter = ToDx12BarrierSync(barrier.m_stagesDst),
                        .AccessBefore = ToDx12BarrierAccess(barrier.m_accessSrc),
                        .AccessAfter = ToDx12BarrierAccess(barrier.m_accessDst),
                        .LayoutBefore = ToDx12BarrierLayout(barrier.m_layoutSrc),
                        .LayoutAfter = ToDx12BarrierLayout(barrier.m_layoutDst),
                        .pResource = texture == nullptr ? nullptr : *texture,
                        .Subresources =
                            {
                                .IndexOrFirstMipLevel = barrier.m_mipStart,
                                .NumMipLevels = barrier.m_mipCount,
                                .FirstArraySlice = barrier.m_arrayStart,
                                .NumArraySlices = barrier.m_arrayCount,
                                .FirstPlane = 0,
                                .NumPlanes = static_cast<u32>(std::popcount(static_cast<u8>(barrier.m_planes)))
                            },
                        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
                    };
                }

                barrierGroups.push_back(D3D12_BARRIER_GROUP{
                    .Type = D3D12_BARRIER_TYPE_TEXTURE,
                    .NumBarriers = (u32)textureMemoryBarriers.Size(),
                    .pTextureBarriers = textureMemoryBarriers.Data(),
                });
            }

            commandList->Barrier(barrierGroups.size(), barrierGroups.data());
        }
        else
        {
            eastl::vector<D3D12_RESOURCE_BARRIER> resourceBarriers(m_allocator);

            for (const auto& barrier: _textureMemoryBarriers)
            {
                ID3D12Resource** texture = m_resources.m_textures.Get(barrier.m_texture.m_handle);
                if (texture == nullptr)
                {
                    continue;
                }

                D3D12_RESOURCE_STATES before = RetrieveState(barrier.m_accessSrc, barrier.m_layoutSrc);
                D3D12_RESOURCE_STATES after = RetrieveState(barrier.m_accessDst, barrier.m_layoutDst);

                for (u8 mip = barrier.m_mipStart; mip < barrier.m_mipCount; mip++)
                {
                    for (u16 slice = barrier.m_arrayStart; slice < barrier.m_arrayCount; slice++)
                    {
                        u32 subResourceIndex = D3D12CalcSubresource(mip, slice, 0, barrier.m_mipCount, barrier.m_arrayCount);

                        resourceBarriers.push_back(D3D12_RESOURCE_BARRIER {
                            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                            .Transition = {
                                .pResource = *texture,
                                .Subresource = subResourceIndex,
                                .StateBefore = before,
                                .StateAfter = after,
                            }
                        });
                    }
                }

                if (BitUtils::EnumHasAny(barrier.m_accessSrc, BarrierAccessFlags::UnorderedAccess)
                    && BitUtils::EnumHasAny(barrier.m_accessDst, BarrierAccessFlags::UnorderedAccess))
                {
                    resourceBarriers.push_back({
                        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                        .UAV = { .pResource = *texture },
                    });
                }
            }

            for (const auto& barrier: _bufferMemoryBarriers)
            {
                ID3D12Resource** buffer = m_resources.m_buffers.Get(barrier.m_buffer.m_handle);
                if (buffer == nullptr)
                {
                    continue;
                }

                D3D12_RESOURCE_STATES before = RetrieveState(barrier.m_accessSrc, TextureLayout::Common);
                D3D12_RESOURCE_STATES after = RetrieveState(barrier.m_accessDst, TextureLayout::Common);

                resourceBarriers.push_back(D3D12_RESOURCE_BARRIER {
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Transition = {
                        .pResource = *buffer,
                        .Subresource = 0,
                        .StateBefore = before,
                        .StateAfter = after,
                    }
                });

                if (BitUtils::EnumHasAny(barrier.m_accessSrc, BarrierAccessFlags::UnorderedAccess)
                    && BitUtils::EnumHasAny(barrier.m_accessDst, BarrierAccessFlags::UnorderedAccess))
                {
                    resourceBarriers.push_back({
                        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                        .UAV = { .pResource = *buffer},
                    });
                }
            }

            for (const auto& barrier: _globalMemoryBarriers)
            {
                if (KE_VERIFY_MSG(BitUtils::EnumHasAny(barrier.m_accessSrc, BarrierAccessFlags::UnorderedAccess)
                                  && BitUtils::EnumHasAny(barrier.m_accessDst, BarrierAccessFlags::UnorderedAccess),
                                  "Global memory barriers for anything other than UAV barriers are not supported without enhanced barriers."))
                {
                    resourceBarriers.push_back({
                        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                        .UAV = { .pResource = nullptr },
                    });
                }
            }

            commandList->ResourceBarrier(resourceBarriers.size(), resourceBarriers.data());
        }
    }

    ShaderModuleHandle Dx12GraphicsContext::RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize)
    {
        return m_resources.RegisterShaderModule(_bytecodeData, _bytecodeSize);
    }

    DescriptorSetLayoutHandle Dx12GraphicsContext::CreateDescriptorSetLayout(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices)
    {
        return m_descriptorSetManager.CreateDescriptorSetLayout(_desc, _bindingIndices);
    }

    DescriptorSetHandle Dx12GraphicsContext::CreateDescriptorSet(DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetManager.CreateDescriptorSet(_layout);
    }

    PipelineLayoutHandle Dx12GraphicsContext::CreatePipelineLayout(const PipelineLayoutDesc& _desc)
    {
        return m_resources.CreatePipelineLayout(_desc, m_descriptorSetManager, m_device.Get());
    }

    GraphicsPipelineHandle Dx12GraphicsContext::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc)
    {
        return m_resources.CreateGraphicsPipeline(_desc, m_device.Get());
    }
    
    bool Dx12GraphicsContext::DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline)
    {
        return m_resources.DestroyGraphicsPipeline(_pipeline);
    }
    
    bool Dx12GraphicsContext::DestroyPipelineLayout(PipelineLayoutHandle _layout)
    {
        return m_resources.DestroyPipelineLayout(_layout);
    }

    bool Dx12GraphicsContext::DestroyDescriptorSet(DescriptorSetHandle _set)
    {
        return m_descriptorSetManager.DestroyDescriptorSet(_set);
    }

    bool Dx12GraphicsContext::DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetManager.DestroyDescriptorSetLayout(_layout);
    }

    bool Dx12GraphicsContext::FreeShaderModule(ShaderModuleHandle _module)
    {
        return m_resources.UnRegisterShaderModule(_module);
    }

    ComputePipelineHandle Dx12GraphicsContext::CreateComputePipeline(const ComputePipelineDesc &_desc)
    {
        // TODO
        return {};
    }

    bool Dx12GraphicsContext::DestroyComputePipeline(ComputePipelineHandle _pipeline)
    {
        // TODO
        return false;
    }

    void Dx12GraphicsContext::UpdateDescriptorSet(
        DescriptorSetHandle _descriptorSet,
        const eastl::span<const DescriptorSetWriteInfo>& _writes,
        bool _singleFrame)
    {
        m_descriptorSetManager.UpdateDescriptorSet(
            _descriptorSet,
            m_resources,
            _writes,
            _singleFrame,
            m_device.Get(),
            m_frameId % m_frameContextCount);
    }

    void Dx12GraphicsContext::SetViewport(CommandListHandle _commandList, const Viewport& _viewport)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetViewport");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        const D3D12_VIEWPORT viewport {
            .TopLeftX = static_cast<float>(_viewport.m_topLeftX),
            .TopLeftY = static_cast<float>(_viewport.m_topLeftY),
            .Width = static_cast<float>(_viewport.m_width),
            .Height = static_cast<float>(_viewport.m_height),
            .MinDepth = _viewport.m_minDepth,
            .MaxDepth = _viewport.m_maxDepth,
        };
        commandList->RSSetViewports(1, &viewport);
    }

    void Dx12GraphicsContext::SetScissorsRect(CommandListHandle _commandList, const Rect& _rect)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetScissorsRect");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        const D3D12_RECT scissorRect = {
            .left = static_cast<LONG>(_rect.m_left),
            .top = static_cast<LONG>(_rect.m_top),
            .right = static_cast<LONG>(_rect.m_right),
            .bottom = static_cast<LONG>(_rect.m_bottom),
        };
        commandList->RSSetScissorRects(1, &scissorRect);
    }

    void Dx12GraphicsContext::SetIndexBuffer(CommandListHandle _commandList, const BufferSpan& _indexBufferView, bool _isU16)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetIndexBuffer");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        VERIFY_OR_RETURN_VOID(_indexBufferView.m_buffer != GenPool::kInvalidHandle);
        ID3D12Resource** pIndexBuffer = m_resources.m_buffers.Get(_indexBufferView.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(pIndexBuffer != nullptr);

        const D3D12_INDEX_BUFFER_VIEW indexBufferView {
            .BufferLocation = (*pIndexBuffer)->GetGPUVirtualAddress() + _indexBufferView.m_offset,
            .SizeInBytes = static_cast<u32>(_indexBufferView.m_size),
            .Format = _isU16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        };

        commandList->IASetIndexBuffer(&indexBufferView);
    }

    void Dx12GraphicsContext::SetVertexBuffers(CommandListHandle _commandList, const eastl::span<const BufferSpan>& _bufferViews)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetVertexBuffers");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        eastl::fixed_vector<D3D12_VERTEX_BUFFER_VIEW, 4> bufferViews;
        bufferViews.reserve(_bufferViews.size());

        for (const auto& view: _bufferViews)
        {
            VERIFY_OR_RETURN_VOID(view.m_buffer != GenPool::kInvalidHandle);
            ID3D12Resource** pIndexBuffer = m_resources.m_buffers.Get(view.m_buffer.m_handle);
            VERIFY_OR_RETURN_VOID(pIndexBuffer != nullptr);

            bufferViews.push_back({
                .BufferLocation = (*pIndexBuffer)->GetGPUVirtualAddress() + view.m_offset,
                .SizeInBytes = static_cast<u32>(view.m_size),
                .StrideInBytes = view.m_stride,
            });
        }

        commandList->IASetVertexBuffers(0, bufferViews.size(), bufferViews.data());
    }

    void Dx12GraphicsContext::SetGraphicsPipeline(CommandListHandle _commandList, GraphicsPipelineHandle _graphicsPipeline)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetGraphicsPipeline");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        VERIFY_OR_RETURN_VOID(_graphicsPipeline != GenPool::kInvalidHandle);
        ID3D12PipelineState** pPso = m_resources.m_pipelineStateObjects.Get(_graphicsPipeline.m_handle);
        VERIFY_OR_RETURN_VOID(pPso != nullptr);
        Dx12Resources::PsoColdData* coldData = m_resources.m_pipelineStateObjects.GetCold(_graphicsPipeline.m_handle);
        VERIFY_OR_RETURN_VOID(coldData != nullptr);

        commandList->SetGraphicsRootSignature(coldData->m_signature);
        commandList->IASetPrimitiveTopology(Dx12Converters::ToDx12Topology(coldData->m_topology));
        commandList->SetPipelineState(*pPso);
    }

    void Dx12GraphicsContext::SetGraphicsPushConstant(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        const eastl::span<const u32>& _data,
        u32 _index,
        u32 _offset)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::SetGraphicsPushConstant");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        u32* offset = m_resources.m_pipelineLayouts.GetCold(_layout.m_handle);
        VERIFY_OR_RETURN_VOID(offset != nullptr);

        const u32 index = _index + *offset;
        commandList->SetGraphicsRoot32BitConstants(
            index,
            _data.size(),
            _data.data(),
            _offset);
    }

    void Dx12GraphicsContext::SetGraphicsDescriptorSetsWithOffset(
        CommandListHandle _commandList,
        PipelineLayoutHandle _pipelineLayout,
        const eastl::span<const DescriptorSetHandle>& _sets,
        u32 _offset)
    {
        m_descriptorSetManager.SetGraphicsDescriptorSets(
            static_cast<CommandList>(_commandList),
            _sets,
            m_resources.m_pipelineLayouts.Get(_pipelineLayout.m_handle)->m_tableSetOffsets,
            _offset,
            m_frameId % m_frameContextCount);
    }

    void Dx12GraphicsContext::DrawInstanced(CommandListHandle _commandList, const DrawInstancedDesc& _desc)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::DrawInstanced");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        commandList->DrawInstanced(
            _desc.m_vertexCount,
            _desc.m_instanceCount,
            _desc.m_vertexOffset,
            _desc.m_instanceOffset);
    }

    void Dx12GraphicsContext::DrawIndexedInstanced(CommandListHandle _commandList, const DrawIndexedInstancedDesc& _desc)
    {
        KE_ZoneScopedFunction("Dx12GraphicsContext::DrawIndexedInstanced");

        auto commandList = reinterpret_cast<CommandList>(_commandList);

        commandList->DrawIndexedInstanced(
            _desc.m_elementCount,
            _desc.m_instanceCount,
            _desc.m_indexOffset,
            _desc.m_vertexOffset,
            _desc.m_instanceOffset);
    }

    void Dx12GraphicsContext::SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline)
    {
        // TODO
    }

    void Dx12GraphicsContext::SetComputeDescriptorSetsWithOffset(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const DescriptorSetHandle> _sets,
        u32 _offset)
    {
        // TODO
    }

    void Dx12GraphicsContext::SetComputePushConstant(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const u32> _data)
    {
        // TODO
    }

    void Dx12GraphicsContext::Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3 _threadGroupSize)
    {
        // TODO
    }

    void Dx12GraphicsContext::PushDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view &_markerName,
        const Color &_color)
    {
        // TODO
    }

    void Dx12GraphicsContext::PopDebugMarker(CommandListHandle _commandList)
    {
        // TODO
    }

    void Dx12GraphicsContext::InsertDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view &_markerName,
        const Color &_color)
    {
        // TODO
    }

    void Dx12GraphicsContext::CalibrateCpuGpuClocks()
    {
        // TODO
    }

    TimestampHandle Dx12GraphicsContext::PutTimestamp(CommandListHandle _commandList)
    {
        // TODO
        return {};
    }

    u64 Dx12GraphicsContext::GetResolvedTimestamp(TimestampHandle _timestamp) const
    {
        // TODO
        return 0;
    }

    eastl::span<const u64> Dx12GraphicsContext::GetResolvedTimestamps(u64 _frameId) const
    {
        // TODO
        return {};
    }
} // KryneEngine