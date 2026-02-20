/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2022.
 */

#include "Graphics/Vulkan/VkGraphicsContext.hpp"

#include <iostream>
#include <regex>
#include <EASTL/algorithm.h>
#include <EASTL/vector_map.h>
#include <GLFW/glfw3.h>

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkDebugHandler.hpp"
#include "Graphics/Vulkan/VkSurface.hpp"
#include "Graphics/Vulkan/VkSwapChain.hpp"
#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/Drawing.hpp"
#include "KryneEngine/Core/Math/Color.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.inl"
#include "KryneEngine/Core/Profiling/TracyGpuProfilerContext.hpp"
#include "KryneEngine/Core/Window/Window.hpp"
#if defined(WIN32)
#   include "KryneEngine/Core/Platform/Windows.h"
#endif

#if !VK_KHR_portability_subset
#   define VK_KHR_portability_subset 1
#   define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
typedef struct VkPhysicalDevicePortabilitySubsetFeaturesKHR
{
    VkStructureType    sType;
    void*              pNext;
    VkBool32           constantAlphaColorBlendFactors;
    VkBool32           events;
    VkBool32           imageViewFormatReinterpretation;
    VkBool32           imageViewFormatSwizzle;
    VkBool32           imageView2DOn3DImage;
    VkBool32           multisampleArrayImage;
    VkBool32           mutableComparisonSamplers;
    VkBool32           pointPolygons;
    VkBool32           samplerMipLodBias;
    VkBool32           separateStencilMaskRef;
    VkBool32           shaderSampleRateInterpolationFunctions;
    VkBool32           tessellationIsolines;
    VkBool32           tessellationPointMode;
    VkBool32           triangleFans;
    VkBool32           vertexAttributeAccessBeyondStride;
} VkPhysicalDevicePortabilitySubsetFeaturesKHR;
#endif

namespace KryneEngine
{
    using namespace VkHelperFunctions;
    using namespace VkCommonStructures;

    namespace
    {
        constexpr eastl::array<const char*> kValidationLayerNames = {
                "VK_LAYER_KHRONOS_validation"
        };

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT _messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT _messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
                void* _pUserData)
        {
            eastl::string severity = "|";
            if (_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                severity += "verbose|";
            }
            if (_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                severity += "info|";
            }
            if (_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                severity += "warning|";
            }
            if (_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                severity += "error|";
            }

            // Ignored messages
            {
                eastl::string message = _pCallbackData->pMessage;
                if (std::regex_match(message.begin(), message.end(), std::regex("Layer name .+ does not conform to naming standard .*"))
                    || std::regex_match(message.begin(), message.end(), std::regex("Override layer has override paths set to .*")))
                {
                    return VK_FALSE;
                }
            }

            if (_messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                std::cout << "Validation layer (" << severity.c_str() << "): " << _pCallbackData->pMessage << std::endl;
            }

            KE_ERROR(_pCallbackData->pMessage);

            return VK_FALSE;
        }
    }

    VkGraphicsContext::VkGraphicsContext(
        AllocatorInstance _allocator,
        const GraphicsCommon::ApplicationInfo& _appInfo,
        Window* _window)
        : GraphicsContext(_allocator, _appInfo, _window)
        , m_surface(_allocator)
        , m_swapChain(_allocator)
        , m_resources(_allocator)
        , m_descriptorSetManager(_allocator)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::VkGraphicsContext");

        {
            KE_ZoneScoped("VkInstance creation");

            const VkApplicationInfo applicationInfo{
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = m_appInfo.m_applicationName.c_str(),
                .applicationVersion = MakeVersion(m_appInfo.m_applicationVersion),
                .pEngineName = "KryneEngine2",
                .engineVersion = MakeVersion(m_appInfo.m_engineVersion),
                .apiVersion = GetApiVersion(m_appInfo.m_api)};

            VkInstanceCreateInfo instanceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if defined(__APPLE__)
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#else
                .flags = 0,
#endif
                .pApplicationInfo = &applicationInfo,
                .enabledLayerCount = 0,
            };

            DynamicArray<VkExtensionProperties> availableExtensions;
            VkHelperFunctions::VkArrayFetch(availableExtensions, vkEnumerateInstanceExtensionProperties, nullptr);

            VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
            if (m_appInfo.m_features.m_validationLayers)
            {
                _PrepareValidationLayers(instanceCreateInfo);
                debugMessengerCreateInfo = _PopulateDebugCreateInfo(this);
                instanceCreateInfo.pNext = &debugMessengerCreateInfo;
            }

            auto extensions = _RetrieveRequiredExtensionNames(m_appInfo);
            _RetrieveOptionalExtensionNames(extensions, availableExtensions, m_appInfo);
            instanceCreateInfo.enabledExtensionCount = extensions.size();
            instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

            VkAssert(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));
        }

        if (m_appInfo.m_features.m_validationLayers)
        {
            _SetupValidationLayersCallback();
        }

        if (m_appInfo.m_features.m_present)
        {
            m_surface.Init(m_instance, _window->GetGlfwWindow());
        }

        _SelectPhysicalDevice();

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

        m_optimalRowPitchAlignment = physicalDeviceProperties.limits.optimalBufferCopyRowPitchAlignment;

        if (physicalDeviceProperties.limits.timestampPeriod > 0.f && physicalDeviceProperties.limits.timestampComputeAndGraphics)
        {
            m_gpuTimestampPeriod = physicalDeviceProperties.limits.timestampPeriod;
            m_supportsTimestampQueries = true;
        }

        if (m_appInfo.m_features.m_present)
        {
            m_surface.UpdateCapabilities(m_physicalDevice);
        }

        _CreateDevice();

        m_resources.InitAllocator(m_appInfo, m_device, m_physicalDevice, m_instance);

#if !defined(KE_FINAL)
        m_debugHandler = eastl::make_shared<VkDebugHandler>();
        *m_debugHandler = VkDebugHandler::Initialize(m_device, m_debugUtils, m_debugMarkers);
        m_resources.m_debugHandler = m_debugHandler;
#endif

        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.Init(
                    m_appInfo,
                    m_device,
                    m_surface,
                    m_resources,
                    _window->GetGlfwWindow(),
                    m_queueIndices,
                    m_frameId,
                    nullptr);

#if !defined(KE_FINAL)
            m_swapChain.SetDebugHandler(m_debugHandler, m_device);
