/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2024.
 */

#include "Graphics/Metal/MetalGraphicsContext.hpp"

#include "EASTL/vector_set.h"
#include "Graphics/Metal/Helpers/EnumConverters.hpp"
#include "Graphics/Metal/MetalConstants.hpp"
#include "Graphics/Metal/MetalFrameContext.hpp"
#include "Graphics/Metal/MetalSwapChain.hpp"
#include "Helpers/ByteUploader.hpp"
#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"
#include "KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"

namespace KryneEngine
{
    bool MetalGraphicsContext::IsFrameExecuted(const u64 _frameId) const
    {
        const u8 frameIndex = _frameId % m_frameContextCount;
        return _frameId < m_frameContexts[frameIndex].m_frameId;
    }

    bool MetalGraphicsContext::HasDedicatedTransferQueue() const
    {
        return m_ioQueue.get() != nullptr;
    }

    bool MetalGraphicsContext::HasDedicatedComputeQueue() const
    {
        return m_computeQueue.get() != nullptr;
    }

    void MetalGraphicsContext::InternalEndFrame()
    {
        KE_ZoneScopedFunction("MetalGraphicsContext::EndFrame");

        // Finish current frame and commit
        {
            KE_ZoneScoped("Finish current frame and commit");

            MTL::Drawable* drawable = nullptr;

            const u8 frameIndex = m_frameId % m_frameContextCount;
            MetalFrameContext& frameContext = m_frameContexts[frameIndex];

            if (m_appInfo.m_features.m_present)
            {
                drawable = m_swapChain.GetDrawable();
                m_graphicsQueue->wait(drawable);
            }

            {
                KE_ZoneScoped("Commit");
                {
                    const auto lock = m_resources.m_residencySetLock.AutoLock();
                    m_resources.m_residencySet->commit();
                }
                frameContext.m_graphicsAllocationSet.Commit(m_frameId, frameContext.m_enhancedCommandBufferErrors);
                frameContext.m_computeAllocationSet.Commit(m_frameId, frameContext.m_enhancedCommandBufferErrors);
                frameContext.m_ioAllocationSet.Commit(m_frameId, frameContext.m_enhancedCommandBufferErrors);
            }

            if (drawable != nullptr)
            {
                m_graphicsQueue->signalDrawable(drawable);
                drawable->present();
            }
        }

        if (m_profilerContext != nullptr)
        {
            m_profilerContext->EndFrame(m_frameId);
        }

        FrameMark;

        // Prepare next frame
        {
            KE_ZoneScoped("Prepare next frame");

            const u64 nextFrame = m_frameId + 1;
            const u8 newFrameIndex = nextFrame % m_frameContextCount;

            m_byteUploader->Reset(newFrameIndex);

            if (m_appInfo.m_features.m_present)
            {
                KE_ZoneScoped("Retrieve next drawable");
                m_swapChain.UpdateNextDrawable(newFrameIndex, m_resources);
            }

            if (nextFrame >= m_frameContextCount + kInitialFrameId)
            {
                const u64 previousFrameId = nextFrame - m_frameContextCount;
                m_frameContexts[newFrameIndex].WaitForFrame(previousFrameId);

                m_frameContexts[newFrameIndex].ResolveCounters(m_timestampConversion);
                m_lastResolvedFrameId = previousFrameId;
                if (m_profilerContext != nullptr)
                {
                    m_profilerContext->ResolveQueries(this, previousFrameId);
                }
            }

            m_frameContexts[newFrameIndex].PrepareForNextFrame(nextFrame);

            m_resources.FlushPools();
            m_argumentBufferManager.FlushPools();
            m_argumentBufferManager.UpdateAndFlushArgumentBuffers(m_resources, newFrameIndex);
        }
    }

    void MetalGraphicsContext::WaitForFrame(const u64 _frameId) const
    {
        const u64 start = _frameId > kInitialFrameId + m_frameContextCount - 1
            ? _frameId + 1 - m_frameContextCount
            : kInitialFrameId;

        for (u64 i = start; i <= _frameId; i++)
        {
            MetalFrameContext& frameContext = m_frameContexts[i % m_frameContextCount];
            frameContext.WaitForFrame(i);
            frameContext.ResolveCounters(m_timestampConversion);
            if (m_lastResolvedFrameId == ~0ull || m_lastResolvedFrameId < i)
            {
                m_lastResolvedFrameId = i;
                if (m_profilerContext != nullptr)
                {
                    m_profilerContext->ResolveQueries(this, i);
                }
            }
        }
    }

    bool MetalGraphicsContext::ResizeSwapChain(Window* _window)
    {
        m_swapChain.Resize(_window);
        return true;
    }

    BufferHandle MetalGraphicsContext::CreateBuffer(const BufferCreateDesc& _desc)
    {
        return m_resources.CreateBuffer(*m_device, _desc);
    }

    bool MetalGraphicsContext::NeedsStagingBuffer(const BufferHandle _buffer)
    {
        const MetalResources::BufferColdData* bufferCold = m_resources.m_buffers.GetCold(_buffer.m_handle);
        if (KE_VERIFY(bufferCold != nullptr)) [[likely]]
        {
            return bufferCold->m_options == MTL::ResourceStorageModePrivate;
        }
        return false;
    }

    bool MetalGraphicsContext::DestroyBuffer(const BufferHandle _bufferHandle)
    {
        return m_resources.DestroyBuffer(_bufferHandle);
    }

    TextureHandle MetalGraphicsContext::CreateTexture(const TextureCreateDesc& _createDesc)
    {
        if (GraphicsContext::CreateTexture(_createDesc) == GenPool::kInvalidHandle)
        {
            return { GenPool::kInvalidHandle };
        }
        return m_resources.CreateTexture(*m_device, _createDesc);
    }

