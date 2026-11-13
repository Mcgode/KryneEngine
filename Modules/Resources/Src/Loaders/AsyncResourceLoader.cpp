/**
 * @file
 * @author Max Godefroy
 * @date 05/02/2026.
 */

#include "KryneEngine/Modules/Resources/Loaders/AsyncResourceLoader.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.inl>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp>

#include "KryneEngine/Modules/Resources/IResourceManager.hpp"

namespace KryneEngine::Modules::Resources
{
    AsyncResourceLoader::AsyncResourceLoader(
        AllocatorInstance _allocator,
        FileSystem::VirtualFileSystem* _vfs,
        const size_t _ioThreadCount,
        const Desc& _desc)
            : IResourceLoader(_vfs)
            , m_allocator(_allocator)
            , m_desc(_desc)
            , m_ioThreads(_allocator, _ioThreadCount)
            , m_requestMap(_allocator)
            , m_loadRequests(_allocator, 32)
            , m_queues(_allocator)
    {
        KE_ASSERT((_desc.m_priorityMask & _desc.m_forceReloadFlag) == 0);
        KE_ASSERT(_desc.m_firstAsyncPriority > kBlockingPriority);
        KE_ASSERT(_desc.m_firstAsyncPriority <= _desc.m_lastAsyncPriority);
        KE_ASSERT((_desc.m_lastAsyncPriority & ~_desc.m_priorityMask) == 0);
        KE_ASSERT(_desc.m_loadingScreenPriority >= _desc.m_firstAsyncPriority
            && _desc.m_loadingScreenPriority <= _desc.m_lastAsyncPriority);

        if (ConcurrentQueueTraits::s_globalConcurrentQueueAllocator == nullptr)
        {
            ConcurrentQueueTraits::s_globalConcurrentQueueAllocator = _allocator;
        }

        m_queues.Resize(_desc.m_lastAsyncPriority - _desc.m_firstAsyncPriority + 1);
        m_queues.InitAll();

        m_fibersManager = FibersManager::GetInstance();

        for (auto i = 0; i < _ioThreadCount; ++i)
        {
            m_ioThreads.Init(i, [this, _allocator, i]()
            {
                eastl::string name { _allocator };
                tracy::SetThreadName(name.sprintf("AsyncResourceLoader IO Thread %d", i).c_str());

                while (m_shouldStop.load(std::memory_order::relaxed) == false)
                {
                    bool gotWork = false;
                    u32 request = 0;

                    for (auto& queue: m_queues)
                    {
                        gotWork = queue.try_dequeue(request);
                        if (gotWork)
                            break;
                    }

                    if (!gotWork)
                    {
                        // The predicate is checked under the mutex, so a request enqueued (or a stop requested) right
                        // after the queues were checked can't be missed.
                        std::unique_lock lock(m_ioThreadsMutex);
                        m_ioThreadsConditionVariable.wait(lock, [this]
                        {
                            return m_shouldStop.load(std::memory_order::relaxed) || HasQueuedRequests();
                        });
                    }
                    else
                    {
                        LoadFile(request, true);
                    }
                }
            });
        }
    }

    AsyncResourceLoader::~AsyncResourceLoader()
    {
        {
            std::lock_guard lock(m_ioThreadsMutex);
            m_shouldStop.store(true, std::memory_order::relaxed);
        }
        m_ioThreadsConditionVariable.notify_all();

        for (std::thread& thread: m_ioThreads)
        {
            thread.join();
        }

        // Finalization jobs still reference the loader
        while (m_pendingFinalizations.load(std::memory_order::acquire) > 0)
        {
            std::this_thread::yield();
        }

        // Drop the requests no IO thread picked up
        for (auto& queue: m_queues)
        {
            u32 request = 0;
            while (queue.try_dequeue(request))
            {
                LoadRequest droppedRequest;
                {
                    const auto lock = m_poolLock.AutoLock();
                    droppedRequest = m_loadRequests[request];
                }
                MarkRequestDone(request, droppedRequest);
            }
        }
    }

    void AsyncResourceLoader::RequestLoad(
        const StringHash& _path,
        ResourceEntry* _entry,
        IResourceManager* _resourceManager,
        const u64 _loadFlags)
    {
        KE_ASSERT(!_path.m_string.empty());
        KE_ASSERT(_entry != nullptr);
        KE_ASSERT(_resourceManager != nullptr);

        const u64 priority = _loadFlags & m_desc.m_priorityMask;
        KE_ASSERT(priority == kBlockingPriority
            || (priority >= m_desc.m_firstAsyncPriority && priority <= m_desc.m_lastAsyncPriority));

        u32 requestIdx = 0;
        bool sendRequest = false;
        bool releaseEntry = false;
        {
            const auto lock = m_poolLock.AutoLock();

            const auto it = m_requestMap.Find(_entry);
            if (it == m_requestMap.end())
            {
                sendRequest = true;
                requestIdx = CreateRequest(_path, _entry, _resourceManager, _loadFlags);
                m_requestMap.Emplace({ _entry, requestIdx });
            }
            else
            {
                LoadRequest& existingRequest = m_loadRequests[it->second];
                if ((existingRequest.m_loadFlags & m_desc.m_priorityMask) > priority
                    || (_loadFlags & m_desc.m_forceReloadFlag) != 0)
                {
                    existingRequest.m_canceled = true;
                    sendRequest = true;
                    requestIdx = CreateRequest(_path, _entry, _resourceManager, _loadFlags);

                    // The entry now maps to the new request, so that finishing the canceled one doesn't remove it
                    m_requestMap.Erase(it);
                    m_requestMap.Emplace({ _entry, requestIdx });
                }
                else
                {
                    releaseEntry = true;
                }
            }
        }

        if (releaseEntry)
        {
            _resourceManager->ReleaseResource(_entry);
        }

        if (sendRequest)
        {
            if (priority == kBlockingPriority)
            {
                LoadFile(requestIdx, false);
            }
            else
            {
                if (priority == m_desc.m_loadingScreenPriority)
                {
                    constexpr u64 addValue = 1ull | (1ull << 32);
                    m_loadingScreenProgress.fetch_add(addValue, std::memory_order::relaxed);
                }

                auto& queue = m_queues[priority - m_desc.m_firstAsyncPriority];
                queue.enqueue(requestIdx);
                std::lock_guard lock(m_ioThreadsMutex);
                m_ioThreadsConditionVariable.notify_one();
            }
        }
    }

