/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#pragma once

#include <EASTL/shared_ptr.h>
#include <EASTL/fixed_vector.h>

#include "Graphics/Vulkan/CommonStructures.hpp"
#include "Graphics/Vulkan/VkHeaders.hpp"
#include "KryneEngine/Core/Threads/LightweightMutex.hpp"

namespace KryneEngine
{
    class VkDebugHandler;

    class VkFrameContext
    {
        friend class VkGraphicsContext;

    public:
        VkFrameContext(
            AllocatorInstance _allocator,
            VkDevice _device,
            const VkCommonStructures::QueueIndices& _queueIndices,
            u32 _timestampPoolSize);

        virtual ~VkFrameContext();

#if !defined(KE_FINAL)
        void SetDebugHandler(const eastl::shared_ptr<VkDebugHandler> &_debugHandler, VkDevice _device, u8 _frameIndex);
#endif

        u32 PutTimestamp(VkCommandBuffer _commandBuffer);

        void Destroy(VkDevice _device);

        VkCommandBuffer BeginGraphicsCommandBuffer(VkDevice _device)
        {
            VkCommandBuffer commandBuffer = m_graphicsCommandPoolSet.BeginCommandBuffer(_device);

            if (m_timestampQueryPool != VK_NULL_HANDLE && m_timestampPoolNeedsReset)
            {
                vkCmdResetQueryPool(commandBuffer, m_timestampQueryPool, 0, m_timestampPoolSize);
                m_timestampPoolNeedsReset = false;
            }

            return commandBuffer;
        }

        void EndGraphicsCommandBuffer(VkCommandBuffer _commandList)
        {
            m_graphicsCommandPoolSet.EndCommandBuffer(_commandList);
        }

        VkCommandBuffer BeginComputeCommandBuffer(VkDevice _device)
        {
            return m_computeCommandPoolSet.BeginCommandBuffer(_device);
        }

        void EndComputeCommandBuffer(VkCommandBuffer _commandList)
        {
            m_computeCommandPoolSet.EndCommandBuffer(_commandList);
        }

        VkCommandBuffer BeginTransferCommandBuffer(VkDevice _device)
        {
            return m_transferCommandPoolSet.BeginCommandBuffer(_device);
        }

        void EndTransferCommandBuffer(VkCommandBuffer _commandList)
        {
            m_transferCommandPoolSet.EndCommandBuffer(_commandList);
        }

        void WaitForFences(VkDevice _device, u64 _frameId) const;

        void ResolveTimestamps(VkDevice _device, double _timestampPeriod, u64 _timestampSyncOffset);

    private:
        struct CommandPoolSet
        {
            u32 m_queueFamilyIndex = ~0u;

            // Each used/available command buffer has its own dedicated command pool (parallel
            // arrays, same index in both pairs): VkCommandPool is not thread-safe, and the spec
            // requires external synchronization on the pool a command buffer was allocated from
            // for every recording call on that buffer, not just allocation/reset. Giving each
            // buffer its own pool lets it be recorded from whichever thread it's dispatched to
            // without contending with any other in-flight buffer.
            eastl::vector<VkCommandPool> m_availableCommandPools;
            eastl::vector<VkCommandBuffer> m_availableCommandBuffers;

            eastl::vector<VkCommandPool> m_usedCommandPools;
            eastl::vector<VkCommandBuffer> m_usedCommandBuffers;

            LightweightMutex m_mutex {};

            VkFence m_fence = VK_NULL_HANDLE;
            VkSemaphore m_semaphore = VK_NULL_HANDLE;

#if !defined(KE_FINAL)
            eastl::shared_ptr<VkDebugHandler> m_debugHandler;
            eastl::string m_baseDebugString;

            void SetDebugHandler(const eastl::shared_ptr<VkDebugHandler>& _handler, VkDevice _device, const eastl::string_view &_baseString);
#endif

            VkCommandBuffer BeginCommandBuffer(VkDevice _device);
            void EndCommandBuffer(VkCommandBuffer _commandList);

            void Reset(VkDevice _device);

            void Destroy(VkDevice _device);
        };

        static constexpr u8 kMaxQueueCount = 3;

        CommandPoolSet m_graphicsCommandPoolSet;
        CommandPoolSet m_computeCommandPoolSet;
        CommandPoolSet m_transferCommandPoolSet;
        eastl::fixed_vector<VkFence, kMaxQueueCount> m_fencesArray;
        u64 m_frameId = 0;

        AllocatorInstance m_allocator;
        VkQueryPool m_timestampQueryPool = VK_NULL_HANDLE;
        std::atomic<u32> m_timestampPoolIndex = 0;
        u32 m_timestampPoolSize = 0;
        bool m_timestampPoolNeedsReset = true;
        u64 m_lastResolvedFrame = ~0ull;
        u64* m_resolvedTimestamps = nullptr;
    };
} // KryneEngine