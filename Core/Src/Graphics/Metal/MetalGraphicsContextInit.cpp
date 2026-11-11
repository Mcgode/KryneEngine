/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2024.
 */

#include "Graphics/Metal/MetalGraphicsContext.hpp"

#include "Graphics/Metal/MetalFrameContext.hpp"
#include "Graphics/Metal/MetalSwapChain.hpp"
#include "Graphics/Metal/Helpers/ByteUploader.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

namespace KryneEngine
{
    MetalGraphicsContext::MetalGraphicsContext(
        const AllocatorInstance _allocator,
        const GraphicsCommon::ApplicationInfo& _appInfo)
        : GraphicsContext(_allocator, _appInfo)
        , m_frameContexts(_allocator)
        , m_resources(_allocator)
        , m_argumentBufferManager(_allocator)
    {}

    GraphicsCommon::ContextCreationError MetalGraphicsContext::Initialize()
    {
        using GraphicsCommon::ContextCreationError;

        if (!m_appInfo.IsMetalApi())
        {
            return ContextCreationError::ApiNotSupported;
        }

        KE_ASSERT_FATAL_MSG(!m_appInfo.m_features.m_present || m_appInfo.m_features.m_graphics,
                            "Metal graphics context does not support presentation without graphics queue");

        // Metal has no geometry shader stage.
        if (m_appInfo.m_features.m_geometryShader == GraphicsCommon::SoftEnable::ForceEnabled)
        {
            return ContextCreationError::GeometryShaderNotSupported;
        }

        m_device = MTL::CreateSystemDefaultDevice();
        if (m_device == nullptr)
        {
            return ContextCreationError::NoCompatibleDevice;
        }

        if (!m_device->supportsFamily(MTL::GPUFamilyMetal4))
        {
            return ContextCreationError::ApiNotSupported;
        }

        {
            MTL::ResidencySetDescriptor* descriptor = MTL::ResidencySetDescriptor::alloc()->init();
            descriptor->setInitialCapacity(128);

            NS::Error* error = nullptr;
            m_resources.m_residencySet = NS::TransferPtr(m_device->newResidencySet(descriptor, &error));
            descriptor->release();
            if (error != nullptr)
            {
                return ContextCreationError::DeviceCreationFailed;
            }
        }

        if (m_appInfo.m_features.m_graphics)
        {
            // Catch internal auto release
            KE_AUTO_RELEASE_POOL;

            const NsPtr descriptor { MTL4::CommandQueueDescriptor::alloc()->init() };
            descriptor->setLabel(MTLSTR("Graphics Queue"));

            m_graphicsQueue = m_device->newMTL4CommandQueue(descriptor.get(), nullptr);
            if (m_graphicsQueue == nullptr)
            {
                return ContextCreationError::DeviceCreationFailed;
            }
            m_graphicsQueue->addResidencySet(m_resources.m_residencySet.get());
        }

        if (m_appInfo.m_features.m_compute)
        {
            if (m_appInfo.m_features.m_asyncCompute || m_graphicsQueue == nullptr)
            {
                // Catch internal auto release
                KE_AUTO_RELEASE_POOL;

                const NsPtr descriptor { MTL4::CommandQueueDescriptor::alloc()->init() };
                descriptor->setLabel(MTLSTR("Compute Queue"));

                m_computeQueue = m_device->newMTL4CommandQueue(descriptor.get(), nullptr);
                if (m_computeQueue == nullptr)
                {
                    return ContextCreationError::DeviceCreationFailed;
                }
                m_computeQueue->addResidencySet(m_resources.m_residencySet.get());
            }
        }

        if (m_appInfo.m_features.m_transfer)
        {
            if (m_appInfo.m_features.m_transferQueue || (m_computeQueue == nullptr && m_graphicsQueue == nullptr))
            {
                // Catch internal auto release
                KE_AUTO_RELEASE_POOL;

                const NsPtr descriptor { MTL4::CommandQueueDescriptor::alloc()->init() };
                descriptor->setLabel(MTLSTR("IO Queue"));

                m_ioQueue = m_device->newMTL4CommandQueue(descriptor.get(), nullptr);
                if (m_ioQueue == nullptr)
                {
                    return ContextCreationError::DeviceCreationFailed;
                }
                m_ioQueue->addResidencySet(m_resources.m_residencySet.get());
            }
        }

        u32 gpuTimestampBufferCapacity = 0;
        if (m_appInfo.m_features.m_gpuTimestamps != GraphicsCommon::SoftEnable::Disabled)
        {
            bool timestampCounterSetFound = false;
            for (auto i = 0; i < m_device->counterSets()->count(); ++i)
            {
                auto* counterSet = reinterpret_cast<MTL::CounterSet*>(m_device->counterSets()->object(i));
                if (counterSet->name()->isEqualToString(MTL::CommonCounterSetTimestamp))
                {
                    m_supportsDrawBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtDrawBoundary);
                    m_supportsDispatchBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtDispatchBoundary);
                    m_supportsBlitBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtBlitBoundary);

                    gpuTimestampBufferCapacity = m_appInfo.m_features.m_gpuTimestampBufferCapacity;
                    timestampCounterSetFound = true;
                }
            }

            if (!timestampCounterSetFound
                && m_appInfo.m_features.m_gpuTimestamps == GraphicsCommon::SoftEnable::ForceEnabled)
            {
                return ContextCreationError::GpuTimestampsNotSupported;
            }
        }

        // Frame context count is an explicit, strict choice — not negotiated against the swap chain.
        m_frameContextCount = static_cast<u8>(m_appInfo.m_bufferingMode);
        const u8 frameIndex = m_frameId % m_frameContextCount;

        m_frameContexts.Resize(m_frameContextCount);
        m_frameContexts.InitAll(
            m_device.get(),
            m_allocator,
            gpuTimestampBufferCapacity,
            m_graphicsQueue.get(),
            m_computeQueue.get(),
            m_ioQueue.get(),
            m_appInfo.m_features.m_validationLayers != GraphicsCommon::SoftEnable::Disabled);

        m_frameContexts[frameIndex].PrepareForNextFrame(m_frameId);

        m_argumentBufferManager.Init(m_frameContextCount, frameIndex);

        m_byteUploader = m_allocator.New<ByteUploader>(m_allocator, m_frameContextCount);

        return ContextCreationError::None;
    }

    MetalGraphicsContext::~MetalGraphicsContext()
    {
        // Initialization may have failed before any frame context was created.
        if (m_frameContextCount > 0)
        {
            WaitForLastFrame();
        }
        m_allocator.Delete(m_byteUploader);
    }
}