    bool AsyncResourceLoader::AllowReload(const u64 _loadFlags)
    {
        return (_loadFlags & m_desc.m_forceReloadFlag) != 0;
    }

    eastl::pair<u32, u32> AsyncResourceLoader::GetLoadingScreenProgress() const
    {
        const u64 rawProgress = m_loadingScreenProgress.load(std::memory_order::acquire);
        const u32 remaining = static_cast<u32>(rawProgress);
        const u32 total = static_cast<u32>(rawProgress >> 32);
        return { total - remaining, remaining };
    }

    bool AsyncResourceLoader::ResetLoadingScreenProgress()
    {
        u64 progress = m_loadingScreenProgress.load(std::memory_order::acquire);
        do
        {
            if (static_cast<u32>(progress) != 0)
                return false;
        } while (!m_loadingScreenProgress.compare_exchange_weak(progress, 0ull, std::memory_order::acq_rel));
        return true;
    }

    u32 AsyncResourceLoader::CreateRequest(
        const StringHash& _path,
        ResourceEntry* _entry,
        IResourceManager* _resourceManager,
        const u64 _loadFlags)
    {
        const size_t pathSize = _path.m_string.size();
        char* path = m_allocator.Allocate<char>(pathSize + 1);
        memcpy(path, _path.m_string.data(), pathSize);
        path[pathSize] = '\0';

        const u32 requestIdx = m_loadRequests.Allocate();
        m_loadRequests[requestIdx] = {
            .m_path = path,
            .m_pathSize = pathSize,
            .m_entry = _entry,
            .m_resourceManager = _resourceManager,
            .m_loadFlags = _loadFlags,
            .m_canceled = false,
        };
        return requestIdx;
    }

    bool AsyncResourceLoader::HasQueuedRequests() const
    {
        for (const auto& queue: m_queues)
        {
            if (queue.size_approx() > 0)
                return true;
        }
        return false;
    }

    void AsyncResourceLoader::LoadFile(const u32 _request, const bool _async)
    {
        KE_ZoneScopedFunction("AsyncResourceLoader::LoadFile");

        LoadRequest request {};
        {
            const auto lock = m_poolLock.AutoLock();
            request = m_loadRequests[_request];
        }

        {
            KE_ZoneScopedF("%s", request.m_path);

            if (request.m_canceled)
            {
                MarkRequestDone(_request, request);
                return;
            }

            const eastl::string_view path { request.m_path, request.m_pathSize };

            eastl::span<std::byte> loadedResource {};
            {
                const FileSystem::ReadOnlyFile file = m_vfs->OpenReadOnlyFile(path);

                if (file.IsValid())
                {
                    loadedResource = request.m_resourceManager->LoadResource(request.m_entry, file);
                }
            }

            if (loadedResource.empty())
            {
                request.m_resourceManager->ReportFailedLoad(request.m_entry, path);
                MarkRequestDone(_request, request);
                return;
            }

            if (!_async)
            {
                request.m_resourceManager->FinalizeResourceLoading(
                    request.m_entry,
                    loadedResource,
                    path,
                    request.m_loadFlags & ~m_desc.m_forceReloadFlag);

                MarkRequestDone(_request, request);
            }
            else
            {
                m_pendingFinalizations.fetch_add(1, std::memory_order::relaxed);
                m_fibersManager->InitAndBatchJobsNoCounter({
                    .m_function = [this, request, path, loadedResource, _request](u16)
                    {
                        KE_ZoneScopedFunction("AsyncResourceLoader::LoadFile (job)")

                        request.m_resourceManager->FinalizeResourceLoading(
                                request.m_entry,
                                loadedResource,
                                path,
                                request.m_loadFlags & ~m_desc.m_forceReloadFlag);

                        {
                            KE_ZoneScopedF("%s", request.m_path);

                            MarkRequestDone(_request, request);
                        }

                        // Last access to the loader, as it may be destroyed right after
                        m_pendingFinalizations.fetch_sub(1, std::memory_order::release);
                    },
                });
            }
        }
    }

    void AsyncResourceLoader::MarkRequestDone(
        const u32 _requestId,
        const LoadRequest& _request)
    {
        KE_ZoneScopedFunction("AsyncResourceLoader::MarkRequestDone");

        {
            const auto lock = m_poolLock.AutoLock();
            const auto it = m_requestMap.Find(_request.m_entry);
            if (it != m_requestMap.end() && it->second == _requestId)
                m_requestMap.Erase(it);

            m_allocator.deallocate(_request.m_path, _request.m_pathSize + 1);
            m_loadRequests.Free(_requestId);

            if ((_request.m_loadFlags & m_desc.m_priorityMask) == m_desc.m_loadingScreenPriority)
            {
                m_loadingScreenProgress.fetch_sub(1ull, std::memory_order::release);
            }
        }

        _request.m_resourceManager->ReleaseResource(_request.m_entry);
    }
}
