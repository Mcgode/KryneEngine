/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2022.
 */

#include "Graphics/Vulkan/VkSwapChain.hpp"

#include <GLFW/glfw3.h>

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkDebugHandler.hpp"
#include "Graphics/Vulkan/VkResources.hpp"
#include "Graphics/Vulkan/VkSurface.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/EastlHelpers.hpp"
#include "KryneEngine/Core/Graphics/ResourceViews/RenderTargetView.hpp"

namespace KryneEngine
{
    VkSwapChain::VkSwapChain(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    void VkSwapChain::Init(
            const GraphicsCommon::ApplicationInfo &_appInfo,
            VkDevice _device, const VkSurface &_surface,
            VkResources &_resources, GLFWwindow *_window,
            const VkCommonStructures::QueueIndices &_queueIndices,
            u64 _currentFrameIndex)
    {
        KE_ZoneScopedFunction("VkSwapChain::VkSwapChain");

        const auto& capabilities = _surface.GetCapabilities();
        KE_ASSERT(!capabilities.m_formats.Empty() && !capabilities.m_presentModes.Empty());

        const auto displayOptions = _appInfo.m_displayOptions;

        // Select appropriate format
        VkSurfaceFormatKHR selectedSurfaceFormat;
        if (displayOptions.m_sRgbPresent != GraphicsCommon::SoftEnable::Disabled)
        {
            for (const auto& format: capabilities.m_formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    selectedSurfaceFormat = format;
                    break;
                }
            }

            KE_ASSERT(displayOptions.m_sRgbPresent == GraphicsCommon::SoftEnable::TryEnable
                      || selectedSurfaceFormat.format != VK_FORMAT_UNDEFINED);
        }
        if (selectedSurfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            selectedSurfaceFormat = capabilities.m_formats[0];
        }

        // Select appropriate present mode
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (displayOptions.m_tripleBuffering != GraphicsCommon::SoftEnable::Disabled)
        {
            for (const auto& presentMode: capabilities.m_presentModes)
            {
                if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    selectedPresentMode = presentMode;
                    break;
                }
            }

            KE_ASSERT(displayOptions.m_tripleBuffering == GraphicsCommon::SoftEnable::TryEnable
                   || selectedPresentMode != VK_PRESENT_MODE_FIFO_KHR);
        }

        // Retrieve extent
        VkExtent2D extent;
        if (capabilities.m_surfaceCapabilities.currentExtent.width != std::numeric_limits<u32>::max()
            && capabilities.m_surfaceCapabilities.currentExtent.height != std::numeric_limits<u32>::max())
        {
            extent = capabilities.m_surfaceCapabilities.currentExtent;
        }
        else
        {
            s32 width, height;
            glfwGetFramebufferSize(_window, &width, &height);

            extent = VkExtent2D {
                static_cast<u32>(width),
                static_cast<u32>(height)
            };

            extent.width = eastl::clamp(extent.width,
                                        capabilities.m_surfaceCapabilities.minImageExtent.width,
                                        capabilities.m_surfaceCapabilities.maxImageExtent.width);
            extent.height = eastl::clamp(extent.height,
                                         capabilities.m_surfaceCapabilities.minImageExtent.height,
                                         capabilities.m_surfaceCapabilities.maxImageExtent.height);
        }

        u32 desiredImageCount = 2;
        if (displayOptions.m_tripleBuffering != GraphicsCommon::SoftEnable::Disabled)
        {
            desiredImageCount++;
        }
        desiredImageCount = eastl::max(desiredImageCount, capabilities.m_surfaceCapabilities.minImageCount);
        if (capabilities.m_surfaceCapabilities.minImageCount != 0)
        {
            desiredImageCount = eastl::min(desiredImageCount, capabilities.m_surfaceCapabilities.maxImageCount);
        }
        KE_ASSERT(desiredImageCount >= 3 || displayOptions.m_tripleBuffering != GraphicsCommon::SoftEnable::ForceEnabled);

        eastl::vector<u32> queueFamilyIndices{};
        m_sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (_appInfo.m_features.m_concurrentQueues)
        {
            queueFamilyIndices = _queueIndices.RetrieveDifferentFamilies();
            if (queueFamilyIndices.size() <= 1)
            {
                queueFamilyIndices.clear();
            }
            else
            {
                m_sharingMode = VK_SHARING_MODE_CONCURRENT;
            }
        }