#endif

            m_frameContextCount = m_swapChain.m_renderTargetViews.Size();
        }
        else
        {
            // If no display, keep double buffering.
            m_frameContextCount = 2;
        }

        {
            KE_ZoneScoped("Frame contexts init");

            m_frameContexts.Resize(m_frameContextCount);
            m_frameContexts.InitAll(
                m_allocator,
                m_device,
                m_queueIndices,
                m_supportsTimestampQueries && m_supportsTimestampCalibration ? m_appInfo.m_features.m_gpuTimestampBufferCapacity : 0);

            m_frameContexts[m_frameId % m_frameContextCount].m_frameId = m_frameId;

#if !defined(KE_FINAL)
            for (auto i = 0u; i < m_frameContextCount; i++)
            {
                m_frameContexts[i].SetDebugHandler(m_debugHandler, m_device, i);
            }
#endif
        }

        CalibrateCpuGpuClocks();

        m_descriptorSetManager.Init(m_frameContextCount, m_frameId % m_frameContextCount);
    }

    VkGraphicsContext::~VkGraphicsContext()
    {
        for (auto& frameContext: m_frameContexts)
        {
            frameContext.Destroy(m_device);
        }
        m_frameContexts.Clear();

        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.Destroy(m_device, m_resources);
            m_surface.Destroy(m_instance);
        }

        m_resources.DestroyAllocator();

        vkDestroyDevice(m_device, nullptr);
        if (m_debugMessenger)
        {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(m_instance, m_debugMessenger, nullptr);
            }
        }
        vkDestroyInstance(m_instance, nullptr);
    }

    bool VkGraphicsContext::IsFrameExecuted(KryneEngine::u64 _frameId) const
    {
        const u8 frameIndex = _frameId % m_frameContextCount;
        return m_frameContexts[frameIndex].m_frameId > _frameId;
    }

    void VkGraphicsContext::WaitForFrame(u64 _frameId) const
    {
        KE_ZoneScopedFunction("VkGraphicsContext::WaitForFrame");

        const u8 frameIndex = _frameId % m_frameContextCount;
        m_frameContexts[frameIndex].WaitForFences(m_device, _frameId);

        const u64 start = _frameId > kInitialFrameId + m_frameContextCount - 1
                              ? _frameId + 1 - m_frameContextCount
                              : kInitialFrameId;
        for (u64 i = start; i <= _frameId; i++)
        {
            m_frameContexts[i % m_frameContextCount].ResolveTimestamps(m_device, m_gpuTimestampPeriod, m_cpuTimestampOffset);
            m_lastResolvedFrame = i;
            if (m_profilerContext != nullptr)
            {
                m_profilerContext->ResolveQueries(this, i);
            }
        }
    }

    bool VkGraphicsContext::HasDedicatedTransferQueue() const
    {
        return m_transferQueue != VK_NULL_HANDLE;
    }
    bool VkGraphicsContext::HasDedicatedComputeQueue() const
    {
        return m_computeQueue != VK_NULL_HANDLE;
    }

    void VkGraphicsContext::InternalEndFrame()
    {
        KE_ZoneScopedFunction("VkGraphicsContext::EndFrame");

        const u8 frameIndex = m_frameId % m_frameContextCount;
        auto& frameContext = m_frameContexts[frameIndex];
        eastl::fixed_vector<VkSemaphore, VkFrameContext::kMaxQueueCount> queueSemaphores(m_allocator);

        VkSemaphore imageAvailableSemaphore;
        if (m_appInfo.m_features.m_present)
        {
            imageAvailableSemaphore = m_swapChain.m_imageAvailableSemaphores[frameIndex];
        }

        // Submit command buffers
        {
            KE_ZoneScoped("Submit non-present queues");

            const auto submitQueue = [&](VkQueue _queue, VkFrameContext::CommandPoolSet& _commandPoolSet)
            {
                if (_queue && !_commandPoolSet.m_usedCommandBuffers.empty())
                {
                    // Reset fence
                    {
                        KE_ASSERT(vkGetFenceStatus(m_device, _commandPoolSet.m_fence) == VK_SUCCESS);
                        VkAssert(vkResetFences(m_device, 1, &_commandPoolSet.m_fence));
                    }

                    constexpr VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                    VkSubmitInfo submitInfo
                        {
                            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

                            .waitSemaphoreCount = m_appInfo.m_features.m_present ? 1u : 0u,
                            .pWaitSemaphores = &imageAvailableSemaphore,
                            .pWaitDstStageMask = stages, // Only need image for render target output

                            .commandBufferCount = static_cast<uint32_t>(_commandPoolSet.m_usedCommandBuffers.size()),
                            .pCommandBuffers = _commandPoolSet.m_usedCommandBuffers.data(),

                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = &_commandPoolSet.m_semaphore,
                        };
                    queueSemaphores.push_back(_commandPoolSet.m_semaphore);

                    VkAssert(vkQueueSubmit(_queue, 1, &submitInfo, _commandPoolSet.m_fence));
                }
            };

            submitQueue(m_transferQueue, frameContext.m_transferCommandPoolSet);
            submitQueue(m_computeQueue, frameContext.m_computeCommandPoolSet);
            submitQueue(m_graphicsQueue, frameContext.m_graphicsCommandPoolSet);
        }

        // Present image
        if (m_appInfo.m_features.m_present) {
            m_swapChain.Present(m_presentQueue, queueSemaphores);
        }

        if (m_profilerContext != nullptr)
        {
            m_profilerContext->EndFrame(m_frameId);
        }

        FrameMark;

        const u64 nextFrameId = m_frameId + 1;
        const u8 nextFrameContextIndex = nextFrameId % m_frameContextCount;
        VkFrameContext& nextFrameContext = m_frameContexts[nextFrameContextIndex];
        if (nextFrameId >= m_frameContextCount)
        {
            nextFrameContext.WaitForFences(m_device, nextFrameId - m_frameContextCount);
            nextFrameContext.m_graphicsCommandPoolSet.Reset();
            nextFrameContext.m_computeCommandPoolSet.Reset();
            nextFrameContext.m_transferCommandPoolSet.Reset();
            if (m_lastResolvedFrame == ~0ull || m_lastResolvedFrame < nextFrameId - m_frameContextCount)
            {
                nextFrameContext.ResolveTimestamps(m_device, m_gpuTimestampPeriod, m_cpuTimestampOffset);
                m_lastResolvedFrame = nextFrameId - m_frameContextCount;
                if (m_profilerContext != nullptr)
                {
                    m_profilerContext->ResolveQueries(this, nextFrameId - m_frameContextCount);
                }
            }
        }
        nextFrameContext.m_frameId = nextFrameId;

        m_resources.FlushPools();
        m_descriptorSetManager.NextFrame(m_device, m_resources, nextFrameContextIndex);

        // Acquire next image
        if (m_appInfo.m_features.m_present)
        {
            m_swapChain.AcquireNextImage(m_device, nextFrameContextIndex);
        }
    }

    void VkGraphicsContext::_PrepareValidationLayers(VkInstanceCreateInfo& _createInfo)
    {
         DynamicArray<VkLayerProperties> availableLayers;
         VkHelperFunctions::VkArrayFetch(availableLayers, vkEnumerateInstanceLayerProperties);

        bool found = false;
        for (const auto &validationLayerName: kValidationLayerNames)
        {
            for (const auto &layerProperty: availableLayers)
            {
                if (strcmp(validationLayerName, layerProperty.layerName) == 0)
                {
                    found = true;
                }
            }

            if (found)
            {
                break;
            }
        }

        if (KE_VERIFY(found))
        {
            _createInfo.ppEnabledLayerNames = kValidationLayerNames.data();
            _createInfo.enabledLayerCount = kValidationLayerNames.size();
        }
    }

    eastl::vector<const char *> VkGraphicsContext::_RetrieveRequiredExtensionNames(const GraphicsCommon::ApplicationInfo& _appInfo)
    {
        u32 glfwCount;
        const char** ppGlfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);

        eastl::vector<const char *> result(ppGlfwExtensions, ppGlfwExtensions + glfwCount, m_allocator);

        if (_appInfo.m_features.m_validationLayers)
        {
            result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            m_debugUtils = true;
        }

#if defined(__APPLE__)
        {
            result.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }
