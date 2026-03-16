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
#include "KryneEngine/Core/Graphics/Handles.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

struct GLFWwindow;

namespace KryneEngine
{
    class VkSurface;
    struct VkResources;
    class VkDebugHandler;

    class VkSwapChain
    {
        friend class VkGraphicsContext;

    public:
        explicit VkSwapChain(AllocatorInstance _allocator);

        void Init(const GraphicsCommon::ApplicationInfo &_appInfo,
                    VkDevice _device,
                    const VkSurface &_surface,
                    VkResources &_resources,
                    GLFWwindow *_window,
                    const VkCommonStructures::QueueIndices &_queueIndices,
                    u64 _currentFrameIndex);

        virtual ~VkSwapChain();

        bool RecreateSwapChain(
            VkDevice _device,
            const VkSurface& _surface,
            VkResources& _resources,
            GLFWwindow* _window,
            const VkCommonStructures::QueueIndices &_queueIndices, u64 _frameId);

        void AcquireNextImage(VkDevice _device, u8 _frameIndex);

        void Present(VkQueue _presentQueue, const eastl::span<VkSemaphore> &_semaphores, u64 _frameId);

        void Update(VkDevice _device, VkResources& _resources, u64 _frameId);

        void Destroy(VkDevice _device, VkResources& _resources) const;

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

            explicit SwapChainData(const AllocatorInstance _allocator)
                : m_renderTargetTextures(_allocator)
                , m_renderTargetViews(_allocator)
                , m_imageAvailableSemaphores(_allocator)
            {}
        };

        AllocatorInstance m_allocator;
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