    BufferHandle MetalGraphicsContext::CreateStagingBuffer(
        const TextureDesc& _createDesc,
        const eastl::span<const TextureMemoryFootprint>& _footprints)
    {
        const TextureMemoryFootprint& lastFootprint = _footprints.back();
        const size_t size = lastFootprint.m_offset +
                            lastFootprint.m_lineByteAlignedSize * lastFootprint.m_height * lastFootprint.m_depth;

        BufferCreateDesc desc {
            .m_desc = {
                .m_size = size,
#if !defined(KE_FINAL)
                .m_debugName = _createDesc.m_debugName + "/StagingBuffer"
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        };

        return CreateBuffer(desc);
    }

    eastl::vector<TextureMemoryFootprint> MetalGraphicsContext::FetchTextureSubResourcesMemoryFootprints(
        const TextureDesc& _desc)
    {
        eastl::vector<TextureMemoryFootprint> result { m_allocator };

        const size_t pixelByteSize = MetalConverters::GetPixelByteSize(_desc.m_format);
        size_t currentOffset = 0;

        const size_t rowAlignment = m_device->minimumLinearTextureAlignmentForPixelFormat(MetalConverters::ToPixelFormat(_desc.m_format));

        for (u16 arraySlice = 0; arraySlice < _desc.m_arraySize; arraySlice++)
        {
            for (u8 mip = 0; mip < _desc.m_mipCount; mip++)
            {
                TextureMemoryFootprint& footprint = result.emplace_back();

                footprint = {
                    .m_offset = currentOffset,
                    .m_width = eastl::max(_desc.m_dimensions.x >> mip, 1u),
                    .m_height = eastl::max(_desc.m_dimensions.y >> mip, 1u),
                    .m_depth = static_cast<u16>(eastl::max(_desc.m_dimensions.z >> mip, 1u)),
                    .m_format = _desc.m_format,
                };

                const size_t rowByteSize = Alignment::AlignUp(footprint.m_width * pixelByteSize, rowAlignment);
                footprint.m_lineByteAlignedSize = rowByteSize;
                footprint.m_rowPitchAlignment = rowAlignment;

                currentOffset += rowByteSize * footprint.m_height * footprint.m_depth;
            }
        }

        return result;
    }

    bool MetalGraphicsContext::DestroyTexture(const TextureHandle _handle)
    {
        return m_resources.UnregisterTexture(_handle);
    }

    TextureViewHandle MetalGraphicsContext::CreateTextureView(const TextureViewDesc& _viewDesc)
    {
        if (GraphicsContext::CreateTextureView(_viewDesc) == GenPool::kInvalidHandle)
        {
            return {GenPool::kInvalidHandle};
        }
        return m_resources.RegisterTextureView(_viewDesc);
    }

    bool MetalGraphicsContext::DestroyTextureView(const TextureViewHandle _handle)
    {
        return m_resources.UnregisterTextureView(_handle);
    }

    SamplerHandle MetalGraphicsContext::CreateSampler(const SamplerDesc& _samplerDesc)
    {
        return m_resources.CreateSampler(*m_device, _samplerDesc);
    }

    bool MetalGraphicsContext::DestroySampler(const SamplerHandle _sampler)
    {
        return m_resources.DestroySampler(_sampler);
    }

    BufferViewHandle MetalGraphicsContext::CreateBufferView(const BufferViewDesc& _viewDesc)
    {
        return m_resources.RegisterBufferView(_viewDesc);
    }

    bool MetalGraphicsContext::DestroyBufferView(const BufferViewHandle _handle)
    {
        return m_resources.UnregisterBufferView(_handle);
    }

    RenderTargetViewHandle MetalGraphicsContext::CreateRenderTargetView(const RenderTargetViewDesc& _desc)
    {
        return m_resources.RegisterRtv(_desc);
    }

    bool MetalGraphicsContext::DestroyRenderTargetView(const RenderTargetViewHandle _handle)
    {
        return m_resources.UnregisterRtv(_handle);
    }

    RenderTargetViewHandle MetalGraphicsContext::GetPresentRenderTargetView(const u8 _swapChainIndex)
    {
        VERIFY_OR_RETURN(m_appInfo.m_features.m_present, { GenPool::kInvalidHandle });

        return m_swapChain.m_rtvs[_swapChainIndex];
    }

    TextureHandle MetalGraphicsContext::GetPresentTexture(const u8 _swapChainIndex)
    {
        VERIFY_OR_RETURN(m_appInfo.m_features.m_present, { GenPool::kInvalidHandle });

        return m_swapChain.m_textures[_swapChainIndex];
    }

    u32 MetalGraphicsContext::GetCurrentPresentImageIndex() const
    {
        VERIFY_OR_RETURN(m_appInfo.m_features.m_present, 0);
        return GetCurrentFrameContextIndex();
    }

    uint2 MetalGraphicsContext::GetPresentFrameBufferSize()
    {
        return m_appInfo.m_features.m_present
            ? m_swapChain.GetDrawableSize()
            : uint2(1);
    }

    RenderPassHandle MetalGraphicsContext::CreateRenderPass(const RenderPassDesc& _desc)
    {
        return m_resources.CreateRenderPassDescriptor(_desc);
    }

    bool MetalGraphicsContext::DestroyRenderPass(const RenderPassHandle _handle)
    {
        return m_resources.DestroyRenderPassDescriptor(_handle);
    }

    CommandListHandle MetalGraphicsContext::BeginGraphicsCommandList()
    {
        VERIFY_OR_RETURN(m_graphicsQueue != nullptr, nullptr);
        const u8 frameIndex = m_frameId % m_frameContextCount;
        return m_frameContexts[frameIndex].BeginGraphicsCommandList(m_device.get());
    }

    void MetalGraphicsContext::EndGraphicsCommandList(const CommandListHandle _commandList)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        KE_ASSERT(commandList != nullptr);
        if (commandList->m_encoder != nullptr)
        {
            commandList->m_encoder->endEncoding();
            commandList->m_encoder = nullptr;
        }
        commandList->m_commandBuffer->endCommandBuffer();
    }