#endif

        if (_appInfo.m_features.m_debugTags == GraphicsCommon::SoftEnable::ForceEnabled)
        {
            if (!m_debugUtils)
            {
                result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                m_debugUtils = true;
            }
            m_debugMarkers = true;
        }

        return result;
    }

    void VkGraphicsContext::_RetrieveOptionalExtensionNames(
            eastl::vector<const char *> &_currentList,
            const DynamicArray<VkExtensionProperties> &_availableExtensions,
            const GraphicsCommon::ApplicationInfo &_appInfo)
    {
        const auto find = [&_availableExtensions](const char* _extensionName)
        {
            const auto it = eastl::find(
                    _availableExtensions.begin(),
                    _availableExtensions.end(),
                    eastl::string(_extensionName),
                    [](VkExtensionProperties _a, const eastl::string& _b) {
                        return eastl::string(_a.extensionName) == _b;
                    });
            return it != _availableExtensions.end();
        };

        if (_appInfo.m_features.m_debugTags == GraphicsCommon::SoftEnable::TryEnable)
        {
            if (find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            {
                if (!m_debugUtils)
                {
                    _currentList.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    m_debugUtils = true;
                }
                m_debugMarkers = true;
            }
        }
    }

    VkDebugUtilsMessengerCreateInfoEXT VkGraphicsContext::_PopulateDebugCreateInfo(void *_userData)
    {
        const auto severityFlags =
//                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
//                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        const auto messageTypeFlags =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        return {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .flags = 0,
                .messageSeverity = severityFlags,
                .messageType = messageTypeFlags,
                .pfnUserCallback = DebugCallback,
                .pUserData = _userData
        };
    }

    void VkGraphicsContext::_SetupValidationLayersCallback()
    {
        auto createInfo = _PopulateDebugCreateInfo(this);
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");

        if (KE_VERIFY(func != nullptr))
        {
            VkAssert(func(m_instance, &createInfo,
                          nullptr, &m_debugMessenger));
        }
    }

    void VkGraphicsContext::_SelectPhysicalDevice()
    {
        KE_ZoneScopedFunction("VkPhysicalDevice::_SelectPhysicalDevice");

        DynamicArray<VkPhysicalDevice> availablePhysicalDevices;
        VkHelperFunctions::VkArrayFetch(availablePhysicalDevices, vkEnumeratePhysicalDevices, m_instance);

        eastl::vector<VkPhysicalDevice> suitableDevices(m_allocator);
        eastl::copy_if(availablePhysicalDevices.begin(), availablePhysicalDevices.end(),
                       eastl::back_inserter(suitableDevices),
                       [this](const VkPhysicalDevice& _physicalDevice)
        {
            DynamicArray<VkExtensionProperties> extensions;
            VkHelperFunctions::VkArrayFetch(extensions, vkEnumerateDeviceExtensionProperties, _physicalDevice, nullptr);
            auto requiredExtensions = _GetRequiredDeviceExtensions();

            bool suitable = true;

            auto placeholderQueueIndices = QueueIndices();
            suitable &= _SelectQueues(m_appInfo, _physicalDevice, m_surface.GetSurface(), placeholderQueueIndices);

            for (const auto& extension: extensions)
            {
                requiredExtensions.erase(eastl::string{ extension.extensionName });
            }
            suitable &= requiredExtensions.empty();

            return suitable;
        });

        if (KE_VERIFY_MSG(!suitableDevices.empty(), "No suitable device found!"))
        {
            u32 maxScore = 0;
            VkPhysicalDevice selectedDevice;

            for (const auto& suitableDevice: suitableDevices)
            {
                u32 score = 0;
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(suitableDevice, &properties);
                score += properties.limits.maxImageDimension2D;

                if (score >= maxScore)
                {
                    selectedDevice = suitableDevice;
                }
            }

            m_physicalDevice = selectedDevice;
        }
    }

    bool VkGraphicsContext::_SelectQueues(
        const GraphicsCommon::ApplicationInfo &_appInfo,
        const VkPhysicalDevice &_physicalDevice,
        const VkSurfaceKHR &_surface,
        QueueIndices &_indices)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::_SelectQueues");

        DynamicArray<VkQueueFamilyProperties> familyProperties;
        VkHelperFunctions::VkArrayFetch(familyProperties, vkGetPhysicalDeviceQueueFamilyProperties, _physicalDevice);
        eastl::vector_map<u32, u32> indices(m_allocator);

        bool foundAll = true;

        const auto& features = _appInfo.m_features;

        KE_ASSERT_MSG(features.m_transfer && (features.m_graphics || features.m_transferQueue), "Not supported yet");
        KE_ASSERT_MSG(features.m_compute && (features.m_graphics || features.m_asyncCompute), "Not supported yet");

        const auto GetIndexOfFamily = [&indices](u32 _familyIndex) -> u32&
        {
            auto it = indices.find(_familyIndex);
            if (it == indices.end())
            {
                it = indices.emplace(_familyIndex, 0).first;
            }
            return it->second;
        };

        if (features.m_graphics)
        {
            for (s8 i = 0; i < familyProperties.Size(); i++)
            {
                const auto flags = familyProperties[i].queueFlags;

                const bool graphicsOk = bool(flags & VK_QUEUE_GRAPHICS_BIT);
                const bool transferOk = !features.m_transfer || features.m_transferQueue || bool(flags & VK_QUEUE_TRANSFER_BIT);
                const bool computeOk = !features.m_compute || features.m_asyncCompute || bool(flags & VK_QUEUE_COMPUTE_BIT);

                auto& index = GetIndexOfFamily(i);

                if (graphicsOk && transferOk && computeOk && index < familyProperties[i].queueCount)
                {
                    _indices.m_graphicsQueueIndex = { i, static_cast<s32>(index++) };
                    break;
                }
            }
            foundAll &= !_indices.m_graphicsQueueIndex.IsInvalid();
        }

        if (features.m_transferQueue)
        {
            u8 topScore = 0;
            s8 topIndex = QueueIndices::kInvalid;
            for (s8 i = 0; i < familyProperties.Size(); i++)
            {
                const auto flags = familyProperties[i].queueFlags;
                if (flags & VK_QUEUE_TRANSFER_BIT && GetIndexOfFamily(i) < familyProperties[i].queueCount)
                {
                    u8 score = 1;
                    score += flags & VK_QUEUE_GRAPHICS_BIT ? 0 : 4;
                    score += flags & VK_QUEUE_COMPUTE_BIT ? 0 : 3;

                    if (score > topScore)
                    {
                        topScore = score;
                        topIndex = i;
                    }
                }
            }
            _indices.m_transferQueueIndex = { topIndex,
                                              static_cast<s32>(GetIndexOfFamily(topIndex)++) };
            foundAll &= !_indices.m_transferQueueIndex.IsInvalid();
        }

        if (features.m_asyncCompute)
        {
            u8 topScore = 0;
            s8 topIndex = QueueIndices::kInvalid;
            for (s8 i = 0; i < familyProperties.Size(); i++)
            {
                const auto flags = familyProperties[i].queueFlags;
                if (flags & VK_QUEUE_COMPUTE_BIT && GetIndexOfFamily(i) < familyProperties[i].queueCount)
                {
                    u8 score = 0;
                    score += flags & VK_QUEUE_TRANSFER_BIT ? 0 : 1;
                    score += flags & VK_QUEUE_GRAPHICS_BIT ? 0 : 3;

                    if (score > topScore)
                    {
                        topScore = score;
                        topIndex = i;
                    }
                }
            }
            _indices.m_computeQueueIndex = { topIndex,
                                             static_cast<s32>(GetIndexOfFamily(topIndex)++) };
            foundAll &= !_indices.m_computeQueueIndex.IsInvalid();
        }

        if (features.m_present)
        {
            u8 topScore = 0;
            s8 topIndex = QueueIndices::kInvalid;
            for (s8 i = 0; i < familyProperties.Size(); i++)
            {
                const auto flags = familyProperties[i].queueFlags;
                VkBool32 supported;
                vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, _surface, &supported);
                if (supported && GetIndexOfFamily(i) < familyProperties[i].queueCount)
                {
                    u8 score = 0;
                    score += flags & VK_QUEUE_GRAPHICS_BIT ? 1 : 5;
                    score += flags & VK_QUEUE_TRANSFER_BIT ? 1 : 4;
                    score += flags & VK_QUEUE_COMPUTE_BIT ? 1 : 3;

                    if (score > topScore)
                    {
                        topScore = score;
                        topIndex = i;
                    }
                }
            }
            _indices.m_presentQueueIndex = {topIndex,
                                            static_cast<s32>(GetIndexOfFamily(topIndex)++) };
            foundAll &= !_indices.m_presentQueueIndex.IsInvalid();
        }

        return foundAll;
    }

    void VkGraphicsContext::_CreateDevice()
    {
        KE_ZoneScopedFunction("VkGraphicsContext::_CreateDevice");

        eastl::vector<VkDeviceQueueCreateInfo> queueCreateInfo(m_allocator);
        eastl::vector<eastl::vector<float>> queuePriorities(m_allocator);

        KE_ASSERT(_SelectQueues(m_appInfo, m_physicalDevice, m_surface.GetSurface(), m_queueIndices));
        {
            const auto createQueueInfo = [&](QueueIndices::Pair _index, float _priority)
            {
                if (_index.IsInvalid())
                {
                    return;
                }

                auto it = eastl::find(queueCreateInfo.begin(), queueCreateInfo.end(), _index.m_familyIndex,
                                      [](const VkDeviceQueueCreateInfo& _info, u32 _familyIndex)
                                      { return _info.queueFamilyIndex == _familyIndex; });
                bool alreadyInserted = it != queueCreateInfo.end();

                if (!alreadyInserted)
                {
                    it = queueCreateInfo.emplace(queueCreateInfo.end());
                    queuePriorities.push_back().set_allocator(m_allocator);
                    it->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    it->flags = 0;
                    it->queueFamilyIndex = _index.m_familyIndex;
                }
                it->queueCount++;
                auto& prioritiesVector = queuePriorities[eastl::distance(queueCreateInfo.begin(), it)];
                if (_index.m_indexInFamily + 1 >= prioritiesVector.size())
                {
                    prioritiesVector.resize(_index.m_indexInFamily + 1);
                    it->pQueuePriorities = prioritiesVector.data();
                }
                prioritiesVector[_index.m_indexInFamily] = _priority;
            };

            createQueueInfo(m_queueIndices.m_graphicsQueueIndex, 1.0);
            createQueueInfo(m_queueIndices.m_transferQueueIndex, 0.5);
            createQueueInfo(m_queueIndices.m_computeQueueIndex, 0.5);
            createQueueInfo(m_queueIndices.m_presentQueueIndex, 1.0);

            for (u32 i = 0; i < queueCreateInfo.size(); i++)
            {
                KE_ASSERT(queueCreateInfo[i].queueCount == queuePriorities[i].size());
            }
        }

        VkPhysicalDeviceFeatures features;
        // Init struct data;
        memset(&features, VK_FALSE, sizeof(VkPhysicalDeviceFeatures));

        const auto requiredExtensionsStrings = _GetRequiredDeviceExtensions();
        auto requiredExtensions = StringHelpers::RetrieveStringPointerContainer(requiredExtensionsStrings);

        void* next = nullptr;

        VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{};
        VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilitySubsetFeatures{};
        {
            DynamicArray<VkExtensionProperties> availableExtensions;
            VkHelperFunctions::VkArrayFetch(availableExtensions, vkEnumerateDeviceExtensionProperties, m_physicalDevice, nullptr);

            const auto find = [&availableExtensions](const char* _name)
            {
                const auto it = eastl::find(
                    availableExtensions.begin(),
                    availableExtensions.end(),
                    eastl::string(_name),
                    [](const auto& _property, const auto& _name)
                    {
                        return eastl::string(_property.extensionName) == _name;
                    });
                return it != availableExtensions.end();
            };

            if (find(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
            {
                requiredExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
                m_synchronization2 = true;

                synchronization2Features = {
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
                    .pNext = next,
                    .synchronization2 = true,
                };
                next = &synchronization2Features;
            }

            if (m_appInfo.m_features.m_gpuTimestamps != GraphicsCommon::SoftEnable::Disabled)
            {
                if (find(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME))
                {
                    requiredExtensions.push_back(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
                    m_supportsTimestampCalibration = true;
                }
                else
                {
                    KE_ASSERT(m_appInfo.m_features.m_gpuTimestamps != GraphicsCommon::SoftEnable::ForceEnabled);
                }
            }

            if (find("VK_KHR_portability_subset"))
            {
                requiredExtensions.push_back("VK_KHR_portability_subset");

                portabilitySubsetFeatures = {
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
                    .pNext = next,
                    .imageViewFormatSwizzle = true,
                };
                next = &portabilitySubsetFeatures;
            }
        }

        VkDeviceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = next,
            .flags = 0,
            .queueCreateInfoCount = static_cast<u32>(queueCreateInfo.size()),
            .pQueueCreateInfos = queueCreateInfo.data(),
            .enabledLayerCount = static_cast<u32>(kValidationLayerNames.size()),
            .ppEnabledLayerNames = kValidationLayerNames.data(),
            .enabledExtensionCount = static_cast<u32>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
            .pEnabledFeatures = &features
        };

        VkAssert(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

        _RetrieveQueues(m_queueIndices);

        if (m_synchronization2)
        {
            m_vkCmdPipelineBarrier2KHR = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(vkGetDeviceProcAddr(m_device, "vkCmdPipelineBarrier2KHR"));
        }

        if (m_debugMarkers)
        {
            m_vkCmdBeginDebugUtilsLabelExt = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT"));
            m_vkCmdEndDebugUtilsLabelExt = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT"));
            m_vkCmdInsertDebugUtilsLabelExt = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(m_device, "vkCmdInsertDebugUtilsLabelEXT"));
        }

        if (m_supportsTimestampCalibration)
        {
            m_vkGetCalibratedTimestampsKHR = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(
                vkGetDeviceProcAddr(m_device, "vkGetCalibratedTimestampsKHR"));
            m_supportsTimestampCalibration = m_vkGetCalibratedTimestampsKHR != nullptr;

            auto* getFunc = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR>(
                vkGetInstanceProcAddr(m_instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR"));

            u32 size = 0;
            VkTimeDomainKHR domainArray[4];
            getFunc(m_physicalDevice, &size, nullptr);
            KE_ASSERT(size <= eastl::size(domainArray));
            getFunc(m_physicalDevice, &size, domainArray);

            for (int i = 0; i < size; i++)
            {
                if (domainArray[i] != VK_TIME_DOMAIN_DEVICE_EXT)
                {
                    m_cpuTimeDomain = domainArray[i];
                    break;
                }
            }
        }
    }

    void VkGraphicsContext::_RetrieveQueues(const QueueIndices &_queueIndices)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::_RetrieveQueues");

        const auto RetrieveQueue = [this](QueueIndices::Pair _queueIndex, VkQueue& destQueue_)
        {
            if (!_queueIndex.IsInvalid())
            {
                vkGetDeviceQueue(m_device, _queueIndex.m_familyIndex, _queueIndex.m_indexInFamily, &destQueue_);
            }
        };

        RetrieveQueue(_queueIndices.m_graphicsQueueIndex, m_graphicsQueue);
        RetrieveQueue(_queueIndices.m_transferQueueIndex, m_transferQueue);
        RetrieveQueue(_queueIndices.m_computeQueueIndex, m_computeQueue);
        RetrieveQueue(_queueIndices.m_presentQueueIndex, m_presentQueue);
    }

    eastl::vector_set<eastl::string> VkGraphicsContext::_GetRequiredDeviceExtensions() const
    {
        eastl::vector_set<eastl::string> result(m_allocator);

        if (m_appInfo.m_features.m_present)
        {
            result.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        result.insert(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

        return result;
    }

    BufferHandle VkGraphicsContext::CreateBuffer(const BufferCreateDesc& _desc)
    {
        return m_resources.CreateBuffer(_desc, m_device);
    }

    bool VkGraphicsContext::NeedsStagingBuffer(BufferHandle _buffer)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::NeedsStagingBuffer");

        VkResources::BufferColdData* coldData = m_resources.m_buffers.GetCold(_buffer.m_handle);
        VERIFY_OR_RETURN(coldData != nullptr, false);

        VkMemoryPropertyFlagBits memoryPropertyFlags;
        vmaGetAllocationMemoryProperties(
            m_resources.m_allocator,
            coldData->m_allocation,
            reinterpret_cast<VkMemoryPropertyFlags*>(&memoryPropertyFlags));
        return !BitUtils::EnumHasAny(memoryPropertyFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    bool VkGraphicsContext::DestroyBuffer(BufferHandle _bufferHandle)
    {
        return m_resources.DestroyBuffer(_bufferHandle);
    }

    TextureHandle VkGraphicsContext::CreateTexture(const TextureCreateDesc& _createDesc)
    {
        if (GraphicsContext::CreateTexture(_createDesc) == GenPool::kInvalidHandle)
        {
            return  { GenPool::kInvalidHandle };
        }
        return m_resources.CreateTexture(_createDesc, m_device);
    }

    eastl::vector<TextureMemoryFootprint> VkGraphicsContext::FetchTextureSubResourcesMemoryFootprints(
        const TextureDesc& _desc)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::FetchTextureSubResourcesMemoryFootprints");

        eastl::vector<TextureMemoryFootprint> footprints(m_allocator);

        const u32 alignment = m_optimalRowPitchAlignment == 0 ? 1 : m_optimalRowPitchAlignment;

        u64 cumulatedOffset = 0;
        for (u32 sliceIndex = 0; sliceIndex < _desc.m_arraySize; sliceIndex++)
        {
            for (u32 mipIndex = 0; mipIndex < _desc.m_mipCount; mipIndex++)
            {
                TextureMemoryFootprint footprint {
                    .m_offset = cumulatedOffset,
                    .m_width = eastl::max(_desc.m_dimensions.x >> mipIndex, 1u),
                    .m_height = eastl::max(_desc.m_dimensions.y >> mipIndex, 1u),
                    .m_depth = static_cast<u16>(eastl::max(_desc.m_dimensions.z >> mipIndex, 1u)),
                    .m_format = _desc.m_format,
                };
                const u32 sizePerBlock = VkHelperFunctions::GetByteSizePerBlock(VkHelperFunctions::ToVkFormat(_desc.m_format));
                footprint.m_lineByteAlignedSize = Alignment::AlignUp(sizePerBlock * footprint.m_width, alignment);
                footprint.m_rowPitchAlignment = alignment;

                footprints.push_back(footprint);

                const u64 size = footprint.m_lineByteAlignedSize * footprint.m_height * footprint.m_depth;
                cumulatedOffset += size;
            }
        }

        return footprints;
    }

    BufferHandle VkGraphicsContext::CreateStagingBuffer(
        const TextureDesc& _createDesc,
        const eastl::span<const TextureMemoryFootprint>& _footprints)
    {
        return m_resources.CreateStagingBuffer(_createDesc, _footprints, m_device);
    }

    bool VkGraphicsContext::DestroyTexture(TextureHandle _handle)
    {
        return m_resources.ReleaseTexture(_handle, m_device);
    }

    [[nodiscard]] TextureViewHandle VkGraphicsContext::CreateTextureView(const TextureViewDesc& _viewDesc)
    {
        if (GraphicsContext::CreateTextureView(_viewDesc) == GenPool::kInvalidHandle)
        {
            return { GenPool::kInvalidHandle };
        }
        return m_resources.CreateTextureView(_viewDesc, m_device);
    }

    bool VkGraphicsContext::DestroyTextureView(KryneEngine::TextureViewHandle _handle)
    {
        return m_resources.DestroyTextureView(_handle, m_device);
    }

    SamplerHandle VkGraphicsContext::CreateSampler(const SamplerDesc& _samplerDesc)
    {
        return m_resources.CreateSampler(_samplerDesc, m_device);
    }

    bool VkGraphicsContext::DestroySampler(SamplerHandle _sampler)
    {
        return m_resources.DestroySampler(_sampler, m_device);
    }

    BufferViewHandle VkGraphicsContext::CreateBufferView(const BufferViewDesc& _viewDesc)
    {
        return m_resources.CreateBufferView(_viewDesc, m_device);
    }

    bool VkGraphicsContext::DestroyBufferView(BufferViewHandle _handle)
    {
        return m_resources.DestroyBufferView(_handle, m_device);
    }

    RenderTargetViewHandle VkGraphicsContext::CreateRenderTargetView(const RenderTargetViewDesc& _desc)
    {
        return m_resources.CreateRenderTargetView(_desc, m_device);
    }

    bool VkGraphicsContext::DestroyRenderTargetView(RenderTargetViewHandle _handle)
    {
        return m_resources.FreeRenderTargetView(_handle, m_device);
    }

    RenderTargetViewHandle VkGraphicsContext::GetPresentRenderTargetView(u8 _index)
    {
        return (m_appInfo.m_features.m_present)
                ? m_swapChain.m_renderTargetViews[_index]
                : RenderTargetViewHandle { GenPool::kInvalidHandle };
    }

    TextureHandle VkGraphicsContext::GetPresentTexture(u8 _swapChainIndex)
    {
        return (m_appInfo.m_features.m_present)
            ? m_swapChain.m_renderTargetTextures[_swapChainIndex]
            : TextureHandle { GenPool::kInvalidHandle };
    }

    u32 VkGraphicsContext::GetCurrentPresentImageIndex() const
    {
        return (m_appInfo.m_features.m_present)
                ? m_swapChain.m_imageIndex
                : 0;
    }
    RenderPassHandle VkGraphicsContext::CreateRenderPass(const RenderPassDesc& _desc)
    {
        return m_resources.CreateRenderPass(_desc, m_device);
    }

    bool VkGraphicsContext::DestroyRenderPass(RenderPassHandle _handle)
    {
        return m_resources.DestroyRenderPass(_handle, m_device);
    }

    CommandListHandle VkGraphicsContext::BeginGraphicsCommandList()
    {
        return reinterpret_cast<CommandListHandle>(
            m_frameContexts[m_frameId % m_frameContextCount].BeginGraphicsCommandBuffer(m_device));
    }

    void VkGraphicsContext::EndGraphicsCommandList(CommandListHandle _commandList)
    {
        m_frameContexts[m_frameId % m_frameContextCount].EndGraphicsCommandBuffer(
            reinterpret_cast<CommandList>(_commandList));
    }

    void VkGraphicsContext::BeginRenderPass(CommandListHandle _commandList, RenderPassHandle _renderPass)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::BeginRenderPass");

        auto* renderPassData = m_resources.m_renderPasses.Get(_renderPass.m_handle);
        VERIFY_OR_RETURN_VOID(renderPassData != nullptr);

        VkRenderPassBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPassData->m_renderPass,
            .framebuffer = renderPassData->m_framebuffer,
            .renderArea = {
                .offset = { 0, 0 },
                .extent = { renderPassData->m_size.m_width, renderPassData->m_size.m_height }
            },
            .clearValueCount = u32(renderPassData->m_clearValues.size()),
            .pClearValues = renderPassData->m_clearValues.data()
        };

        vkCmdBeginRenderPass(reinterpret_cast<CommandList>(_commandList), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VkGraphicsContext::EndRenderPass(CommandListHandle _commandList)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::EndRenderPass");

        vkCmdEndRenderPass(reinterpret_cast<CommandList>(_commandList));
    }

    void VkGraphicsContext::SetTextureData(
        CommandListHandle _commandList,
        BufferHandle _stagingBuffer,
        TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subResourceIndex,
        const void* _data)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetTextureData");

        VkBuffer* stagingBuffer = m_resources.m_buffers.Get(_stagingBuffer.m_handle);
        VkImage* dstTexture = m_resources.m_textures.Get(_dstTexture.m_handle);

        // Map data
        {
            BufferMapping mapping(
                _stagingBuffer,
                _footprint.m_lineByteAlignedSize * _footprint.m_height * _footprint.m_depth,
                _footprint.m_offset,
                true
            );
            MapBuffer(mapping);
            memcpy(mapping.m_ptr, _data, mapping.m_size);
            UnmapBuffer(mapping);
        }

        VERIFY_OR_RETURN_VOID(stagingBuffer != nullptr);
        VERIFY_OR_RETURN_VOID(dstTexture != nullptr);

        const VkBufferImageCopy region {
            .bufferOffset = _footprint.m_offset,
            // Only mark as tightly packed if the pixel element size is greater or equal to optimal alignment.
            .bufferRowLength = GetByteSizePerBlock(ToVkFormat(_footprint.m_format)) < _footprint.m_rowPitchAlignment ? _footprint.m_lineByteAlignedSize : 0,
            .bufferImageHeight = 0, // Set entry to 0 to mark data as tightly packed.
            .imageSubresource = {
                .aspectMask = VkHelperFunctions::RetrieveAspectMask(_subResourceIndex.m_planeSlice),
                .mipLevel = _subResourceIndex.m_mipIndex,
                .baseArrayLayer = _subResourceIndex.m_arraySlice,
                .layerCount = 1,
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { _footprint.m_width, _footprint.m_height, _footprint.m_depth },
        };

        vkCmdCopyBufferToImage(
            reinterpret_cast<CommandList>(_commandList),
            *stagingBuffer,
            *dstTexture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
    }

    void VkGraphicsContext::SetTextureRegionData(
        CommandListHandle _commandList,
        BufferSpan _srcBuffer,
        TextureHandle _dstTexture,
        const TextureMemoryFootprint& _footprint,
        const SubResourceIndexing& _subresourceIndex,
        const uint3& _regionOffset,
        const uint3& _regionSize)
    {
        VkBuffer* srcBuffer = m_resources.m_buffers.Get(_srcBuffer.m_buffer.m_handle);
        VkImage* dstTexture = m_resources.m_textures.Get(_dstTexture.m_handle);

        KE_ASSERT(srcBuffer != nullptr && dstTexture != nullptr);

        VkBufferImageCopy region {
            .bufferOffset = _srcBuffer.m_offset,
            // Only mark as tightly packed if the pixel element size is greater or equal to optimal alignment.
            .bufferRowLength = GetByteSizePerBlock(ToVkFormat(_footprint.m_format)) < _footprint.m_rowPitchAlignment ? _footprint.m_lineByteAlignedSize : 0,
            .bufferImageHeight = 0, // Set entry to 0 to mark data as tightly packed.
            .imageSubresource = {
                .aspectMask = RetrieveAspectMask(_subresourceIndex.m_planeSlice),
                .mipLevel = _subresourceIndex.m_mipIndex,
                .baseArrayLayer = _subresourceIndex.m_arraySlice,
                .layerCount = 1,
            },
            .imageOffset = { static_cast<s32>(_regionOffset.x), static_cast<s32>(_regionOffset.y), static_cast<s32>(_regionOffset.z) },
            .imageExtent = { _regionSize.x, _regionSize.y, _regionSize.z },
        };
        vkCmdCopyBufferToImage(
            static_cast<CommandList>(_commandList),
            *srcBuffer,
            *dstTexture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
    }

    void VkGraphicsContext::MapBuffer(BufferMapping& _mapping)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::MapBuffer");

        VkResources::BufferColdData* coldData = m_resources.m_buffers.GetCold(_mapping.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(coldData != nullptr);
        KE_ASSERT_MSG(_mapping.m_ptr == nullptr, "Structure still holds a mapping");

        KE_ASSERT(coldData->m_info.size >= _mapping.m_offset);
        KE_ASSERT(_mapping.m_size == ~0 || coldData->m_info.size >= _mapping.m_offset + _mapping.m_size);
        _mapping.m_size = eastl::min(_mapping.m_size, coldData->m_info.size - _mapping.m_offset);

        if (coldData->m_info.pMappedData != nullptr)
        {
            _mapping.m_ptr = static_cast<std::byte*>(coldData->m_info.pMappedData) + _mapping.m_offset;
        }
        else
        {
            void* ptr;
            vmaMapMemory(m_resources.m_allocator, coldData->m_allocation, &ptr);
            _mapping.m_ptr = static_cast<std::byte*>(ptr) + _mapping.m_offset;
        }
    }

    void VkGraphicsContext::UnmapBuffer(BufferMapping& _mapping)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::UnmapBuffer");

        VkResources::BufferColdData* coldData = m_resources.m_buffers.GetCold(_mapping.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(coldData != nullptr);
        KE_ASSERT_MSG(_mapping.m_ptr != nullptr, "Structure holds no mapping");

        if (coldData->m_info.pMappedData)
        {
            vmaFlushAllocation(
                m_resources.m_allocator,
                coldData->m_allocation,
                _mapping.m_offset,
                _mapping.m_size);
        }
        else
        {
            vmaUnmapMemory(m_resources.m_allocator, coldData->m_allocation);
        }
        _mapping.m_ptr = nullptr;
    }

    void VkGraphicsContext::CopyBuffer(CommandListHandle _commandList, const BufferCopyParameters& _params)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::CopyBuffer");

        VkBuffer* bufferSrc = m_resources.m_buffers.Get(_params.m_bufferSrc.m_handle);
        VkBuffer* bufferDst = m_resources.m_buffers.Get(_params.m_bufferDst.m_handle);
        VERIFY_OR_RETURN_VOID(bufferSrc != nullptr && bufferDst != nullptr);

        const VkBufferCopy region {
            .srcOffset = _params.m_offsetSrc,
            .dstOffset = _params.m_offsetDst,
            .size = _params.m_copySize,
        };

        vkCmdCopyBuffer(reinterpret_cast<CommandList>(_commandList), *bufferSrc, *bufferDst, 1, &region);
    }

    void VkGraphicsContext::PlaceMemoryBarriers(
        CommandListHandle _commandList,
        const eastl::span<const GlobalMemoryBarrier>& _globalMemoryBarriers,
        const eastl::span<const BufferMemoryBarrier>& _bufferMemoryBarriers,
        const eastl::span<const TextureMemoryBarrier>& _textureMemoryBarriers)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::PlaceMemoryBarriers");

        using namespace VkHelperFunctions;

        if (m_vkCmdPipelineBarrier2KHR != nullptr)
        {
            DynamicArray<VkMemoryBarrier2> globalMemoryBarriers(_globalMemoryBarriers.size());
            DynamicArray<VkBufferMemoryBarrier2> bufferMemoryBarriers(_bufferMemoryBarriers.size());
            DynamicArray<VkImageMemoryBarrier2> imageMemoryBarriers(_textureMemoryBarriers.size());

            for (auto i = 0u; i < globalMemoryBarriers.Size(); i++)
            {
                const GlobalMemoryBarrier& barrier = _globalMemoryBarriers[i];
                globalMemoryBarriers[i] = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesSrc, true),
                    .srcAccessMask = ToVkAccessFlags2(barrier.m_accessSrc),
                    .dstStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesDst, false),
                    .dstAccessMask = ToVkAccessFlags2(barrier.m_accessDst),
                };
            }

            for (auto i = 0u; i < bufferMemoryBarriers.Size(); i++)
            {
                const BufferMemoryBarrier& barrier = _bufferMemoryBarriers[i];
                VkBuffer* buffer = m_resources.m_buffers.Get(barrier.m_buffer.m_handle);

                bufferMemoryBarriers[i] = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesSrc, true),
                    .srcAccessMask = ToVkAccessFlags2(barrier.m_accessSrc),
                    .dstStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesDst, false),
                    .dstAccessMask = ToVkAccessFlags2(barrier.m_accessDst),
                    .srcQueueFamilyIndex = 0,
                    .dstQueueFamilyIndex = 0,
                    .buffer = buffer != nullptr ? *buffer : VK_NULL_HANDLE,
                    .offset = barrier.m_offset,
                    .size = barrier.m_size,
                };
            }

            for (auto i = 0u; i < imageMemoryBarriers.Size(); i++)
            {
                const TextureMemoryBarrier& barrier = _textureMemoryBarriers[i];
                VkImage* image = m_resources.m_textures.Get(barrier.m_texture.m_handle);

                imageMemoryBarriers[i] = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesSrc, true),
                    .srcAccessMask = ToVkAccessFlags2(barrier.m_accessSrc),
                    .dstStageMask = ToVkPipelineStageFlagBits2(barrier.m_stagesDst, false),
                    .dstAccessMask = ToVkAccessFlags2(barrier.m_accessDst),
                    .oldLayout = ToVkLayout(barrier.m_layoutSrc),
                    .newLayout = ToVkLayout(barrier.m_layoutDst),
                    .srcQueueFamilyIndex = 0,
                    .dstQueueFamilyIndex = 0,
                    .image = image != nullptr ? *image : VK_NULL_HANDLE,
                    .subresourceRange = {
                        .aspectMask = RetrieveAspectMask(barrier.m_planes),
                        .baseMipLevel = barrier.m_mipStart,
                        .levelCount = barrier.m_mipCount == 0XFF ? VK_REMAINING_MIP_LEVELS : barrier.m_mipCount,
                        .baseArrayLayer = barrier.m_arrayStart,
                        .layerCount = barrier.m_arrayCount == 0xFFFF ? VK_REMAINING_ARRAY_LAYERS : barrier.m_arrayCount,
                    }};
            }

            const VkDependencyInfo dependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = (u32)globalMemoryBarriers.Size(),
                .pMemoryBarriers = globalMemoryBarriers.Data(),
                .bufferMemoryBarrierCount = (u32)bufferMemoryBarriers.Size(),
                .pBufferMemoryBarriers = bufferMemoryBarriers.Data(),
                .imageMemoryBarrierCount = (u32)imageMemoryBarriers.Size(),
                .pImageMemoryBarriers = imageMemoryBarriers.Data(),
            };

            m_vkCmdPipelineBarrier2KHR(reinterpret_cast<CommandList>(_commandList), &dependencyInfo);
        }
        else
        {
            eastl::vector<VkMemoryBarrier> globalMemoryBarriers(m_allocator);
            eastl::vector<VkBufferMemoryBarrier> bufferMemoryBarriers(m_allocator);
            eastl::vector<VkImageMemoryBarrier> imageMemoryBarriers(m_allocator);

            globalMemoryBarriers.reserve(_globalMemoryBarriers.size());
            bufferMemoryBarriers.reserve(_bufferMemoryBarriers.size());
            imageMemoryBarriers.reserve(_textureMemoryBarriers.size());

            u32 gIndex = 0;
            u32 bIndex = 0;
            u32 iIndex = 0;

            do
            {
                globalMemoryBarriers.clear();
                bufferMemoryBarriers.clear();
                imageMemoryBarriers.clear();

                bool found = false;
                BarrierSyncStageFlags src;
                BarrierSyncStageFlags dst;

                const auto shouldRegister = [&found, &src, &dst](auto _src, auto _dst)
                {
                    if (found)
                    {
                        return _src == src && _dst == dst;
                    }
                    else
                    {
                        found = true;
                        src = _src;
                        dst = _dst;
                        return true;
                    }
                };

                for (; gIndex < _globalMemoryBarriers.size(); gIndex++)
                {
                    const GlobalMemoryBarrier& barrier = _globalMemoryBarriers[gIndex];
                    if (shouldRegister(barrier.m_stagesSrc, barrier.m_stagesDst))
                    {
                        globalMemoryBarriers.push_back({
                            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                            .srcAccessMask = ToVkAccessFlags(barrier.m_accessSrc),
                            .dstAccessMask = ToVkAccessFlags(barrier.m_accessDst),
                        });
                    }
                    else
                    {
                        break;
                    }
                }

                for (; bIndex < _bufferMemoryBarriers.size(); bIndex++)
                {
                    const BufferMemoryBarrier& barrier = _bufferMemoryBarriers[bIndex];
                    VkBuffer* buffer = m_resources.m_buffers.Get(barrier.m_buffer.m_handle);

                    if (shouldRegister(barrier.m_stagesSrc, barrier.m_stagesDst))
                    {
                        bufferMemoryBarriers.push_back({
                            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                            .srcAccessMask = ToVkAccessFlags(barrier.m_accessSrc),
                            .dstAccessMask = ToVkAccessFlags(barrier.m_accessDst),
                            .srcQueueFamilyIndex = 0,
                            .dstQueueFamilyIndex = 0,
                            .buffer = buffer != nullptr ? *buffer : VK_NULL_HANDLE,
                            .offset = barrier.m_offset,
                            .size = barrier.m_size,
                        });
                    }
                    else
                    {
                        break;
                    }
                }

                for (; iIndex < _textureMemoryBarriers.size(); iIndex++)
                {
                    const TextureMemoryBarrier& barrier = _textureMemoryBarriers[iIndex];
                    VkImage* image = m_resources.m_textures.Get(barrier.m_texture.m_handle);

                    if (shouldRegister(barrier.m_stagesSrc, barrier.m_stagesDst))
                    {
                        imageMemoryBarriers.push_back({
                            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                            .srcAccessMask = ToVkAccessFlags(barrier.m_accessSrc),
                            .dstAccessMask = ToVkAccessFlags(barrier.m_accessDst),
                            .oldLayout = ToVkLayout(barrier.m_layoutSrc),
                            .newLayout = ToVkLayout(barrier.m_layoutDst),
                            .srcQueueFamilyIndex = 0,
                            .dstQueueFamilyIndex = 0,
                            .image = image != nullptr ? *image : VK_NULL_HANDLE,
                            .subresourceRange = {
                                .aspectMask = RetrieveAspectMask(barrier.m_planes),
                                .baseMipLevel = barrier.m_mipStart,
                                .levelCount = barrier.m_mipCount == 0XFF ? VK_REMAINING_MIP_LEVELS : barrier.m_mipCount,
                                .baseArrayLayer = barrier.m_arrayStart,
                                .layerCount = barrier.m_arrayCount == 0xFFFF ? VK_REMAINING_ARRAY_LAYERS : barrier.m_arrayCount,
                            }
                        });
                    }
                    else
                    {
                        break;
                    }
                }

                vkCmdPipelineBarrier(
                    reinterpret_cast<CommandList>(_commandList),
                    ToVkPipelineStageFlagBits(src, true),
                    ToVkPipelineStageFlagBits(dst, false),
                    0,
                    globalMemoryBarriers.size(),
                    globalMemoryBarriers.data(),
                    bufferMemoryBarriers.size(),
                    bufferMemoryBarriers.data(),
                    imageMemoryBarriers.size(),
                    imageMemoryBarriers.data());
            }
            while (gIndex < _globalMemoryBarriers.size() && bIndex < _bufferMemoryBarriers.size() && iIndex < _textureMemoryBarriers.size());
        }
    }

    ShaderModuleHandle VkGraphicsContext::RegisterShaderModule(void* _bytecodeData, u64 _bytecodeSize)
    {
        return m_resources.CreateShaderModule(_bytecodeData, _bytecodeSize, m_device);
    }

    DescriptorSetLayoutHandle VkGraphicsContext::CreateDescriptorSetLayout(
        const DescriptorSetDesc& _desc,
        u32* _bindingIndices)
    {
        return m_descriptorSetManager.CreateDescriptorSetLayout(
            _desc,
            _bindingIndices, m_device);
    }

    DescriptorSetHandle VkGraphicsContext::CreateDescriptorSet(DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetManager.CreateDescriptorSet(_layout, m_device);
    }

    PipelineLayoutHandle VkGraphicsContext::CreatePipelineLayout(const PipelineLayoutDesc& _desc)
    {
        return m_resources.CreatePipelineLayout(_desc, m_device, &m_descriptorSetManager);
    }

    GraphicsPipelineHandle VkGraphicsContext::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc)
    {
        return m_resources.CreateGraphicsPipeline(_desc, m_device);
    }

    bool VkGraphicsContext::DestroyGraphicsPipeline(GraphicsPipelineHandle _pipeline)
    {
        return m_resources.DestroyGraphicsPipeline(_pipeline, m_device);
    }

    bool VkGraphicsContext::DestroyPipelineLayout(PipelineLayoutHandle _layout)
    {
        return m_resources.DestroyPipelineLayout(_layout, m_device);
    }

    bool VkGraphicsContext::DestroyDescriptorSet(DescriptorSetHandle _set)
    {
        return m_descriptorSetManager.DestroyDescriptorSet(_set, m_device);
    }

    bool VkGraphicsContext::DestroyDescriptorSetLayout(DescriptorSetLayoutHandle _layout)
    {
        return m_descriptorSetManager.DestroyDescriptorSetLayout(_layout, m_device);
    }

    bool VkGraphicsContext::FreeShaderModule(ShaderModuleHandle _module)
    {
        return m_resources.DestroyShaderModule(_module, m_device);
    }

    ComputePipelineHandle VkGraphicsContext::CreateComputePipeline(const ComputePipelineDesc& _desc)
    {
        return m_resources.CreateComputePipeline(_desc, m_device);
    }

    bool VkGraphicsContext::DestroyComputePipeline(ComputePipelineHandle _pipeline)
    {
        return m_resources.DestroyComputePipeline(_pipeline, m_device);
    }

    void VkGraphicsContext::UpdateDescriptorSet(
        DescriptorSetHandle _descriptorSet, const eastl::span<const DescriptorSetWriteInfo>& _writes, bool _singleFrame)
    {
        return m_descriptorSetManager.UpdateDescriptorSet(
            _descriptorSet,
            _writes,
            _singleFrame,
            m_device,
            m_resources,
            m_frameId % m_frameContextCount);
    }

    void VkGraphicsContext::SetViewport(CommandListHandle _commandList, const Viewport& _viewport)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetViewport");

        const VkViewport viewport {
            .x = static_cast<float>(_viewport.m_topLeftX),
            .y = static_cast<float>(_viewport.m_height - _viewport.m_topLeftY),
            .width = static_cast<float>(_viewport.m_width),
            .height = -static_cast<float>(_viewport.m_height),
            .minDepth = _viewport.m_minDepth,
            .maxDepth = _viewport.m_maxDepth,
        };
        vkCmdSetViewport(reinterpret_cast<CommandList>(_commandList), 0, 1, &viewport);
    }

    void VkGraphicsContext::SetScissorsRect(CommandListHandle _commandList, const Rect& _rect)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetScissorsRect");

        const VkRect2D scissorRect {
            .offset = {
                static_cast<s32>(_rect.m_left),
                static_cast<s32>(_rect.m_top),
            },
            .extent = {
                _rect.m_right - _rect.m_left,
                _rect.m_bottom - _rect.m_top,
            }
        };
        vkCmdSetScissor(reinterpret_cast<CommandList>(_commandList), 0, 1, &scissorRect);
    }

    void VkGraphicsContext::SetIndexBuffer(CommandListHandle _commandList, const BufferSpan& _indexBufferView, bool _isU16)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetIndexBuffer");

        VkBuffer* pBuffer = m_resources.m_buffers.Get(_indexBufferView.m_buffer.m_handle);
        VERIFY_OR_RETURN_VOID(pBuffer != nullptr);
        vkCmdBindIndexBuffer(
            reinterpret_cast<CommandList>(_commandList),
            *pBuffer,
            _indexBufferView.m_offset,
            _isU16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    void VkGraphicsContext::SetVertexBuffers(CommandListHandle _commandList, const eastl::span<const BufferSpan>& _bufferViews)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetVertexBuffers");

        eastl::fixed_vector<VkBuffer, 4> buffers;
        eastl::fixed_vector<u64, 4> offsets;
        buffers.reserve(_bufferViews.size());
        offsets.reserve(_bufferViews.size());

        for (const auto& view: _bufferViews)
        {
            VERIFY_OR_RETURN_VOID(view.m_buffer != GenPool::kInvalidHandle);
            VkBuffer* pBuffer = m_resources.m_buffers.Get(view.m_buffer.m_handle);
            VERIFY_OR_RETURN_VOID(pBuffer != nullptr);

            buffers.push_back(*pBuffer);
            offsets.push_back(view.m_offset);
        }
        vkCmdBindVertexBuffers(
            reinterpret_cast<CommandList>(_commandList),
            0,
            _bufferViews.size(),
            buffers.data(),
            offsets.data());
    }

    void VkGraphicsContext::SetGraphicsPipeline(CommandListHandle _commandList, GraphicsPipelineHandle _graphicsPipeline)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetGraphicsPipeline");

        VkPipeline* pPipeline = m_resources.m_pipelines.Get(_graphicsPipeline.m_handle);
        VERIFY_OR_RETURN_VOID(pPipeline != nullptr);

        vkCmdBindPipeline(reinterpret_cast<CommandList>(_commandList), VK_PIPELINE_BIND_POINT_GRAPHICS, *pPipeline);
    }

    void VkGraphicsContext::SetGraphicsPushConstant(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        const eastl::span<const u32>& _data,
        u32 _index,
        u32 _offset)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetGraphicsPushConstant");

        auto [pLayout, pColdData] = m_resources.m_pipelineLayouts.GetAll(_layout.m_handle);
        VERIFY_OR_RETURN_VOID(pLayout != nullptr);

        vkCmdPushConstants(
            reinterpret_cast<CommandList>(_commandList),
            *pLayout,
            VkHelperFunctions::ToVkShaderStageFlags(pColdData->m_pushConstants[_index].m_visibility),
            (_offset + pColdData->m_pushConstants[_index].m_offset) * sizeof(u32),
            _data.size() * sizeof(u32),
            _data.data());
    }

    void VkGraphicsContext::SetGraphicsDescriptorSetsWithOffset(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        const eastl::span<const DescriptorSetHandle>& _sets,
        u32 _offset)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetGraphicsDescriptorSets");

        const u8 frameIndex = m_frameId % m_frameContextCount;

        VkPipelineLayout* pLayout = m_resources.m_pipelineLayouts.Get(_layout.m_handle);
        VERIFY_OR_RETURN_VOID(pLayout != nullptr);

        for (auto i = 0; i < _sets.size(); i++)
        {
            VERIFY_OR_RETURN_VOID(m_descriptorSetManager.m_descriptorSetPools.Get(_sets[i].m_handle) != nullptr);
            const u64 offset = m_frameContextCount * _sets[i].m_handle.m_index + frameIndex;

            vkCmdBindDescriptorSets(
                reinterpret_cast<CommandList>(_commandList),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                *pLayout,
                i + _offset,
                1,
                m_descriptorSetManager.m_descriptorSets.begin() + offset,
                0,
                nullptr);
        }
    }

    void VkGraphicsContext::DrawInstanced(CommandListHandle _commandList, const DrawInstancedDesc& _desc)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::DrawInstanced");

        vkCmdDraw(
            reinterpret_cast<CommandList>(_commandList),
            _desc.m_vertexCount,
            _desc.m_instanceCount,
            _desc.m_vertexOffset,
            _desc.m_instanceOffset);
    }

    void VkGraphicsContext::DrawIndexedInstanced(CommandListHandle _commandList, const DrawIndexedInstancedDesc& _desc)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::DrawIndexedInstanced");

        vkCmdDrawIndexed(
            reinterpret_cast<CommandList>(_commandList),
            _desc.m_elementCount,
            _desc.m_instanceCount,
            _desc.m_indexOffset,
            _desc.m_vertexOffset,
            _desc.m_instanceOffset);
    }

    void VkGraphicsContext::SetComputePipeline(CommandListHandle _commandList, ComputePipelineHandle _pipeline)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetComputePipeline");

        VkPipeline* pPipeline = m_resources.m_pipelines.Get(_pipeline.m_handle);

        VERIFY_OR_RETURN_VOID(pPipeline != nullptr);

        vkCmdBindPipeline(
            reinterpret_cast<CommandList>(_commandList),
            VK_PIPELINE_BIND_POINT_COMPUTE,
            *pPipeline);
    }

    void VkGraphicsContext::SetComputeDescriptorSetsWithOffset(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const DescriptorSetHandle> _sets,
        u32 _offset)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetComputeDescriptorSetsWithOffset");

        const u8 frameIndex = m_frameId % m_frameContextCount;

        VkPipelineLayout* pLayout = m_resources.m_pipelineLayouts.Get(_layout.m_handle);
        VERIFY_OR_RETURN_VOID(pLayout != nullptr);

        for (auto i = 0; i < _sets.size(); i++)
        {
            VERIFY_OR_RETURN_VOID(m_descriptorSetManager.m_descriptorSetPools.Get(_sets[i].m_handle) != nullptr);
            const u64 offset = m_frameContextCount * _sets[i].m_handle.m_index + frameIndex;

            vkCmdBindDescriptorSets(
                reinterpret_cast<CommandList>(_commandList),
                VK_PIPELINE_BIND_POINT_COMPUTE,
                *pLayout,
                i + _offset,
                1,
                m_descriptorSetManager.m_descriptorSets.begin() + offset,
                0,
                nullptr);
        }
    }

    void VkGraphicsContext::SetComputePushConstant(
        CommandListHandle _commandList,
        PipelineLayoutHandle _layout,
        eastl::span<const u32> _data)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::SetComputePushConstant");

        auto [pLayout, pColdData] = m_resources.m_pipelineLayouts.GetAll(_layout.m_handle);
        VERIFY_OR_RETURN_VOID(pLayout != nullptr);
        VERIFY_OR_RETURN_VOID(!pColdData->m_pushConstants.empty());

        vkCmdPushConstants(
            reinterpret_cast<CommandList>(_commandList),
            *pLayout,
            VkHelperFunctions::ToVkShaderStageFlags(pColdData->m_pushConstants[0].m_visibility),
            (pColdData->m_pushConstants[0].m_offset) * sizeof(u32),
            _data.size_bytes(),
            _data.data());
    }

    void VkGraphicsContext::Dispatch(CommandListHandle _commandList, uint3 _threadGroupCount, uint3)
    {
        KE_ZoneScopedFunction("VkGraphicsContext::Dispatch");

        vkCmdDispatch(
            reinterpret_cast<CommandList>(_commandList),
            _threadGroupCount.x,
            _threadGroupCount.y,
            _threadGroupCount.z);
    }

    void VkGraphicsContext::PushDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color& _color)
    {
        if (m_vkCmdBeginDebugUtilsLabelExt == nullptr)
            return;

        const VkDebugUtilsLabelEXT label {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = _markerName.data(),
            .color = {
                _color.m_value.a,
                _color.m_value.r,
                _color.m_value.g,
                _color.m_value.b,
            }};
        m_vkCmdBeginDebugUtilsLabelExt(
            reinterpret_cast<CommandList>(_commandList),
            &label);
    }

    void VkGraphicsContext::PopDebugMarker(CommandListHandle _commandList)
    {
        if (m_vkCmdEndDebugUtilsLabelExt == nullptr)
            return;

        m_vkCmdEndDebugUtilsLabelExt(reinterpret_cast<CommandList>(_commandList));
    }

    void VkGraphicsContext::InsertDebugMarker(
        CommandListHandle _commandList,
        const eastl::string_view& _markerName,
        const Color& _color)
    {
        if (m_vkCmdInsertDebugUtilsLabelExt == nullptr)
            return;

        const VkDebugUtilsLabelEXT label {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = _markerName.data(),
            .color = {
                _color.m_value.a,
                _color.m_value.r,
                _color.m_value.g,
                _color.m_value.b,
            },
        };
        m_vkCmdInsertDebugUtilsLabelExt(
            reinterpret_cast<CommandList>(_commandList),
            &label);
    }

    void VkGraphicsContext::CalibrateCpuGpuClocks()
    {
        if (!m_supportsTimestampCalibration)
            return;

        const eastl::array<VkCalibratedTimestampInfoKHR, 2> domains {
            VkCalibratedTimestampInfoKHR {
                .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR,
                .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT,
            },
            VkCalibratedTimestampInfoKHR {
                .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR,
                .timeDomain = m_cpuTimeDomain,
            },
        };

        eastl::array<u64, 2> rawTimestamps {};
        u64 maxDeviation;

        VkAssert(m_vkGetCalibratedTimestampsKHR(
            m_device,
            domains.size(),
            domains.data(),
            rawTimestamps.data(),
            &maxDeviation));

        u64 cpuTimestampNs;
        switch (m_cpuTimeDomain)
        {
            case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR:
            {
#ifdef _WIN32
                LARGE_INTEGER freq{};
                QueryPerformanceFrequency(&freq);
                const double qpcToNs = 1e9 / double(freq.QuadPart);
                cpuTimestampNs = uint64_t(double(rawTimestamps[1]) * qpcToNs);
#endif
                break;
            }
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
                cpuTimestampNs = rawTimestamps[1]; // CLOCK_MONOTONIC is already in ns
                break;
            default:
                KE_FATAL("Unsupported CPU time domain");
        }

        const u64 gpuTimestampNs = static_cast<u64>(static_cast<double>(rawTimestamps[0]) * m_gpuTimestampPeriod);
        m_cpuTimestampOffset = cpuTimestampNs - gpuTimestampNs;
    }

    TimestampHandle VkGraphicsContext::PutTimestamp(CommandListHandle _commandList)
    {
        return {

            .m_index = m_frameContexts[m_frameId % m_frameContextCount].PutTimestamp(
                reinterpret_cast<VkCommandBuffer>(_commandList)),
            .m_frameId = static_cast<u32>(m_frameId),
        };
    }

    u64 VkGraphicsContext::GetResolvedTimestamp(TimestampHandle _timestamp) const
    {
        if (!m_supportsTimestampQueries || !m_supportsTimestampCalibration || m_lastResolvedFrame == ~0ull)
            return 0;

        if (_timestamp.m_frameId + m_frameContextCount <= u32(m_lastResolvedFrame) || _timestamp.m_frameId > u32(m_lastResolvedFrame))
            return 0;

        return m_frameContexts[_timestamp.m_frameId % m_frameContextCount].m_resolvedTimestamps[_timestamp.m_index];
    }

    eastl::span<const u64> VkGraphicsContext::GetResolvedTimestamps(u64 _frameId) const
    {
        if (!m_supportsTimestampQueries || !m_supportsTimestampCalibration || m_lastResolvedFrame == ~0ull)
            return {};

        if (_frameId + m_frameContextCount <= m_lastResolvedFrame || _frameId > m_lastResolvedFrame)
            return {};

        return { m_frameContexts[_frameId % m_frameContextCount].m_resolvedTimestamps, m_appInfo.m_features.m_gpuTimestampBufferCapacity };
    }
}
