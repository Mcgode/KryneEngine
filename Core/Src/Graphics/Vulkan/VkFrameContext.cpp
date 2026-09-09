/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2023.
 */

#include "Graphics/Vulkan/VkFrameContext.hpp"

#include "Graphics/Vulkan/HelperFunctions.hpp"
#include "Graphics/Vulkan/VkDebugHandler.hpp"

namespace KryneEngine
{
    VkFrameContext::VkFrameContext(
        AllocatorInstance _allocator,
        VkDevice _device,
        const VkCommonStructures::QueueIndices &_queueIndices,
        u32 _timestampPoolSize)
        : m_allocator(_allocator)
    {
        const auto CreateCommandPool = [this, _device](
                const VkCommonStructures::QueueIndices::Pair& _pair,
                CommandPoolSet& _commandPoolSet)
        {
            KE_ZoneScopedFunction("VkFrameContext::CreateCommandPool");

            if (!_pair.IsInvalid())
            {
                // Create command pool
	            {
		            const VkCommandPoolCreateInfo createInfo {
                            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                            .queueFamilyIndex =  static_cast<u32>(_pair.m_indexInFamily)
		            };

                    VkAssert(vkCreateCommandPool(_device, &createInfo, nullptr, &_commandPoolSet.m_commandPool));
	            }

                // Create fence
	            {
		            constexpr VkFenceCreateInfo createInfo {
                            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                            .flags = VK_FENCE_CREATE_SIGNALED_BIT // Create as signaled for first wait
					};
                    VkAssert(vkCreateFence(_device, &createInfo, nullptr, &_commandPoolSet.m_fence));

                    // Save fences into single array for mutualized waits and resets
                    m_fencesArray.push_back(_commandPoolSet.m_fence);
	            }

                // Create semaphore
	            {
                    constexpr VkSemaphoreCreateInfo createInfo = {
                            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                            .flags = 0
                    };
                    VkAssert(vkCreateSemaphore(_device, &createInfo, nullptr, &_commandPoolSet.m_semaphore));
	            }
            }
        };

        CreateCommandPool(_queueIndices.m_graphicsQueueIndex, m_graphicsCommandPoolSet);
        CreateCommandPool(_queueIndices.m_computeQueueIndex, m_computeCommandPoolSet);
        CreateCommandPool(_queueIndices.m_transferQueueIndex, m_transferCommandPoolSet);

        if (_timestampPoolSize > 0)
        {
            m_timestampPoolSize = _timestampPoolSize;

            const VkQueryPoolCreateInfo timestampQueryPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = m_timestampPoolSize,
            };
            VkAssert(vkCreateQueryPool(
                _device,
                &timestampQueryPoolCreateInfo,
                nullptr,
                &m_timestampQueryPool));

            m_resolvedTimestamps = m_allocator.Allocate<u64>(m_timestampPoolSize);
        }
    }

    VkFrameContext::~VkFrameContext()
    {
        KE_ASSERT(!m_graphicsCommandPoolSet.m_commandPool);
        KE_ASSERT(!m_computeCommandPoolSet.m_commandPool);
        KE_ASSERT(!m_transferCommandPoolSet.m_commandPool);
    }

#if !defined(KE_FINAL)
    void VkFrameContext::SetDebugHandler(const eastl::shared_ptr<VkDebugHandler> &_debugHandler, VkDevice _device, u8 _frameIndex)
    {
        eastl::string baseName;
        baseName.sprintf("VkFrameContext[%d]/", _frameIndex);

        m_graphicsCommandPoolSet.SetDebugHandler(_debugHandler, _device, baseName + "/GraphicsPoolSet");
        m_computeCommandPoolSet.SetDebugHandler(_debugHandler, _device, baseName + "/ComputePoolSet");
        m_transferCommandPoolSet.SetDebugHandler(_debugHandler, _device, baseName + "/TransferPoolSet");
    }