        m_currentSwapChain = m_allocator.New<SwapChainData>(m_allocator);
        m_currentSwapChain->m_framebufferSize = { extent.width, extent.height };

        {
            m_reCreateInfo = VkSwapchainCreateInfoKHR{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .flags = 0,
                .surface = _surface.GetSurface(),
                .minImageCount = desiredImageCount,
                .imageFormat = selectedSurfaceFormat.format,
                .imageColorSpace = selectedSurfaceFormat.colorSpace,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = m_sharingMode,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = selectedPresentMode,
                .oldSwapchain = VK_NULL_HANDLE,
            };
            VkSwapchainCreateInfoKHR createInfo = m_reCreateInfo;
            createInfo.imageExtent = extent;
            createInfo.preTransform = capabilities.m_surfaceCapabilities.currentTransform;
            createInfo.queueFamilyIndexCount = static_cast<u32>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();

            VkAssert(vkCreateSwapchainKHR(_device, &createInfo, nullptr, &m_currentSwapChain->m_swapChain));
        }

        {
            u32 imageCount;
            VkAssert(vkGetSwapchainImagesKHR(_device, m_currentSwapChain->m_swapChain,  &imageCount, nullptr));
            DynamicArray<VkImage> images;
            images.Resize(imageCount);
            VkAssert(vkGetSwapchainImagesKHR(_device, m_currentSwapChain->m_swapChain, &imageCount, images.Data()));
            KE_ASSERT_MSG(imageCount > 0, "Unable to retrieve swapchain images");

            m_currentSwapChain->m_renderTargetTextures.Resize(imageCount);
            m_currentSwapChain->m_renderTargetViews.Resize(imageCount);
            m_currentSwapChain->m_imageAvailableSemaphores.Resize(imageCount);
            for (auto i = 0u; i < imageCount; i++)
            {
                const auto textureHandle = _resources.RegisterTexture(images[i], {extent.width, extent.height, 1});
#if !defined(KE_FINAL)
                const eastl::string rtvDebugName = _appInfo.m_applicationName + "/Swapchain/RTV[" + eastl::to_string(i) + "]";
#endif
                const RenderTargetViewDesc rtvDesc {
                    .m_texture = textureHandle,
                    .m_format = VkHelperFunctions::FromVkFormat(selectedSurfaceFormat.format),
#if !defined(KE_FINAL)
                    .m_debugName = rtvDebugName,
#endif
                };

                m_currentSwapChain->m_renderTargetTextures.Init(i, textureHandle);
                m_currentSwapChain->m_renderTargetViews.Init(i, _resources.CreateRenderTargetView(rtvDesc, _device));
                {
                    VkSemaphoreCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                    VkAssert(vkCreateSemaphore(_device, &createInfo, nullptr, &m_currentSwapChain->m_imageAvailableSemaphores[i]));
                }
            }
        }

