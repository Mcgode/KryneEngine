/**
 * @file
 * @author Max Godefroy
 * @date 05/02/2026.
 */

#pragma once

#include <thread>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.hpp>
#include <KryneEngine/Core/Memory/Containers/ConcurrentQueue.hpp>
#include <KryneEngine/Core/Memory/Containers/Pool.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Threads/SpinLock.hpp>

#include "KryneEngine/Modules/Resources/IResourceLoader.hpp"

namespace KryneEngine::Modules::Resources
{
    /**
     * @brief Resource loader reading files on dedicated IO threads, then finalizing them in fiber jobs.
     *
     * @details
     * The `u64` load flags passed to `RequestLoad()` hold two pieces of information, whose bit layout is provided by
     * the user through `Desc`:
     *  - **Priority** (`Desc::m_priorityMask` bits): read as `_loadFlags & m_priorityMask`, without any shift.
     *  **The lower the value, the higher the priority.**
     *      - `kBlockingPriority` (0) is the highest priority: the resource is loaded synchronously, blocking the
     *      calling thread until it is ready. Use sparingly.
     *      - Values in `[Desc::m_firstAsyncPriority, Desc::m_lastAsyncPriority]` are async priorities. Each one
     *      gets its own queue, and the IO threads always serve the non-empty queue with the lowest value first.
     *      - `Desc::m_loadingScreenPriority` is one of the async priorities, whose requests are also tracked by
     *      `GetLoadingScreenProgress()`. Where it sits in the async range is left to the user.
     *      - Any other value is invalid.
     *  - **Force reload** (`Desc::m_forceReloadFlag` bit): forces a new load, even if a request for the same entry is
     *  already pending. Typically used for hot reloading.
     *
     * When a resource entry already has a pending request, a new request for it cancels and replaces the pending one
     * only if it has a higher priority (lower value) or the force reload flag. Otherwise, the new request is ignored.
     *
     * For example, with `m_priorityMask = 0b111`, `m_forceReloadFlag = 0b1000`, `m_firstAsyncPriority = 1` and
     * `m_lastAsyncPriority = 5`:
     *  - `0` is a blocking load.
     *  - `1` is the most urgent async load, `5` the least urgent one.
     *  - `0b1011` is a forced reload with async priority `3`.
     */
    class AsyncResourceLoader final: public IResourceLoader
    {
    public:
        /// The priority loading synchronously on the calling thread. It is the highest possible priority.
        static constexpr u64 kBlockingPriority = 0;

        struct Desc
        {
            /// The load flags bits holding the load priority. They must include all the async priority values.
            u64 m_priorityMask;

            /// The load flags bit forcing a new load, even if a request for the same entry is pending. It must not
            /// overlap `m_priorityMask`.
            u64 m_forceReloadFlag;

            /// The inclusive range of async priorities, each one getting its own queue.
            /// `m_firstAsyncPriority` is the most urgent one (lowest value, served first), and must be greater than
            /// `kBlockingPriority`. `m_lastAsyncPriority` is the least urgent one (highest value, served last).
            u64 m_firstAsyncPriority;
            u64 m_lastAsyncPriority;

            /// The async priority whose requests are tracked by `GetLoadingScreenProgress()`. It must be within the
            /// async priority range.
            u64 m_loadingScreenPriority;
        };

        AsyncResourceLoader(
            AllocatorInstance _allocator,
            FileSystem::VirtualFileSystem* _vfs,
            size_t _ioThreadCount,
            const Desc& _desc);

        /**
         * @brief Stops the IO threads, and waits for the finalization jobs still in flight.
         *
         * @details
         * Requests that weren't picked up by an IO thread yet are dropped without being loaded. As it waits for
         * fiber jobs, the loader must be destroyed before the `FibersManager`, and not from a fiber job.
         */
        ~AsyncResourceLoader() override;

        void RequestLoad(
            const StringHash& _path,
            ResourceEntry* _entry,
            IResourceManager* _resourceManager,
            u64 _loadFlags) override;

        template <class Enum>
        void RequestLoad(
            const StringHash& _path,
            ResourceEntry* _entry,
            IResourceManager* _resourceManager,
            Enum _loadFlags)
        {
            RequestLoad(_path, _entry, _resourceManager, static_cast<u64>(_loadFlags));
        }

        [[nodiscard]] bool AllowReload(u64 _loadFlags) override;

        /**
         * @return A pair of u32 values with the following format: `[loaded, remaining]`
         */
        [[nodiscard]] eastl::pair<u32, u32> GetLoadingScreenProgress() const;

        /**
         * @brief Tries to restore the loading screen progress to its initial state.
         *
         * @return True if the loading screen progress was reset, false otherwise
         */
        bool ResetLoadingScreenProgress();

    private:
        // `Pool` neither constructs nor destroys its elements, so requests must stay trivially copyable.
        struct LoadRequest
        {
            /// Null-terminated copy of the path, owned by the request.
            char* m_path;
            size_t m_pathSize;
            ResourceEntry* m_entry;
            IResourceManager* m_resourceManager;
            u64 m_loadFlags;
            bool m_canceled;
        };
        static_assert(std::is_trivially_copyable_v<LoadRequest>);

        AllocatorInstance m_allocator;
        Desc m_desc;

        DynamicArray<std::thread> m_ioThreads;
        std::mutex m_ioThreadsMutex;
        std::condition_variable m_ioThreadsConditionVariable;
        std::atomic_bool m_shouldStop = false;
        FibersManager* m_fibersManager = nullptr;
        std::atomic<u32> m_pendingFinalizations { 0 };

        SpinLock m_poolLock;
        std::atomic<u64> m_loadingScreenProgress { 0 };
        FlatHashMap<ResourceEntry*, u32> m_requestMap;
        Pool<LoadRequest> m_loadRequests;
        DynamicArray<ConcurrentQueue<u32>> m_queues;

        /// Must be called with `m_poolLock` held.
        u32 CreateRequest(
            const StringHash& _path,
            ResourceEntry* _entry,
            IResourceManager* _resourceManager,
            u64 _loadFlags);

        [[nodiscard]] bool HasQueuedRequests() const;

        void LoadFile(u32 _request, bool _async);

        void MarkRequestDone(u32 _requestId, const LoadRequest& _request);
    };
}
