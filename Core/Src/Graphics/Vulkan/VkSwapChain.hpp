/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2022.
 */

#pragma once

#include <EASTL/shared_ptr.h>
#include <EASTL/span.h>

#include "Graphics/Vulkan/CommonStructures.hpp"
#include "Graphics/Vulkan/VkHeaders.hpp"
#include "Graphics/Vulkan/VkSurface.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine
{
    struct VkResources;
    class VkDebugHandler;

    class VkSwapChain
    {
        friend class VkGraphicsContext;

    public:
        explicit VkSwapChain(AllocatorInstance _allocator);

        void Init(const GraphicsCommon::ApplicationInfo &_appInfo,
                    VkDevice _device,
                    VkInstance _instance,
                    VkPhysicalDevice _physicalDevice,
                    VkResources &_resources,
                    const SwapChainDesc& _desc,
                    const VkCommonStructures::QueueIndices &_queueIndices,
                    u64 _currentFrameIndex);

        virtual ~VkSwapChain();

        bool RecreateSwapChain(
            VkDevice _device,
            VkPhysicalDevice _physicalDevice,
            VkResources& _resources,
            uint2 _newSize,
            u64 _frameId);

        void AcquireNextImage(VkDevice _device, u8 _frameIndex);

        void Present(VkQueue _presentQueue, const eastl::span<VkSemaphore> &_semaphores, u64 _frameId);

        void Update(VkDevice _device, VkResources& _resources, u64 _frameId);

        void Destroy(VkDevice _device, VkInstance _instance, VkResources& _resources);

        [[nodiscard]] uint2 GetFramebufferSize(u64 _frameId) const { return GetSwapChain(_frameId)->m_framebufferSize; }
        [[nodiscard]] VkFormat GetFormat(u64 _frameId) const { return GetSwapChain(_frameId)->m_format; }
        [[nodiscard]] u32 GetCurrentImageIndex() const { return m_imageIndex; }

        [[nodiscard]] RenderTargetViewHandle GetRenderTargetView(u64 _frameId, u8 _index) const
        {
            return GetSwapChain(_frameId)->m_renderTargetViews[_index];
        }
        [[nodiscard]] TextureHandle GetTexture(u64 _frameId, u8 _index) const
        {
            return GetSwapChain(_frameId)->m_renderTargetTextures[_index];
        }
        [[nodiscard]] VkSemaphore GetImageAvailableSemaphore(u64 _frameId, u8 _frameIndex) const
        {
            return GetSwapChain(_frameId)->m_imageAvailableSemaphores[_frameIndex];
        }
        [[nodiscard]] u8 GetImageCount(u64 _frameId) const
        {
            return static_cast<u8>(GetSwapChain(_frameId)->m_renderTargetViews.Size());
        }

#if !defined(KE_FINAL)
        void SetDebugHandler(const eastl::shared_ptr<VkDebugHandler> &_handler, VkDevice _device);
#endif

    private:
        struct SwapChainData
        {
            VkSwapchainKHR m_swapChain {};
            DynamicArray<TextureHandle> m_renderTargetTextures;
            DynamicArray<RenderTargetViewHandle> m_renderTargetViews;
            DynamicArray<VkSemaphore> m_imageAvailableSemaphores;
            uint2 m_framebufferSize {};
            VkFormat m_format = VK_FORMAT_UNDEFINED;

            explicit SwapChainData(const AllocatorInstance _allocator)
                : m_renderTargetTextures(_allocator)
                , m_renderTargetViews(_allocator)
                , m_imageAvailableSemaphores(_allocator)
            {}
        };

        AllocatorInstance m_allocator;
        VkSurface m_surface;
        SwapChainDesc m_desc {};
        VkCommonStructures::QueueIndices m_queueIndices {};
        VkSwapchainCreateInfoKHR m_reCreateInfo {};
        VkSharingMode m_sharingMode {};
        SwapChainData* m_currentSwapChain = nullptr;
        SwapChainData* m_nextSwapChain = nullptr;
        u64 m_nextSwapChainTransitionFrame = 0;
        u32 m_imageIndex = 0;
#if !defined(KE_FINAL)
        eastl::shared_ptr<VkDebugHandler> m_debugHandler = nullptr;
#endif

        [[nodiscard]] SwapChainData* GetSwapChain(u64 _frameId) const;

        void DestroySwapChain(VkDevice _device, VkResources& _resources, SwapChainData* _swapChain) const;
    };
}