    RenderCommandEncoderHandle MetalGraphicsContext::BeginRenderPass(
        const CommandListHandle _commandList,
        const RenderPassHandle _handle)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        VERIFY_OR_RETURN(commandList != nullptr, { nullptr });

        const MetalResources::RenderPassHotData* rpHot = m_resources.m_renderPasses.Get(_handle.m_handle);
        VERIFY_OR_RETURN(rpHot != nullptr, { nullptr });

        // Update system RTVs
        for (const auto& systemRtv: rpHot->m_systemRtvs)
        {
            const MetalResources::RtvHotData* rtvHot = m_resources.m_renderTargetViews.Get(systemRtv.m_handle.m_handle);
            VERIFY_OR_RETURN(rtvHot != nullptr, { nullptr });

            rpHot->m_descriptor->colorAttachments()->object(systemRtv.m_index)
                ->setTexture(rtvHot->m_texture);
        }

        // Leaving dangling encoders is expected behaviour.
        // This allows same command type batching, avoiding encoder re-creation
        KE_ASSERT_FATAL(commandList->m_encoder == nullptr || commandList->m_type != CommandListData::EncoderType::Render);
        commandList->ResetEncoder(CommandListData::EncoderType::Render);

        KE_AUTO_RELEASE_POOL;

        MTL4::RenderCommandEncoder* encoder = commandList->m_commandBuffer->renderCommandEncoder(rpHot->m_descriptor)->retain();

#if !defined(KE_FINAL)
        auto* string = NS::String::string(rpHot->m_debugName.c_str(), NS::UTF8StringEncoding);
        commandList->m_encoder->setLabel(string);
#endif

        RenderState* renderState = m_allocator.New<RenderState>();

        {
            MTL4::ArgumentTableDescriptor* descriptor = MTL4::ArgumentTableDescriptor::alloc()->init();
            descriptor->setMaxBufferBindCount(MetalConstants::kVertexArgumentTableSize);
            renderState->m_vertexArgumentTable = m_device->newArgumentTable(descriptor, nullptr);
            descriptor->release();
        }
        {
            MTL4::ArgumentTableDescriptor* descriptor = MTL4::ArgumentTableDescriptor::alloc()->init();
            descriptor->setMaxBufferBindCount(MetalConstants::kDefaultArgumentTableSize);
            renderState->m_fragmentArgumentTable = m_device->newArgumentTable(descriptor, nullptr);
            descriptor->release();
        }
        encoder->setArgumentTable(renderState->m_vertexArgumentTable.get(), MTL::RenderStageVertex);
        encoder->setArgumentTable(renderState->m_fragmentArgumentTable.get(), MTL::RenderStageFragment);

        commandList->m_encoder = encoder;
        commandList->m_userData = renderState;

