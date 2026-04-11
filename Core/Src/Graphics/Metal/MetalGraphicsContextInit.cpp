/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2024.
 */

#include "Graphics/Metal/MetalGraphicsContext.hpp"
#include <cstdio>

#include "Graphics/Metal/MetalFrameContext.hpp"
#include "Graphics/Metal/MetalSwapChain.hpp"
#include "KryneEngine/Core/Window/Window.hpp"

namespace KryneEngine
{
    MetalGraphicsContext::MetalGraphicsContext(
        const AllocatorInstance _allocator,
        const GraphicsCommon::ApplicationInfo& _appInfo,
        Window* _window)
        : GraphicsContext(_allocator, _appInfo, _window)
        , m_frameContexts(_allocator)
        , m_resources(_allocator)
        , m_argumentBufferManager(_allocator)
    {
        m_device = MTL::CreateSystemDefaultDevice();

        KE_ASSERT(m_device->supportsFamily(MTL::GPUFamilyMetal3) || m_device->supportsFamily(MTL::GPUFamilyMac2));

        if (_appInfo.m_features.m_graphics)
        {
            // Catch internal auto release
            KE_AUTO_RELEASE_POOL;
            m_graphicsQueue = m_device->newCommandQueue();
        }

        if (_appInfo.m_features.m_compute)
        {
            if (_appInfo.m_features.m_asyncCompute || m_graphicsQueue == nullptr)
            {
                // Catch internal auto release
                KE_AUTO_RELEASE_POOL;
                m_computeQueue = m_device->newCommandQueue();
            }
        }

        if (_appInfo.m_features.m_transfer)
        {
            if (_appInfo.m_features.m_transferQueue || (m_computeQueue == nullptr && m_graphicsQueue == nullptr))
            {
                NsPtr<MTL::IOCommandQueueDescriptor> descriptor { MTL::IOCommandQueueDescriptor::alloc()->init() };

                // Catch internal auto release
                KE_AUTO_RELEASE_POOL;
                m_ioQueue = m_device->newIOCommandQueue(descriptor.get(), nullptr);
            }
        }

        u32 gpuTimestampBufferCapacity = 0;
        if (_appInfo.m_features.m_gpuTimestamps != GraphicsCommon::SoftEnable::Disabled)
        {
            for (auto i = 0; i < m_device->counterSets()->count(); ++i)
            {
                auto* counterSet = reinterpret_cast<MTL::CounterSet*>(m_device->counterSets()->object(i));
                if (counterSet->name()->isEqualToString(MTL::CommonCounterSetTimestamp))
                {
                    m_supportsDrawBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtDrawBoundary);
                    m_supportsDispatchBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtDispatchBoundary);
                    m_supportsBlitBoundarySampling = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtBlitBoundary);

                    gpuTimestampBufferCapacity = m_appInfo.m_features.m_gpuTimestampBufferCapacity;
                }
            }
        }

        m_frameContextCount = 2;
        const u8 frameIndex = m_frameId % m_frameContextCount;

        KE_ASSERT_FATAL_MSG(!_appInfo.m_features.m_present || _appInfo.m_features.m_graphics,
                            "Metal graphics context does not support presentation without graphics queue");
        KE_ASSERT_FATAL(!(_appInfo.m_features.m_present ^ (_window != nullptr)));
        if (_appInfo.m_features.m_present)
        {
            m_swapChain.Init(_allocator, *m_device, _appInfo, _window, m_resources, frameIndex);
            m_frameContextCount = m_swapChain.m_textures.Size();
        }

        m_frameContexts.Resize(m_frameContextCount);
        m_frameContexts.InitAll(
            m_device.get(),
            _allocator,
            gpuTimestampBufferCapacity,
            m_graphicsQueue != nullptr,
            m_computeQueue != nullptr,
            m_ioQueue != nullptr,
            m_appInfo.m_features.m_validationLayers != GraphicsCommon::SoftEnable::Disabled);

        m_frameContexts[frameIndex].PrepareForNextFrame(m_frameId);

        m_argumentBufferManager.Init(m_frameContextCount, frameIndex);
    }

    MetalGraphicsContext::~MetalGraphicsContext()
    {
        WaitForLastFrame();
    }
}