#endif

    u32 VkFrameContext::PutTimestamp(VkCommandBuffer _commandBuffer)
    {
        const u32 queryIndex = m_timestampPoolIndex.fetch_add(1, std::memory_order_acquire);
        vkCmdWriteTimestamp(_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestampQueryPool, queryIndex);
        return queryIndex;
    }

    void VkFrameContext::Destroy(VkDevice _device)
    {
        m_graphicsCommandPoolSet.Destroy(_device);
        m_computeCommandPoolSet.Destroy(_device);
        m_transferCommandPoolSet.Destroy(_device);

        if (m_timestampQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(_device, SafeReset(m_timestampQueryPool), nullptr);
            m_allocator.deallocate(m_resolvedTimestamps);
            m_resolvedTimestamps = nullptr;
        }
    }

    void VkFrameContext::WaitForFences(VkDevice _device, u64 _frameId) const
    {
        KE_ZoneScopedFunction("VkFrameContext::WaitForFrame");

        // If fences have already been reset to a later frame, then previous fence was signaled, no need to wait.
        if (m_frameId > _frameId)
        {
            return;
        }

        VkAssert(vkWaitForFences(_device, m_fencesArray.size(), m_fencesArray.data(), true, UINT64_MAX));
    }

    void VkFrameContext::ResolveTimestamps(VkDevice _device, double _timestampPeriod, u64 _timestampSyncOffset)
    {
        if (m_timestampQueryPool == VK_NULL_HANDLE)
            return;

        if (m_lastResolvedFrame != ~0ull && m_lastResolvedFrame == m_frameId)
            return;
        m_lastResolvedFrame = m_frameId;

        const u32 count = m_timestampPoolIndex.load(std::memory_order_relaxed);

        if (count > 0)
        {
            vkGetQueryPoolResults(
                _device,
                m_timestampQueryPool,
                0,
                count,
                sizeof(u64) * count,
                m_resolvedTimestamps,
                sizeof(u64),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            for (u32 i = 0; i < count; i++)
            {
                m_resolvedTimestamps[i] =
                    static_cast<u64>(static_cast<double>(m_resolvedTimestamps[i]) * _timestampPeriod)
                    + _timestampSyncOffset;
            }
        }

        m_timestampPoolNeedsReset = true;
        m_timestampPoolIndex.store(0, std::memory_order_release);
    }

    VkCommandBuffer VkFrameContext::CommandPoolSet::BeginCommandBuffer(VkDevice _device)
    {
        KE_ZoneScopedFunction("VkFrameContext::CommandPoolSet::BeginCommandBuffer");

        const auto lock = m_mutex.AutoLock();

        if (m_availableCommandBuffers.empty())
        {
            KE_ZoneScoped("Allocate new command buffer");

            const VkCommandBufferAllocateInfo allocateInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = m_commandPool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1
            };
            VkAssert(vkAllocateCommandBuffers(_device, &allocateInfo, &m_usedCommandBuffers.push_back()));

#if !defined(KE_FINAL)
            if (m_debugHandler != nullptr)
            {
                eastl::string name;
                name.sprintf("%s/CommandBuffer[%zu]", m_baseDebugString.c_str(), m_usedCommandBuffers.size() - 1);
                ZoneText(name.c_str(), name.size());
                m_debugHandler->SetName(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, (u64)m_usedCommandBuffers.back(), name);
            }
#endif
        }
        else
        {
            m_usedCommandBuffers.push_back(m_availableCommandBuffers.back());
            m_availableCommandBuffers.pop_back();
        }

        VkCommandBuffer commandBuffer = m_usedCommandBuffers.back();

        const VkCommandBufferBeginInfo beginInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VkAssert(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        return commandBuffer;
    }

    void VkFrameContext::CommandPoolSet::EndCommandBuffer(VkCommandBuffer _commandList)
    {
        KE_ZoneScopedFunction("VkFrameContext::CommandPoolSet::EndCommandBuffer");

        const auto lock = m_mutex.AutoLock();

        const auto it = eastl::find(m_usedCommandBuffers.begin(), m_usedCommandBuffers.end(), _commandList);
        if (KE_VERIFY(it != m_usedCommandBuffers.end()))
        {
            vkEndCommandBuffer(_commandList);
        }
    }

    void VkFrameContext::CommandPoolSet::Reset()
    {
        KE_ZoneScopedFunction("VkFrameContext::CommandPoolSet::Reset");

        const auto lock = m_mutex.AutoLock();

        for (auto commandBuffer: m_usedCommandBuffers)
        {
            vkResetCommandBuffer(commandBuffer, 0);
        }

        m_availableCommandBuffers.insert(
                m_availableCommandBuffers.end(),
                m_usedCommandBuffers.begin(),
                m_usedCommandBuffers.end());
        m_usedCommandBuffers.clear();
    }

    void VkFrameContext::CommandPoolSet::Destroy(VkDevice _device)
    {
        vkDestroySemaphore(_device, SafeReset(m_semaphore), nullptr);

        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetFenceStatus.html
        KE_ASSERT_MSG(!m_fence || vkGetFenceStatus(_device, m_fence) == VK_SUCCESS, "Fence should be signaled by the time the frame is destroyed");
        vkDestroyFence(_device, SafeReset(m_fence), nullptr);

        if (!m_usedCommandBuffers.empty())
        {
            Reset();
        }

        const auto lock = m_mutex.AutoLock();
        KE_ASSERT_MSG(m_usedCommandBuffers.empty(), "PoolSet should be reset before destroy");

        if (!m_usedCommandBuffers.empty())
        {
            vkFreeCommandBuffers(_device, m_commandPool, m_usedCommandBuffers.size(), m_usedCommandBuffers.data());
        }
        if (!m_availableCommandBuffers.empty())
        {
            vkFreeCommandBuffers(_device, m_commandPool, m_availableCommandBuffers.size(), m_availableCommandBuffers.data());
        }

        vkDestroyCommandPool(_device, SafeReset(m_commandPool), nullptr);
    }

#if !defined(KE_FINAL)
    void VkFrameContext::CommandPoolSet::SetDebugHandler(
            const eastl::shared_ptr<VkDebugHandler> &_handler,
            VkDevice _device,
            const eastl::string_view &_baseString)
    {
        m_debugHandler = _handler;
        m_baseDebugString = _baseString;

        m_debugHandler->SetName(_device, VK_OBJECT_TYPE_SEMAPHORE, (u64)m_semaphore, m_baseDebugString + "Semaphore");
        m_debugHandler->SetName(_device, VK_OBJECT_TYPE_FENCE, (u64)m_fence, m_baseDebugString + "Fence");

        m_debugHandler->SetName(_device, VK_OBJECT_TYPE_COMMAND_POOL, (u64)m_commandPool, m_baseDebugString + "CommandPool");
    }
#endif
} // KryneEngine