        return { commandList };
    }

    void MetalGraphicsContext::EndRenderPass(const RenderCommandEncoderHandle _renderCommandEncoder)
    {
        const auto commandList = static_cast<CommandList>(_renderCommandEncoder.m_handle);
        KE_ASSERT(commandList->m_type == CommandListData::EncoderType::Render);
        m_allocator.Delete(static_cast<RenderState*>(commandList->m_userData));
        commandList->m_userData = nullptr;
        commandList->ResetEncoder();
    }

    ComputeCommandEncoderHandle MetalGraphicsContext::BeginComputePass(const CommandListHandle _commandList)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        KE_ASSERT(commandList->m_type != CommandListData::EncoderType::Render);

        KE_AUTO_RELEASE_POOL;

        commandList->ResetEncoder(CommandListData::EncoderType::Compute);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());
        if (commandList->m_encoder == nullptr)
        {
            encoder = commandList->m_commandBuffer->computeCommandEncoder()->retain();
            commandList->m_encoder = encoder;
        }

        MTL4::ArgumentTableDescriptor* descriptor = MTL4::ArgumentTableDescriptor::alloc()->init();
        descriptor->setMaxBufferBindCount(MetalConstants::kDefaultArgumentTableSize);
        MTL4::ArgumentTable* argumentTable = m_device->newArgumentTable(descriptor, nullptr)->retain();
        encoder->setArgumentTable(argumentTable);

        commandList->m_userData = argumentTable;

        return { _commandList };
    }

    void MetalGraphicsContext::EndComputePass(const ComputeCommandEncoderHandle _computeEncoder)
    {
        const auto commandList = static_cast<CommandList>(_computeEncoder.m_handle);
        KE_ASSERT(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Compute);
        auto* argumentTable = static_cast<MTL4::ArgumentTable*>(commandList->m_userData);
        KE_ASSERT(argumentTable != nullptr);

        argumentTable->release();
        commandList->m_userData = nullptr;

        commandList->ResetEncoder();
    }

    TransferCommandEncoderHandle MetalGraphicsContext::BeginTransferPass(const CommandListHandle _commandList)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        KE_ASSERT(commandList->m_type != CommandListData::EncoderType::Render);

        KE_AUTO_RELEASE_POOL;

        commandList->ResetEncoder(CommandListData::EncoderType::Transfer);
        if (commandList->m_encoder == nullptr)
        {
            commandList->m_encoder = commandList->m_commandBuffer->computeCommandEncoder()->retain();
        }

        return { _commandList };
    }

    void MetalGraphicsContext::EndTransferPass(const TransferCommandEncoderHandle _utilEncoder)
    {
        const auto commandList = static_cast<CommandList>(_utilEncoder.m_handle);
        KE_ASSERT(commandList->m_type == CommandListData::EncoderType::Transfer);

        commandList->ResetEncoder();
    }

    void MetalGraphicsContext::SetTextureData(
        TransferCommandEncoderHandle _transferEncoder,
        const BufferHandle _stagingBuffer,
        const TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subResourceIndex,
        const void* _data)
    {
        const auto commandList = static_cast<CommandList>(_transferEncoder.m_handle);
        KE_ASSERT(commandList->m_type == CommandListData::EncoderType::Transfer || commandList->m_type == CommandListData::EncoderType::Compute);
        KE_ASSERT(commandList->m_encoder != nullptr);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());

        MTL::Buffer* stagingBuffer = m_resources.m_buffers.Get(_stagingBuffer.m_handle)->m_buffer;

        const auto stagingBufferContent = reinterpret_cast<uintptr_t>(stagingBuffer->contents());
        memcpy(
            reinterpret_cast<void*>(stagingBufferContent + _footprint.m_offset),
            _data,
            _footprint.m_lineByteAlignedSize * _footprint.m_height * _footprint.m_depth);

        encoder->copyFromBuffer(
            stagingBuffer,
            _footprint.m_offset,
            _footprint.m_lineByteAlignedSize,
            _footprint.m_depth == 1 ? 0 : _footprint.m_lineByteAlignedSize * _footprint.m_height,
            MTL::Size { _footprint.m_width, _footprint.m_height, _footprint.m_depth },
            m_resources.m_textures.Get(_dstTexture.m_handle)->m_texture,
            _subResourceIndex.m_arraySlice,
            _subResourceIndex.m_mipIndex,
            MTL::Origin { 0, 0, 0 });
    }

    void MetalGraphicsContext::SetTextureRegionData(
        TransferCommandEncoderHandle _transferEncoder,
        const BufferSpan _srcBuffer,
        const TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subresourceIndex,
        const uint3& _regionOffset,
        const uint3& _regionSize)
    {
        const auto commandList = static_cast<CommandList>(_transferEncoder.m_handle);
        KE_ASSERT(commandList->m_type == CommandListData::EncoderType::Transfer || commandList->m_type == CommandListData::EncoderType::Compute);
        KE_ASSERT(commandList->m_encoder != nullptr);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());

        const MTL::Buffer* srcBuffer = m_resources.m_buffers.Get(_srcBuffer.m_buffer.m_handle)->m_buffer;
        const MTL::Texture* dstTexture = m_resources.m_textures.Get(_dstTexture.m_handle)->m_texture;

        encoder->copyFromBuffer(
            srcBuffer,
            _srcBuffer.m_offset,
            _footprint.m_lineByteAlignedSize,
            _footprint.m_depth == 1 ? 0 : _footprint.m_lineByteAlignedSize * _footprint.m_height,
            MTL::Size { _footprint.m_width, _footprint.m_height, _footprint.m_depth },
            dstTexture,
            _subresourceIndex.m_arraySlice,
            _subresourceIndex.m_mipIndex,
            { _regionOffset.x, _regionOffset.y, _regionOffset.z });
    }

    void MetalGraphicsContext::MapBuffer(BufferMapping& _mapping)
    {
        MTL::Buffer* buffer = m_resources.m_buffers.Get(_mapping.m_buffer.m_handle)->m_buffer;

        KE_ASSERT_MSG(_mapping.m_ptr == nullptr, "Did not unmap previous map");

        KE_ASSERT(_mapping.m_size == ~0ull || buffer->length() >= _mapping.m_offset + _mapping.m_size);
        _mapping.m_size = eastl::min(_mapping.m_size, buffer->length() - _mapping.m_offset);

        _mapping.m_ptr = static_cast<std::byte*>(buffer->contents()) + _mapping.m_offset;
    }

    void MetalGraphicsContext::UnmapBuffer(BufferMapping& _mapping)
    {
        auto [hot, cold] = m_resources.m_buffers.GetAll(_mapping.m_buffer.m_handle);
        if ((cold->m_options & MTL::ResourceStorageModeManaged) != 0)
        {
            hot->m_buffer->didModifyRange({_mapping.m_offset, _mapping.m_size});
        }
        _mapping.m_ptr = nullptr;
    }

    void MetalGraphicsContext::CopyBuffer(
        const TransferCommandEncoderHandle _transferEncoder,
        const BufferCopyParameters& _params)
    {
        const auto commandList = static_cast<CommandList>(_transferEncoder.m_handle);
        KE_ASSERT(commandList->m_type == CommandListData::EncoderType::Transfer || commandList->m_type == CommandListData::EncoderType::Compute);
        KE_ASSERT(commandList->m_encoder != nullptr);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());

        encoder->copyFromBuffer(
            m_resources.m_buffers.Get(_params.m_bufferSrc.m_handle)->m_buffer,
            _params.m_offsetSrc,
            m_resources.m_buffers.Get(_params.m_bufferDst.m_handle)->m_buffer,
            _params.m_offsetDst,
            _params.m_copySize);
    }

    void MetalGraphicsContext::PlaceMemoryBarriers(
        const CommandEncoderHandle _commandEncoder,
        const MemoryBarriers& _barriers)
    {
        const auto commandList = static_cast<CommandList>(_commandEncoder.m_handle);

        KE_ASSERT(commandList->m_type != CommandListData::EncoderType::None);
        MTL4::CommandEncoder* encoder = commandList->m_encoder.get();
        KE_ASSERT(encoder != nullptr);

        eastl::vector_set<u64> barriers { m_allocator };

        constexpr BarrierAccessFlags writeFlags = BarrierAccessFlags::AllWrite
            | BarrierAccessFlags::ColorAttachment
            | BarrierAccessFlags::DepthStencilWrite
            | BarrierAccessFlags::UnorderedAccess
            | BarrierAccessFlags::ResolveDst
            | BarrierAccessFlags::TransferDst
            | BarrierAccessFlags::AccelerationStructureWrite;

        for (const auto& barrier: _barriers.m_globalBarriers)
        {
            if (barrier.m_accessSrc == BarrierAccessFlags::None)
                continue;
            if (!BitUtils::EnumHasAny(barrier.m_accessSrc, writeFlags) && BitUtils::EnumHasAny(barrier.m_accessDst, writeFlags))
                continue;
            const MTL::Stages after = MetalConverters::GetMetalStages(barrier.m_stagesSrc);
            const MTL::Stages before = MetalConverters::GetMetalStages(barrier.m_stagesDst);
            const u64 rawBarrier = BitUtils::BitfieldInsert<u64>(before, after, 32, 32);
            barriers.emplace(rawBarrier);
        }

        for (const auto& barrier: _barriers.m_bufferBarriers)
        {
            if (barrier.m_accessSrc == BarrierAccessFlags::None)
                continue;
            if (!BitUtils::EnumHasAny(barrier.m_accessSrc, writeFlags) && BitUtils::EnumHasAny(barrier.m_accessDst, writeFlags))
                continue;
            const MTL::Stages after = MetalConverters::GetMetalStages(barrier.m_stagesSrc);
            const MTL::Stages before = MetalConverters::GetMetalStages(barrier.m_stagesDst);
            const u64 rawBarrier = BitUtils::BitfieldInsert<u64>(before, after, 32, 32);
            barriers.emplace(rawBarrier);
        }

        for (const auto& barrier: _barriers.m_textureBarriers)
        {
            if (barrier.m_accessSrc == BarrierAccessFlags::None)
                continue;
            if (!BitUtils::EnumHasAny(barrier.m_accessSrc, writeFlags) && BitUtils::EnumHasAny(barrier.m_accessDst, writeFlags))
                continue;
            const MTL::Stages after = MetalConverters::GetMetalStages(barrier.m_stagesSrc);
            const MTL::Stages before = MetalConverters::GetMetalStages(barrier.m_stagesDst);
            const u64 rawBarrier = BitUtils::BitfieldInsert<u64>(before, after, 32, 32);
            barriers.emplace(rawBarrier);
        }

        for (const u64 rawBarrier: barriers)
        {
            const MTL::Stages before = static_cast<u32>(rawBarrier);
            const MTL::Stages after = static_cast<u32>(rawBarrier >> 32);

            switch (_barriers.m_placementType)
            {
            case BarrierPlacementType::IntraEncoder:
                encoder->barrierAfterEncoderStages(after, before, MTL4::VisibilityOptionDevice);
                break;
            case BarrierPlacementType::Producer:
                encoder->barrierAfterStages(after, before, MTL4::VisibilityOptionDevice);
                break;
            case BarrierPlacementType::Consumer:
                encoder->barrierAfterQueueStages(after, before, MTL4::VisibilityOptionDevice);
                break;
            }
        }
    }

    ShaderModuleHandle MetalGraphicsContext::RegisterShaderModule(void* _bytecodeData, const u64 _bytecodeSize)
    {
        return m_resources.LoadLibrary(*m_device, _bytecodeData, _bytecodeSize);
    }

    DescriptorSetLayoutHandle MetalGraphicsContext::CreateDescriptorSetLayout(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices)
    {
        return m_argumentBufferManager.CreateArgumentDescriptor(_desc, _bindingIndices);
    }

    DescriptorSetHandle MetalGraphicsContext::CreateDescriptorSet(const DescriptorSetLayoutHandle _layout)
    {
        return m_argumentBufferManager.CreateArgumentBuffer(*m_device, m_resources, _layout);
    }

    PipelineLayoutHandle MetalGraphicsContext::CreatePipelineLayout(const PipelineLayoutDesc& _desc)
    {
        return m_argumentBufferManager.CreatePipelineLayout(_desc);
    }

    GraphicsPipelineHandle MetalGraphicsContext::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc)
    {
        return m_resources.CreateGraphicsPso(*m_device, m_argumentBufferManager, _desc);
    }

    bool MetalGraphicsContext::DestroyGraphicsPipeline(const GraphicsPipelineHandle _pipeline)
    {
        return m_resources.DestroyGraphicsPso(_pipeline);
    }

    bool MetalGraphicsContext::DestroyPipelineLayout(const PipelineLayoutHandle _layout)
    {
        return m_argumentBufferManager.DestroyPipelineLayout(_layout);
    }

    bool MetalGraphicsContext::DestroyDescriptorSet(const DescriptorSetHandle _set)
    {
        return m_argumentBufferManager.DestroyArgumentBuffer(_set, m_resources);
    }

    bool MetalGraphicsContext::DestroyDescriptorSetLayout(const DescriptorSetLayoutHandle _layout)
    {
        return m_argumentBufferManager.DeleteArgumentDescriptor(_layout);
    }

    bool MetalGraphicsContext::FreeShaderModule(const ShaderModuleHandle _module)
    {
        return m_resources.FreeLibrary(_module);
    }

    ComputePipelineHandle MetalGraphicsContext::CreateComputePipeline(const ComputePipelineDesc& _desc)
    {
        return m_resources.CreateComputePso(*m_device, m_argumentBufferManager, _desc);;
    }

    bool MetalGraphicsContext::DestroyComputePipeline(const ComputePipelineHandle _pipeline)
    {
        return m_resources.DestroyComputePso(_pipeline);;
    }

    void MetalGraphicsContext::UpdateDescriptorSet(
        const DescriptorSetHandle _descriptorSet, const eastl::span<const DescriptorSetWriteInfo>& _writes,
        const bool _singleFrame)
    {
        m_argumentBufferManager.UpdateArgumentBuffer(
            m_resources,
            _writes,
            _singleFrame,
            _descriptorSet,
            m_frameId % m_frameContextCount);
    }

    void MetalGraphicsContext::SetViewport(const RenderCommandEncoderHandle _renderEncoder, const Viewport& _viewport)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);

        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
        encoder->setViewport({
            .originX = static_cast<double>(_viewport.m_topLeftX),
            .originY = static_cast<double>(_viewport.m_topLeftY),
            .width = static_cast<double>(_viewport.m_width),
            .height = static_cast<double>(_viewport.m_height),
            .znear = static_cast<double>(_viewport.m_minDepth),
            .zfar = static_cast<double>(_viewport.m_maxDepth),
        });
    }

    void MetalGraphicsContext::SetScissorsRect(const RenderCommandEncoderHandle _renderEncoder, const Rect& _rect)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());

        encoder->setScissorRect({
            .x = _rect.m_left,
            .y = _rect.m_top,
            .width = _rect.m_right - _rect.m_left,
            .height = _rect.m_bottom - _rect.m_top,
        });
    }

    void MetalGraphicsContext::SetIndexBuffer(
        const RenderCommandEncoderHandle _renderEncoder,
        const BufferSpan& _indexBufferView, const bool _isU16)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        renderState->m_indexBufferView = _indexBufferView;
        renderState->m_indexBufferIsU16 = _isU16;
    }

    void MetalGraphicsContext::SetVertexBuffers(
        const RenderCommandEncoderHandle _renderEncoder,
        const eastl::span<const BufferSpan>& _bufferViews)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
        auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        u32 i = 0;
        for (const auto& bufferView : _bufferViews)
        {
            const MTL::Buffer* buffer = m_resources.m_buffers.Get(bufferView.m_buffer.m_handle)->m_buffer;
            renderState->m_vertexArgumentTable->setAddress(
                buffer->gpuAddress() + bufferView.m_offset,
                i + MetalConstants::kVertexStreamBuffersOffset);
            ++i;
        }
    }

    void MetalGraphicsContext::SetGraphicsPipeline(
        const RenderCommandEncoderHandle _renderEncoder,
        const GraphicsPipelineHandle _graphicsPipeline)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
        auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        const MetalResources::GraphicsPsoHotData* graphicsPsoData = m_resources.m_graphicsPso.Get(_graphicsPipeline.m_handle);

        encoder->setRenderPipelineState(graphicsPsoData->m_pso);

        renderState->m_topology = graphicsPsoData->m_topology;
        if (memcmp(&renderState->m_dynamicState, &graphicsPsoData->m_staticState, sizeof(RenderDynamicState)) != 0)
        {
            RenderDynamicState& current = renderState->m_dynamicState;
            const RenderDynamicState& ref = graphicsPsoData->m_staticState;

            if (!graphicsPsoData->m_dynamicBlendFactor && (current.m_blendFactor != ref.m_blendFactor))
            {
                encoder->setBlendColor(
                    ref.m_blendFactor.r,
                    ref.m_blendFactor.g,
                    ref.m_blendFactor.b,
                    ref.m_blendFactor.a);
                current.m_blendFactor = ref.m_blendFactor;
            }

            if (current.m_depthStencilHash != ref.m_depthStencilHash)
            {
                encoder->setDepthStencilState(graphicsPsoData->m_depthStencilState);
                current.m_depthStencilHash = ref.m_depthStencilHash;
            }

            if (memcmp(&current.m_depthBias, &ref.m_depthBias, 3 * sizeof(float)) != 0)
            {
                encoder->setDepthBias(ref.m_depthBias, ref.m_depthBiasSlope, ref.m_depthBiasClamp);
                current.m_depthBias = ref.m_depthBias;
                current.m_depthBiasSlope = ref.m_depthBiasSlope;
                current.m_depthBiasClamp = ref.m_depthBiasClamp;
            }

            if (current.m_fillMode != ref.m_fillMode)
            {
                encoder->setTriangleFillMode(MetalConverters::GetTriangleFillMode(ref.m_fillMode));
                current.m_fillMode = ref.m_fillMode;
            }

            if (current.m_cullMode != ref.m_cullMode)
            {
                encoder->setCullMode(MetalConverters::GetCullMode(ref.m_cullMode));
                current.m_cullMode = ref.m_cullMode;
            }

            if (current.m_front != ref.m_front)
            {
                encoder->setFrontFacingWinding(MetalConverters::GetWinding(ref.m_front));
                current.m_front = ref.m_front;
            }

            if (current.m_depthClip != ref.m_depthClip)
            {
                encoder->setDepthClipMode(ref.m_depthClip ? MTL::DepthClipModeClip : MTL::DepthClipModeClamp);
                current.m_depthClip = ref.m_depthClip;
            }

            if (!graphicsPsoData->m_dynamicStencilRef && current.m_stencilRefValue != ref.m_stencilRefValue)
            {
                encoder->setStencilReferenceValue(ref.m_stencilRefValue);
                current.m_stencilRefValue = ref.m_stencilRefValue;
            }
        }
    }

    void MetalGraphicsContext::SetGraphicsPushConstant(
        const RenderCommandEncoderHandle _renderEncoder,
        const PipelineLayoutHandle _layout,
        const eastl::span<const u32>& _data,
        const u32 _index,
        u32 /*_offset*/)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        const auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        const MetalArgumentBufferManager::PushConstantData& pushConstantData =
            m_argumentBufferManager.m_pipelineLayouts.Get(_layout.m_handle)->m_pushConstantsData[_index];

        for (auto& data: pushConstantData.m_data)
        {
            if (BitUtils::EnumHasAny(data.m_visibility, ShaderVisibility::Vertex))
            {
                renderState->m_vertexArgumentTable->setAddress(
                    m_byteUploader->SetBytes<u32>(m_device.get(), m_resources, _data, m_frameId % m_frameContextCount),
                    data.m_bufferIndex);
            }
            if (BitUtils::EnumHasAny(data.m_visibility, ShaderVisibility::Fragment))
            {
                renderState->m_fragmentArgumentTable->setAddress(
                    m_byteUploader->SetBytes<u32>(m_device.get(), m_resources, _data, m_frameId % m_frameContextCount),
                    data.m_bufferIndex);
            }
        }
    }

    void MetalGraphicsContext::SetGraphicsDescriptorSetsWithOffset(
        const RenderCommandEncoderHandle _renderEncoder,
        const PipelineLayoutHandle _layout,
        const eastl::span<const DescriptorSetHandle>& _sets,
        const u32 _offset)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        const auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        const MetalArgumentBufferManager::PipelineLayoutHotData& layoutData =
            *m_argumentBufferManager.m_pipelineLayouts.Get(_layout.m_handle);


        const u8 frameIndex = m_frameId % m_frameContextCount;
        for (u32 i = 0; i < _sets.size(); i++)
        {
            const u32 index = _offset + i;
            const ShaderVisibility visibility = layoutData.m_setVisibilities[index];
            const MetalArgumentBufferManager::ArgumentBufferHotData& argBuffer =
                *m_argumentBufferManager.m_argumentBufferSets.Get(_sets[i].m_handle);

            if (BitUtils::EnumHasAny(visibility, ShaderVisibility::Vertex))
            {
                renderState->m_vertexArgumentTable->setAddress(
                    argBuffer.m_argumentBuffer->gpuAddress() + frameIndex * argBuffer.m_encoder->encodedLength(),
                    index);
            }
            if (BitUtils::EnumHasAny(visibility, ShaderVisibility::Fragment))
            {
                renderState->m_fragmentArgumentTable->setAddress(
                    argBuffer.m_argumentBuffer->gpuAddress() + frameIndex * argBuffer.m_encoder->encodedLength(),
                    index);
            }
        }
    }

    void MetalGraphicsContext::DrawInstanced(
        const RenderCommandEncoderHandle _renderEncoder,
        const DrawInstancedDesc& _desc)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
        const auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        KE_AUTO_RELEASE_POOL;
        encoder->drawPrimitives(
            MetalConverters::GetPrimitiveType(renderState->m_topology),
            _desc.m_vertexOffset,
            _desc.m_vertexCount,
            _desc.m_instanceCount,
            _desc.m_instanceOffset);
    }

    void MetalGraphicsContext::DrawIndexedInstanced(
        const RenderCommandEncoderHandle _renderEncoder,
        const DrawIndexedInstancedDesc& _desc)
    {
        const auto commandList = static_cast<CommandList>(_renderEncoder.m_handle);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Render);
        auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
        const auto* renderState = static_cast<RenderState*>(commandList->m_userData);
        KE_ASSERT_FATAL(renderState != nullptr);

        const MTL::IndexType indexType = renderState->m_indexBufferIsU16 ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
        const size_t indexBufferOffset = renderState->m_indexBufferView.m_offset + _desc.m_indexOffset * (renderState->m_indexBufferIsU16 ? sizeof(u16) : sizeof(u32));
        const size_t indexBufferSize = _desc.m_elementCount * (renderState->m_indexBufferIsU16 ? sizeof(u16) : sizeof(u32));

        const MTL::Buffer* indexBuffer = m_resources.m_buffers.Get(renderState->m_indexBufferView.m_buffer.m_handle)->m_buffer;

        KE_AUTO_RELEASE_POOL;
        encoder->drawIndexedPrimitives(
            MetalConverters::GetPrimitiveType(renderState->m_topology),
            _desc.m_elementCount,
            indexType,
            indexBuffer->gpuAddress() + indexBufferOffset,
            indexBufferSize,
            _desc.m_instanceCount,
            _desc.m_vertexOffset,
            _desc.m_instanceOffset);
    }

    void MetalGraphicsContext::SetComputePipeline(
        const CommandListHandle _commandList, const ComputePipelineHandle _pipeline)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Compute);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());

        MetalResources::ComputePsoHotData* hot = m_resources.m_computePso.Get(_pipeline.m_handle);
        encoder->setComputePipelineState(hot->m_pso);
    }

    void MetalGraphicsContext::SetComputeDescriptorSetsWithOffset(
        const CommandListHandle _commandList,
        const PipelineLayoutHandle _layout,
        const eastl::span<const DescriptorSetHandle> _sets,
        const u32 _offset)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Compute);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());
        auto* argumentTable = static_cast<MTL4::ArgumentTable*>(commandList->m_userData);
        KE_ASSERT(argumentTable != nullptr);

        const MetalArgumentBufferManager::PipelineLayoutHotData* layoutData = m_argumentBufferManager.m_pipelineLayouts.Get(_layout.m_handle);
        const u8 frameIndex = m_frameId % m_frameContextCount;

        for (u32 i = 0; i < _sets.size(); ++i)
        {
            const u32 index = i + _offset;

            const ShaderVisibility visibility = layoutData->m_setVisibilities[index];
            KE_ASSERT(BitUtils::EnumHasAny(visibility, ShaderVisibility::Compute) || visibility == ShaderVisibility::None);

            const MetalArgumentBufferManager::ArgumentBufferHotData& argBuffer =
                    *m_argumentBufferManager.m_argumentBufferSets.Get(_sets[i].m_handle);

            argumentTable->setAddress(
                argBuffer.m_argumentBuffer->gpuAddress() + frameIndex * argBuffer.m_encoder->encodedLength(),
                index);
        }
    }

    void MetalGraphicsContext::SetComputePushConstant(
        const CommandListHandle _commandList, const PipelineLayoutHandle _layout, const eastl::span<const u32> _data)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Compute);
        auto* argumentTable = static_cast<MTL4::ArgumentTable*>(commandList->m_userData);
        KE_ASSERT(argumentTable != nullptr);

        const MetalArgumentBufferManager::PushConstantData& pushConstantData =
            m_argumentBufferManager.m_pipelineLayouts.Get(_layout.m_handle)->m_pushConstantsData[0];

        KE_ASSERT(pushConstantData.m_data.size() == 1);
        KE_ASSERT(pushConstantData.m_data[0].m_visibility == ShaderVisibility::Compute);
        argumentTable->setAddress(
            m_byteUploader->SetBytes<u32>(m_device.get(), m_resources, _data, m_frameId % m_frameContextCount),
            pushConstantData.m_data[0].m_bufferIndex);
    }

    void MetalGraphicsContext::Dispatch(
        const CommandListHandle _commandList, const uint3 _threadGroupCount, const uint3 _threadGroupSize)
    {
        const auto commandList = static_cast<CommandList>(_commandList);
        VERIFY_OR_RETURN_VOID(commandList->m_encoder != nullptr && commandList->m_type == CommandListData::EncoderType::Compute);
        auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());

        const MTL::Size threadGroupCount {
            _threadGroupCount.x,
            _threadGroupCount.y,
            _threadGroupCount.z
        };

        const MTL::Size threadGroupSize {
            _threadGroupSize.x,
            _threadGroupSize.y,
            _threadGroupSize.z
        };

        encoder->dispatchThreadgroups(threadGroupCount, threadGroupSize);
    }

    void MetalGraphicsContext::PushDebugMarker(
        const CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color&)
    {
        if (m_appInfo.m_features.m_debugTags == GraphicsCommon::SoftEnable::Disabled)
            return;

        const auto commandList = static_cast<CommandList>(_commandList);

        KE_AUTO_RELEASE_POOL;
        auto* string = NS::String::string(_markerName.data(), NS::UTF8StringEncoding);
        commandList->m_commandBuffer->pushDebugGroup(string);
    }

    void MetalGraphicsContext::PopDebugMarker(const CommandListHandle _commandList)
    {
        if (m_appInfo.m_features.m_debugTags == GraphicsCommon::SoftEnable::Disabled)
            return;

        const auto commandList = static_cast<CommandList>(_commandList);

        commandList->m_commandBuffer->popDebugGroup();
    }

    void MetalGraphicsContext::InsertDebugMarker(
        const CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color&)
    {
        if (m_appInfo.m_features.m_debugTags == GraphicsCommon::SoftEnable::Disabled)
            return;

        const auto commandList = static_cast<CommandList>(_commandList);

        if (commandList->m_encoder == nullptr)
            return;

        KE_AUTO_RELEASE_POOL;
        auto* string = NS::String::string(_markerName.data(), NS::UTF8StringEncoding);
        commandList->m_encoder->insertDebugSignpost(string);
    }

    void MetalGraphicsContext::CalibrateCpuGpuClocks()
    {
        m_calibrateCpuGpuClocks = true;
        m_timestampConversion.m_gpuFrequency = static_cast<double>(m_device->queryTimestampFrequency()) / 1e9;
        m_timestampConversion.m_cpuReference = tracy::Profiler::GetTime();
        MTL::Timestamp cpu, gpu;
        m_device->sampleTimestamps(&cpu, &gpu);
        gpu = static_cast<u64>(static_cast<double>(gpu) * m_timestampConversion.m_gpuFrequency); // Convert from ns to ticks
        m_timestampConversion.m_gpuReference = gpu;
    }

    TimestampHandle MetalGraphicsContext::PutTimestamp(const CommandListHandle _commandList)
    {
        const u8 frameIndex = m_frameId % m_frameContextCount;
        MetalFrameContext& frameContext = m_frameContexts[frameIndex];

        if (frameContext.m_sampleCounterHeap.get() == nullptr)
        {
            return { ~0u, ~0u };
        }
        const auto commandList = static_cast<CommandList>(_commandList);

        const u32 index = frameContext.AllocateTimestamp();

        if (commandList->m_encoder != nullptr)
        {
            switch (commandList->m_type)
            {
            case CommandListData::EncoderType::Render:
            {
                auto* encoder = reinterpret_cast<MTL4::RenderCommandEncoder*>(commandList->m_encoder.get());
                encoder->writeTimestamp(
                    MTL4::TimestampGranularityRelaxed,
                    MetalConstants::kAllRenderStages,
                    frameContext.m_sampleCounterHeap.get(),
                    index);
                break;
            }
            case CommandListData::EncoderType::Transfer:
            case CommandListData::EncoderType::Compute:
            {
                auto* encoder = reinterpret_cast<MTL4::ComputeCommandEncoder*>(commandList->m_encoder.get());
                encoder->writeTimestamp(
                    MTL4::TimestampGranularityRelaxed,
                    frameContext.m_sampleCounterHeap.get(),
                    index);
                break;
            }
            case CommandListData::EncoderType::None:
                commandList->m_commandBuffer->writeTimestampIntoHeap(frameContext.m_sampleCounterHeap.get(), index);
                break;
            }
        }
        else
        {
            commandList->m_commandBuffer->writeTimestampIntoHeap(frameContext.m_sampleCounterHeap.get(), index);
        }

        return { index, static_cast<u32>(m_frameId) };
    }

    u64 MetalGraphicsContext::GetResolvedTimestamp(const TimestampHandle _timestamp) const
    {
        if (m_lastResolvedFrameId == ~0ull)
            return 0;
        if (_timestamp.m_frameId > static_cast<u32>(m_lastResolvedFrameId) || _timestamp.m_frameId + m_frameContextCount <= static_cast<u32>(m_lastResolvedFrameId))
            return 0;

        const MetalFrameContext& frameContext = m_frameContexts[_timestamp.m_frameId % m_frameContextCount];

        if (frameContext.m_resolvedTimestamps.Empty() || frameContext.m_resolvedTimestamps.Size() <= _timestamp.m_index)
            return 0;
        return frameContext.m_resolvedTimestamps[_timestamp.m_index];
    }

    eastl::span<const u64> MetalGraphicsContext::GetResolvedTimestamps(const u64 _frameId) const
    {
        if (m_lastResolvedFrameId == ~0ull)
            return {};
        if (_frameId > m_lastResolvedFrameId || _frameId + m_frameContextCount <= m_lastResolvedFrameId)
            return {};

        const MetalFrameContext& frameContext = m_frameContexts[_frameId % m_frameContextCount];
        return { frameContext.m_resolvedTimestamps.Data(), frameContext.m_resolvedTimestamps.Size() };
    }
}
