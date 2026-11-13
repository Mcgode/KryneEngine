/**
 * @file
 * @author Max Godefroy
 * @date 17/01/2026.
 */

#pragma once

#include <KryneEngine/Core/Threads/HelperFunctions.hpp>
#include <atomic>
#include <limits>

namespace KryneEngine::Modules::Resources
{
    class IResourceManager;

    struct ResourceEntry
    {
        std::atomic<void*> m_resource = nullptr;
        std::atomic<size_t> m_version = 0;
        IResourceManager* m_manager = nullptr;
        std::atomic<s64> m_refCount = 0;

        static constexpr s64 kUnloading = std::numeric_limits<s64>::min();

        /**
         * @return `false` if the entry is unloaded. If it is, the caller should load the resource.
         */
        bool TryUseResource()
        {
            s64 current = m_refCount.load(std::memory_order::relaxed);

            // Safely try to increment the ref count from a value greater than zero.
            // Stall while an unload is in progress.
            for (;;)
            {
                if (current == kUnloading)
                {
                    Threads::CpuYield();
                    current = m_refCount.load(std::memory_order::relaxed);
                }
                else if (current == 0)
                {
                    return false;
                }
                else if (m_refCount.compare_exchange_weak(
                    current,
                    current + 1,
                    std::memory_order::acquire,
                    std::memory_order::relaxed))
                {
                    return true;
                }
            }
        }

        template <class Resource>
        Resource* GetResource() const
        {
            return static_cast<Resource*>(m_resource.load(std::memory_order::acquire));
        }
    };
}