        AcquireNextImage(_device, _currentFrameIndex % m_currentSwapChain->m_imageAvailableSemaphores.Size());
    }

    VkSwapChain::~VkSwapChain()
    {
        KE_ASSERT(m_currentSwapChain == nullptr);
        KE_ASSERT(m_nextSwapChain == nullptr);
    }

    bool VkSwapChain::RecreateSwapChain(
        const VkDevice _device,
        const VkSurface& _surface,
        VkResources& _resources,
        GLFWwindow* _window,
        const VkCommonStructures::QueueIndices& _queueIndices,
        const u64 _frameId)
    {
        KE_ZoneScopedFunction("VkSwapChain::RecreateSwapChain");

        if (m_nextSwapChain != nullptr)
            return false;

        m_nextSwapChain = m_allocator.New<SwapChainData>(m_allocator);

        // Delay transition to the next frame, as the acquired image this frame belongs to the previous swap chain.
        m_nextSwapChainTransitionFrame = _frameId + 1;

        const VkSurface::Capabilities capabilities = _surface.GetCapabilities();

        VkExtent2D extent;
        if (capabilities.m_surfaceCapabilities.currentExtent.width != std::numeric_limits<u32>::max()
            && capabilities.m_surfaceCapabilities.currentExtent.height != std::numeric_limits<u32>::max())
        {
            extent = capabilities.m_surfaceCapabilities.currentExtent;
        }
        else
        {
            s32 width, height;
            glfwGetFramebufferSize(_window, &width, &height);

            extent = VkExtent2D {
                static_cast<u32>(width),
                static_cast<u32>(height)
            };

            extent.width = eastl::clamp(extent.width,
                                        capabilities.m_surfaceCapabilities.minImageExtent.width,
                                        capabilities.m_surfaceCapabilities.maxImageExtent.width);
            extent.height = eastl::clamp(extent.height,
                                         capabilities.m_surfaceCapabilities.minImageExtent.height,
                                         capabilities.m_surfaceCapabilities.maxImageExtent.height);
        }

        m_nextSwapChain->m_framebufferSize = { extent.width, extent.height };

        {
            eastl::vector<u32> queueFamilyIndices {};
            if (m_sharingMode == VK_SHARING_MODE_CONCURRENT)
                queueFamilyIndices = _queueIndices.RetrieveDifferentFamilies();

            VkSwapchainCreateInfoKHR createInfo = m_reCreateInfo;
            createInfo.imageExtent = extent;
            createInfo.preTransform = capabilities.m_surfaceCapabilities.currentTransform;
            createInfo.queueFamilyIndexCount = static_cast<u32>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
            createInfo.oldSwapchain = m_currentSwapChain->m_swapChain;

            VkAssert(vkCreateSwapchainKHR(_device, &createInfo, nullptr, &m_nextSwapChain->m_swapChain));
        }

        {
            u32 imageCount;
            VkAssert(vkGetSwapchainImagesKHR(_device, m_nextSwapChain->m_swapChain,  &imageCount, nullptr));
            DynamicArray<VkImage> images(m_allocator);
            images.Resize(imageCount);
            VkAssert(vkGetSwapchainImagesKHR(_device, m_nextSwapChain->m_swapChain, &imageCount, images.Data()));
            KE_ASSERT_MSG(imageCount > 0, "Unable to retrieve swapchain images");

            m_nextSwapChain->m_renderTargetTextures.Resize(imageCount);
            m_nextSwapChain->m_renderTargetViews.Resize(imageCount);
            m_nextSwapChain->m_imageAvailableSemaphores.Resize(imageCount);

            char name[256];
#if !defined(KE_FINAL)
            if (m_debugHandler != nullptr)
            {
                snprintf(name, sizeof(name), "Swapchain");
                m_debugHandler->SetName(_device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<u64>(m_nextSwapChain->m_swapChain), name);
            }
#endif

            for (u32 i = 0; i < imageCount; ++i)
            {

                const auto textureHandle = _resources.RegisterTexture(images[i], {extent.width, extent.height, 1});
#if !defined(KE_FINAL)
                if (m_debugHandler != nullptr)
                {
                    snprintf(name, sizeof(name), "Swapchain/Texture[%d]", i);
                    m_debugHandler->SetName(_device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(images[i]), name);
                }
                snprintf(name, sizeof(name), "Swapchain/RTV[%d]", i);
#endif
                const RenderTargetViewDesc rtvDesc {
                    .m_texture = textureHandle,
                    .m_format = VkHelperFunctions::FromVkFormat(m_reCreateInfo.imageFormat),
#if !defined(KE_FINAL)
                    .m_debugName = { name, m_allocator },
#endif
                };

                m_nextSwapChain->m_renderTargetTextures.Init(i, textureHandle);
                m_nextSwapChain->m_renderTargetViews.Init(i, _resources.CreateRenderTargetView(rtvDesc, _device));
                {
                    VkSemaphoreCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                    VkAssert(vkCreateSemaphore(_device, &createInfo, nullptr, &m_nextSwapChain->m_imageAvailableSemaphores[i]));
#if !defined(KE_FINAL)
                    if (m_debugHandler != nullptr)
                    {
                        snprintf(name, sizeof(name), "Swapchain/ImageAvailableSemaphore[%d]", i);
                        m_debugHandler->SetName(_device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<u64>(m_nextSwapChain->m_imageAvailableSemaphores[i]), name);
                    }
#endif
                }
            }
        }

        return true;
    }

    void VkSwapChain::AcquireNextImage(const VkDevice _device, const u8 _frameIndex)
    {
        KE_ZoneScopedFunction("VkSwapChain::AcquireNextImage");

        SwapChainData* swapChain = m_nextSwapChain == nullptr ? m_currentSwapChain : m_nextSwapChain;
        const VkResult result = vkAcquireNextImageKHR(
                _device,
                swapChain->m_swapChain,
                UINT64_MAX,
                swapChain->m_imageAvailableSemaphores[_frameIndex],
                VK_NULL_HANDLE,
                &m_imageIndex);
        switch (result)
        {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            break;
        default:
            KE_ERROR("Unhandled error %d", result);
        }
    }

    void VkSwapChain::Present(
        const VkQueue _presentQueue,
        const eastl::span<VkSemaphore> &_semaphores,
        const u64 _frameId)
    {
        KE_ZoneScopedFunction("VkSwapChain::Present");

        SwapChainData* swapChain = GetSwapChain(_frameId);

	    const VkPresentInfoKHR presentInfo = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = static_cast<uint32_t>(_semaphores.size()),
                .pWaitSemaphores = _semaphores.data(),
                .swapchainCount = 1,
                .pSwapchains = &swapChain->m_swapChain,
                .pImageIndices = &m_imageIndex,
        };

        const VkResult result = vkQueuePresentKHR(_presentQueue, &presentInfo);
        switch (result)
        {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            break;
        default:
            KE_ERROR("Unhandled error %d", result);
        }
    }

    void VkSwapChain::Update(const VkDevice _device, VkResources& _resources, const u64 _frameId)
    {
        if (m_nextSwapChain != nullptr && _frameId >= m_nextSwapChainTransitionFrame + m_nextSwapChain->m_imageAvailableSemaphores.Size() - 1)
        {
            DestroySwapChain(_device, _resources, m_currentSwapChain);
            m_currentSwapChain = m_nextSwapChain;
            m_nextSwapChain = nullptr;
        }
    }

    void VkSwapChain::Destroy(const VkDevice _device, VkResources &_resources) const
    {
        SwapChainData* swapChains[2] = { m_currentSwapChain, m_nextSwapChain };

        for (auto* swapChain : swapChains)
        {
            if (swapChain == nullptr)
                continue;

            DestroySwapChain(_device, _resources, swapChain);
        }
    }

#if !defined(KE_FINAL)
    void VkSwapChain::SetDebugHandler(const eastl::shared_ptr<VkDebugHandler>& _handler, const VkDevice _device)
    {
        m_debugHandler = _handler;

        char name[256];
        {
            snprintf(name, sizeof(name), "Swapchain");
            m_debugHandler->SetName(_device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<u64>(m_currentSwapChain->m_swapChain), name);
        }

        DynamicArray<VkImage> imageArray;
        u32 imageCount = m_currentSwapChain->m_imageAvailableSemaphores.Size();
        imageArray.Resize(imageCount);
        vkGetSwapchainImagesKHR(_device, m_currentSwapChain->m_swapChain, &imageCount, imageArray.Data());
        for (auto i = 0u; i < imageCount; i++)
        {
            {
                snprintf(name, sizeof(name), "Swapchain/Texture[%d]", i);
                m_debugHandler->SetName(_device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(imageArray[i]), name);
            }

            {
                snprintf(name, sizeof(name), "Swapchain/ImageAvailableSemaphore[%d]", i);
                m_debugHandler->SetName(_device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<u64>(m_currentSwapChain->m_imageAvailableSemaphores[i]), name);
            }
        }
    }
#endif

    VkSwapChain::SwapChainData* VkSwapChain::GetSwapChain(const u64 _frameId) const
    {
        return m_nextSwapChain != nullptr && _frameId >= m_nextSwapChainTransitionFrame
            ? m_nextSwapChain
            : m_currentSwapChain;
    }

    void VkSwapChain::DestroySwapChain(const VkDevice _device, VkResources& _resources, SwapChainData* _swapChain) const
    {
        for (const auto handle: _swapChain->m_renderTargetViews)
        {
            KE_ASSERT_MSG(_resources.FreeRenderTargetView(handle, _device),
                          "Handle was invalid. It shouldn't. Something went wrong with the lifecycle.");
        }
        _swapChain->m_renderTargetViews.Clear();

        for (const auto handle: _swapChain->m_renderTargetTextures)
        {
            // Free the texture from the gen pool, but don't do a release of the VkImage, as it's handled
            // by the swapchain
            KE_ASSERT_MSG(_resources.ReleaseTexture(handle, _device, false),
                          "Handle was invalid. It shouldn't. Something went wrong with the lifecycle.");
        }
        _swapChain->m_renderTargetTextures.Clear();

        for (const auto semaphore: _swapChain->m_imageAvailableSemaphores)
        {
            vkDestroySemaphore(_device, semaphore, nullptr);
        }

        vkDestroySwapchainKHR(_device, SafeReset(_swapChain->m_swapChain), nullptr);

        m_allocator.deallocate(_swapChain);
    }
}
