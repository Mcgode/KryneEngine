/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2026.
 */

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <gtest/gtest.h>
#include <KryneEngine/Core/Platform/Cpu.hpp>
#include <KryneEngine/Core/Platform/StdAlloc.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Modules/FileSystem/ReadOnlyFile.hpp>
#include <KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp>
#include <KryneEngine/Modules/Resources/IResourceManager.hpp>
#include <KryneEngine/Modules/Resources/Loaders/AsyncResourceLoader.hpp>
#include <KryneEngine/Modules/Resources/ResourceEntry.hpp>
#include <KryneEngine/Modules/Resources/ResourceTypeId.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::Resources
{
    using namespace KryneEngine::Modules::Resources;
    namespace FileSystem = KryneEngine::Modules::FileSystem;

    namespace
    {
        using namespace std::chrono_literals;

        constexpr auto kTimeout = 10s;

        // Same layout as the example in the `AsyncResourceLoader` documentation, with the loading screen priority in
        // the middle of the async range.
        enum class TestLoadFlags: u64
        {
            Blocking = 0,
            Highest = 1,
            LoadingScreen = 2,
            High = 3,
            Normal = 4,
            Low = 5,
            PriorityMask = 0b111,
            ForceReload = 0b1000,
        };

        constexpr TestLoadFlags operator|(TestLoadFlags _a, TestLoadFlags _b)
        {
            return static_cast<TestLoadFlags>(static_cast<u64>(_a) | static_cast<u64>(_b));
        }

        constexpr AsyncResourceLoader::Desc kDesc {
            .m_priorityMask = static_cast<u64>(TestLoadFlags::PriorityMask),
            .m_forceReloadFlag = static_cast<u64>(TestLoadFlags::ForceReload),
            .m_firstAsyncPriority = static_cast<u64>(TestLoadFlags::Highest),
            .m_lastAsyncPriority = static_cast<u64>(TestLoadFlags::Low),
            .m_loadingScreenPriority = static_cast<u64>(TestLoadFlags::LoadingScreen),
        };

        class CountingAllocator final: public IAllocator
        {
        public:
            CountingAllocator(): IAllocator("CountingAllocator") {}

            void* Allocate(const size_t _size, const size_t _alignment) override
            {
                m_liveAllocations++;
                return StdAlloc::MemAlign(_size, _alignment == 0 ? alignof(std::max_align_t) : _alignment);
            }

            void Free(void* _ptr, size_t) override
            {
                m_liveAllocations--;
                StdAlloc::Free(_ptr);
            }

            std::atomic<s64> m_liveAllocations = 0;
        };

        // See FibersManager_UnitTests.cpp: fiber threads are pinned to cores, so their count can't exceed the usable
        // core count.
        u16 ClampFiberThreadCount(const u16 _desired)
        {
            const u32 usableCores = Platform::GetUsableCpuCoreCount();
            return usableCores > 0
                ? eastl::min<u16>(_desired, static_cast<u16>(usableCores))
                : _desired;
        }

        /// Never instantiated: the test manager publishes no resource, this only gives the entries a type to release.
        struct DummyResource
        {
            static constexpr ResourceTypeId kTypeId = GenerateResourceTypeId("Tests::DummyResource");

            AllocatorInstance m_allocator;
        };

        /**
         * Records every call from the loader. Loads of the blocked entry wait in `LoadResource()` (on the IO thread)
         * until allowed, so tests can control what the IO thread is doing.
         */
        class TestResourceManager final: public IResourceManager
        {
        public:
            struct Event
            {
                ResourceEntry* m_entry;
                std::string m_path;
                std::string m_content;
                std::thread::id m_thread;
            };

            eastl::span<std::byte> LoadResource(ResourceEntry* _entry, const FileSystem::ReadOnlyFile& _file) override
            {
                {
                    std::unique_lock lock(m_mutex);
                    m_loads.push_back({ _entry, {}, {}, std::this_thread::get_id() });
                    if (_entry == m_blockedEntry)
                    {
                        m_waitingLoads++;
                        m_condition.notify_all();
                        m_condition.wait(lock, [this] { return m_blockedEntry == nullptr || m_allowedLoads > 0; });
                        if (m_blockedEntry != nullptr)
                            m_allowedLoads--;
                        m_waitingLoads--;
                    }
                }
                return IResourceManager::LoadResource(_entry, _file);
            }

            void FinalizeResourceLoading(
                ResourceEntry* _entry,
                const eastl::span<std::byte> _loadedResourceData,
                const eastl::string_view _path) override
            {
                const std::string content(reinterpret_cast<const char*>(_loadedResourceData.data()), _loadedResourceData.size());
                GetAllocator().deallocate(_loadedResourceData.data(), _loadedResourceData.size());

                std::lock_guard lock(m_mutex);
                m_finalized.push_back({ _entry, std::string(_path.data(), _path.size()), content, std::this_thread::get_id() });
                m_condition.notify_all();
            }

            void ReportFailedLoad(ResourceEntry* _entry, const eastl::string_view _path) override
            {
                std::lock_guard lock(m_mutex);
                m_failed.push_back({ _entry, std::string(_path.data(), _path.size()), {}, std::this_thread::get_id() });
                m_condition.notify_all();
            }

            [[nodiscard]] AllocatorInstance GetAllocator() const override { return {}; }

            void Block(ResourceEntry* _entry)
            {
                std::lock_guard lock(m_mutex);
                m_blockedEntry = _entry;
                m_allowedLoads = 0;
            }

            void Allow(const u32 _loads)
            {
                std::lock_guard lock(m_mutex);
                m_allowedLoads += _loads;
                m_condition.notify_all();
            }

            void Unblock()
            {
                std::lock_guard lock(m_mutex);
                m_blockedEntry = nullptr;
                m_condition.notify_all();
            }

            bool WaitForBlockedLoad()
            {
                std::unique_lock lock(m_mutex);
                return m_condition.wait_for(lock, kTimeout, [this] { return m_waitingLoads > 0; });
            }

            bool WaitForFinalized(const size_t _count)
            {
                std::unique_lock lock(m_mutex);
                return m_condition.wait_for(lock, kTimeout, [&] { return m_finalized.size() >= _count; });
            }

            bool WaitForFailed(const size_t _count)
            {
                std::unique_lock lock(m_mutex);
                return m_condition.wait_for(lock, kTimeout, [&] { return m_failed.size() >= _count; });
            }

            [[nodiscard]] std::vector<Event> GetLoads() const { std::lock_guard lock(m_mutex); return m_loads; }
            [[nodiscard]] std::vector<Event> GetFinalized() const { std::lock_guard lock(m_mutex); return m_finalized; }
            [[nodiscard]] std::vector<Event> GetFailed() const { std::lock_guard lock(m_mutex); return m_failed; }

            [[nodiscard]] size_t CountLoads(const ResourceEntry* _entry) const
            {
                std::lock_guard lock(m_mutex);
                return std::count_if(m_loads.begin(), m_loads.end(), [&](const Event& _e) { return _e.m_entry == _entry; });
            }

            void ReleaseResource(ResourceEntry* _entry) const override
            {
                _entry->ReleaseResource<DummyResource>();
            }

        private:
            mutable std::mutex m_mutex;
            std::condition_variable m_condition;
            std::vector<Event> m_loads;
            std::vector<Event> m_finalized;
            std::vector<Event> m_failed;

            ResourceEntry* m_blockedEntry = nullptr;
            u32 m_allowedLoads = 0;
            u32 m_waitingLoads = 0;
        };

        bool WaitForProgress(const AsyncResourceLoader& _loader, const eastl::pair<u32, u32> _expected)
        {
            const auto deadline = std::chrono::steady_clock::now() + kTimeout;
            while (_loader.GetLoadingScreenProgress() != _expected)
            {
                if (std::chrono::steady_clock::now() > deadline)
                    return false;
                std::this_thread::sleep_for(1ms);
            }
            return true;
        }

        /// The loader releases its reference after the last call to the manager, so the count is polled.
        bool WaitForRefCount(const ResourceEntry& _entry, const s64 _expected)
        {
            const auto deadline = std::chrono::steady_clock::now() + kTimeout;
            while (_entry.m_refCount.load(std::memory_order::acquire) != _expected)
            {
                if (std::chrono::steady_clock::now() > deadline)
                    return false;
                std::this_thread::sleep_for(1ms);
            }
            return true;
        }

        class AsyncResourceLoaderTest: public ::testing::Test
        {
        protected:
            AsyncResourceLoaderTest()
                : m_fibersManager(ClampFiberThreadCount(2), AllocatorInstance())
                , m_fileSystem(AllocatorInstance())
            {
                m_previousFibersManager = FibersManager::GetInstance();
                FibersManager::SetInstance(&m_fibersManager);

                m_directory = std::filesystem::temp_directory_path() / "KryneEngine_AsyncResourceLoaderTests";
                std::filesystem::create_directories(m_directory);
            }

            ~AsyncResourceLoaderTest() override
            {
                FibersManager::SetInstance(m_previousFibersManager);

                std::error_code error;
                std::filesystem::remove_all(m_directory, error);

                m_catcher.ExpectNoMessage();
            }

            StringHash WriteFile(const char* _name, const char* _content) const
            {
                const std::filesystem::path path = m_directory / _name;
                std::ofstream file(path, std::ios::binary | std::ios::trunc);
                file << _content;
                return StringHash(path.string().c_str());
            }

            StringHash MissingFile(const char* _name) const
            {
                return StringHash((m_directory / _name).string().c_str());
            }

            /**
             * Requests a load the way `RuntimeResourceSystem::LoadResource()` does: with one reference for the
             * caller, which the tests keep, and one for the loader, which it must release once the request is done.
             */
            template <class Flags>
            void RequestLoad(
                AsyncResourceLoader& _loader,
                const StringHash& _path,
                ResourceEntry& _entry,
                const Flags _loadFlags)
            {
                // Only written by the first request of an entry, before the loader knows about it.
                if (_entry.m_typeId == 0)
                    _entry.m_typeId = DummyResource::kTypeId;
                _entry.m_refCount.fetch_add(2, std::memory_order::release);
                _loader.RequestLoad(_path, &_entry, &m_manager, static_cast<u64>(_loadFlags));
            }

            ScopedAssertCatcher m_catcher;
            FibersManager m_fibersManager;
            FibersManager* m_previousFibersManager = nullptr;
            FileSystem::VirtualFileSystem m_fileSystem;
            TestResourceManager m_manager;
            std::filesystem::path m_directory;
        };
    }

    TEST_F(AsyncResourceLoaderTest, AsyncLoad)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash path = WriteFile("async.txt", "async content");
        ResourceEntry entry;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);
        RequestLoad(loader, path, entry, TestLoadFlags::Normal);

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        ASSERT_TRUE(m_manager.WaitForFinalized(1));

        const auto loads = m_manager.GetLoads();
        const auto finalized = m_manager.GetFinalized();
        ASSERT_EQ(loads.size(), 1);
        ASSERT_EQ(finalized.size(), 1);
        EXPECT_EQ(finalized[0].m_entry, &entry);
        EXPECT_EQ(finalized[0].m_path, path.m_string.c_str());
        EXPECT_EQ(finalized[0].m_content, "async content");
        EXPECT_TRUE(m_manager.GetFailed().empty());

        // Read on an IO thread, then finalized in a fiber job
        EXPECT_NE(loads[0].m_thread, std::this_thread::get_id());
        EXPECT_NE(finalized[0].m_thread, std::this_thread::get_id());
        EXPECT_NE(finalized[0].m_thread, loads[0].m_thread);

        // Not a loading screen request
        EXPECT_EQ(loader.GetLoadingScreenProgress(), (eastl::pair<u32, u32>(0, 0)));

        // The loader released its reference, only the caller's is left
        EXPECT_TRUE(WaitForRefCount(entry, 1));
    }

    TEST_F(AsyncResourceLoaderTest, BlockingLoad)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash path = WriteFile("blocking.txt", "blocking content");
        ResourceEntry entry;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);
        RequestLoad(loader, path, entry, AsyncResourceLoader::kBlockingPriority);

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        // Done before returning, on the calling thread
        const auto loads = m_manager.GetLoads();
        const auto finalized = m_manager.GetFinalized();
        ASSERT_EQ(loads.size(), 1);
        ASSERT_EQ(finalized.size(), 1);
        EXPECT_EQ(loads[0].m_thread, std::this_thread::get_id());
        EXPECT_EQ(finalized[0].m_thread, std::this_thread::get_id());
        EXPECT_EQ(finalized[0].m_entry, &entry);
        EXPECT_EQ(finalized[0].m_content, "blocking content");

        // The loader reference is released before returning too
        EXPECT_EQ(entry.m_refCount.load(), 1);

        // The request is done, so a new one loads the file again
        RequestLoad(loader, path, entry, TestLoadFlags::Blocking);
        EXPECT_EQ(m_manager.GetFinalized().size(), 2);
        EXPECT_EQ(entry.m_refCount.load(), 2);
    }

    TEST_F(AsyncResourceLoaderTest, MissingFile)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash asyncPath = MissingFile("missing_async.txt");
        const StringHash blockingPath = MissingFile("missing_blocking.txt");
        ResourceEntry asyncEntry;
        ResourceEntry blockingEntry;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);
        RequestLoad(loader, blockingPath, blockingEntry, TestLoadFlags::Blocking);
        RequestLoad(loader, asyncPath, asyncEntry, TestLoadFlags::LoadingScreen);

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        ASSERT_TRUE(m_manager.WaitForFailed(2));

        const auto failed = m_manager.GetFailed();
        EXPECT_EQ(failed[0].m_entry, &blockingEntry);
        EXPECT_EQ(failed[0].m_path, blockingPath.m_string.c_str());
        EXPECT_EQ(failed[1].m_entry, &asyncEntry);
        EXPECT_EQ(failed[1].m_path, asyncPath.m_string.c_str());
        EXPECT_TRUE(m_manager.GetLoads().empty());
        EXPECT_TRUE(m_manager.GetFinalized().empty());

        // A failed load still counts as done for the loading screen
        EXPECT_TRUE(WaitForProgress(loader, { 1, 0 }));

        // A failed load releases the loader reference as well
        EXPECT_EQ(blockingEntry.m_refCount.load(), 1);
        EXPECT_TRUE(WaitForRefCount(asyncEntry, 1));
    }

    TEST_F(AsyncResourceLoaderTest, PriorityOrder)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash blockerPath = WriteFile("blocker.txt", "blocker");
        const StringHash lowPath = WriteFile("low.txt", "low");
        const StringHash normalPath = WriteFile("normal.txt", "normal");
        const StringHash highPath = WriteFile("high.txt", "high");
        const StringHash loadingScreenPath = WriteFile("loading_screen.txt", "loading screen");
        const StringHash highestPath = WriteFile("highest.txt", "highest");

        ResourceEntry blocker, low, normal, high, loadingScreen, highest;

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);

        // Keep the only IO thread busy, so that the requests below are all queued before any of them is served
        m_manager.Block(&blocker);
        RequestLoad(loader, blockerPath, blocker, TestLoadFlags::Low);
        ASSERT_TRUE(m_manager.WaitForBlockedLoad());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        RequestLoad(loader, lowPath, low, TestLoadFlags::Low);
        RequestLoad(loader, normalPath, normal, TestLoadFlags::Normal);
        RequestLoad(loader, highPath, high, TestLoadFlags::High);
        RequestLoad(loader, loadingScreenPath, loadingScreen, TestLoadFlags::LoadingScreen);
        RequestLoad(loader, highestPath, highest, static_cast<u64>(TestLoadFlags::Highest));
        m_manager.Unblock();

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        ASSERT_TRUE(m_manager.WaitForFinalized(6));

        // Lowest priority value first
        const auto loads = m_manager.GetLoads();
        ASSERT_EQ(loads.size(), 6);
        EXPECT_EQ(loads[0].m_entry, &blocker);
        EXPECT_EQ(loads[1].m_entry, &highest);
        EXPECT_EQ(loads[2].m_entry, &loadingScreen);
        EXPECT_EQ(loads[3].m_entry, &high);
        EXPECT_EQ(loads[4].m_entry, &normal);
        EXPECT_EQ(loads[5].m_entry, &low);
    }

    TEST_F(AsyncResourceLoaderTest, DuplicateRequests)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash blockerPath = WriteFile("blocker.txt", "blocker");
        const StringHash path = WriteFile("duplicate.txt", "duplicate");
        ResourceEntry blocker;
        ResourceEntry entry;

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);

        m_manager.Block(&blocker);
        RequestLoad(loader, blockerPath, blocker, TestLoadFlags::Low);
        ASSERT_TRUE(m_manager.WaitForBlockedLoad());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        RequestLoad(loader, path, entry, TestLoadFlags::Normal);
        // Same priority: ignored
        RequestLoad(loader, path, entry, TestLoadFlags::Normal);
        // Higher priority: replaces the pending request
        RequestLoad(loader, path, entry, TestLoadFlags::High);
        // Lower priority than the pending request: ignored
        RequestLoad(loader, path, entry, TestLoadFlags::Normal);
        // Forced reload: replaces the pending request, even with a lower priority
        RequestLoad(loader, path, entry, TestLoadFlags::Low | TestLoadFlags::ForceReload);

        m_manager.Unblock();

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        // Canceled requests are served first (higher priority) and skipped, so once the entry is finalized, all
        // of its requests are done.
        ASSERT_TRUE(m_manager.WaitForFinalized(2));
        EXPECT_EQ(m_manager.CountLoads(&entry), 1);

        // Nothing is pending anymore, so a new request loads the file again
        RequestLoad(loader, path, entry, TestLoadFlags::Normal);
        ASSERT_TRUE(m_manager.WaitForFinalized(3));
        EXPECT_EQ(m_manager.CountLoads(&entry), 2);

        // Ignored, canceled and served requests all released their loader reference: one caller reference is left
        // per request.
        EXPECT_TRUE(WaitForRefCount(entry, 6));
        EXPECT_TRUE(WaitForRefCount(blocker, 1));
    }

    // Regression test: replacing a pending request used to leave the entry mapped to the canceled request. Finishing
    // the canceled request then removed the entry from the map while the new request was still pending, letting
    // duplicated requests through.
    TEST_F(AsyncResourceLoaderTest, ReplacedRequestStaysTracked)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash path = WriteFile("replaced.txt", "replaced");
        const StringHash sentinelPath = WriteFile("sentinel.txt", "sentinel");
        ResourceEntry entry;
        ResourceEntry sentinel;

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);
        m_manager.Block(&entry);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // The first request is being loaded when the forced reload replaces it
        RequestLoad(loader, path, entry, TestLoadFlags::LoadingScreen);
        ASSERT_TRUE(m_manager.WaitForBlockedLoad());
        RequestLoad(loader, path, entry, TestLoadFlags::LoadingScreen | TestLoadFlags::ForceReload);
        EXPECT_EQ(loader.GetLoadingScreenProgress(), (eastl::pair<u32, u32>(0, 2)));

        // Let the first request finish, while the forced reload waits on the IO thread
        m_manager.Allow(1);
        ASSERT_TRUE(WaitForProgress(loader, { 1, 1 }));

        // The forced reload is still pending with a higher priority, so this request must be ignored
        RequestLoad(loader, path, entry, TestLoadFlags::Normal);

        m_manager.Unblock();
        ASSERT_TRUE(WaitForProgress(loader, { 2, 0 }));

        // Requests are served by priority on the single IO thread, so a duplicated request would be loaded before
        // the sentinel.
        RequestLoad(loader, sentinelPath, sentinel, TestLoadFlags::Low);
        ASSERT_TRUE(m_manager.WaitForFinalized(3));

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        EXPECT_EQ(m_manager.CountLoads(&entry), 2);
        EXPECT_EQ(m_manager.GetFinalized().size(), 3);
        EXPECT_TRUE(WaitForRefCount(entry, 3));
        EXPECT_TRUE(WaitForRefCount(sentinel, 1));
    }

    TEST_F(AsyncResourceLoaderTest, LoadingScreenProgress)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const StringHash paths[] = {
            WriteFile("first.txt", "first"),
            WriteFile("second.txt", "second"),
            WriteFile("third.txt", "third"),
        };
        const StringHash otherPath = WriteFile("other.txt", "other");
        ResourceEntry entries[3];
        ResourceEntry other;

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 1, kDesc);
        EXPECT_EQ(loader.GetLoadingScreenProgress(), (eastl::pair<u32, u32>(0, 0)));
        EXPECT_TRUE(loader.ResetLoadingScreenProgress());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        m_manager.Block(&entries[0]);
        for (u32 i = 0; i < 3; ++i)
            RequestLoad(loader, paths[i], entries[i], TestLoadFlags::LoadingScreen);
        RequestLoad(loader, otherPath, other, TestLoadFlags::Highest);
        ASSERT_TRUE(m_manager.WaitForBlockedLoad());

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        // Only loading screen requests are tracked
        EXPECT_EQ(loader.GetLoadingScreenProgress(), (eastl::pair<u32, u32>(0, 3)));
        EXPECT_FALSE(loader.ResetLoadingScreenProgress()) << "Can't reset while loads are remaining";

        m_manager.Unblock();
        ASSERT_TRUE(WaitForProgress(loader, { 3, 0 }));
        ASSERT_TRUE(m_manager.WaitForFinalized(4));

        EXPECT_TRUE(loader.ResetLoadingScreenProgress());
        EXPECT_EQ(loader.GetLoadingScreenProgress(), (eastl::pair<u32, u32>(0, 0)));
    }

    TEST_F(AsyncResourceLoaderTest, MultipleIoThreads)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        constexpr u32 kFileCount = 64;
        std::vector<StringHash> paths;
        std::vector<std::string> contents;
        for (u32 i = 0; i < kFileCount; ++i)
        {
            contents.push_back("content " + std::to_string(i));
            paths.push_back(WriteFile(("file_" + std::to_string(i) + ".txt").c_str(), contents.back().c_str()));
        }
        std::vector<ResourceEntry> entries(kFileCount);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        AsyncResourceLoader loader(AllocatorInstance(), &m_fileSystem, 4, kDesc);
        for (u32 i = 0; i < kFileCount; ++i)
            RequestLoad(loader, paths[i], entries[i], TestLoadFlags::LoadingScreen);

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        ASSERT_TRUE(WaitForProgress(loader, { kFileCount, 0 }));
        ASSERT_TRUE(m_manager.WaitForFinalized(kFileCount));

        const auto finalized = m_manager.GetFinalized();
        ASSERT_EQ(finalized.size(), kFileCount);
        for (const auto& event: finalized)
        {
            const size_t index = event.m_entry - entries.data();
            ASSERT_LT(index, kFileCount);
            EXPECT_EQ(event.m_content, contents[index]);
        }
        EXPECT_TRUE(m_manager.GetFailed().empty());
    }

    TEST_F(AsyncResourceLoaderTest, DestroyWithPendingRequests)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        static CountingAllocator allocator;
        const s64 baseLiveAllocations = allocator.m_liveAllocations;

        constexpr u32 kFileCount = 32;
        const StringHash blockerPath = WriteFile("blocker.txt", "blocker");
        std::vector<StringHash> paths;
        for (u32 i = 0; i < kFileCount; ++i)
            paths.push_back(WriteFile(("pending_" + std::to_string(i) + ".txt").c_str(), "pending"));
        ResourceEntry blocker;
        std::vector<ResourceEntry> entries(kFileCount);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            AsyncResourceLoader loader(AllocatorInstance(&allocator), &m_fileSystem, 1, kDesc);

            m_manager.Block(&blocker);
            RequestLoad(loader, blockerPath, blocker, TestLoadFlags::Highest);
            ASSERT_TRUE(m_manager.WaitForBlockedLoad());

            for (u32 i = 0; i < kFileCount; ++i)
                RequestLoad(loader, paths[i], entries[i], TestLoadFlags::Low);

            // Some of the queued requests may be served before the loader stops, the others are dropped
            m_manager.Unblock();
        }

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        // All finalization jobs were waited for, and all requests were released
        const size_t finalizedCount = m_manager.GetFinalized().size();
        EXPECT_GE(finalizedCount, 1);
        EXPECT_LE(finalizedCount, kFileCount + 1);
        EXPECT_EQ(m_manager.GetLoads().size(), finalizedCount);
        EXPECT_TRUE(m_manager.GetFailed().empty());
        EXPECT_EQ(allocator.m_liveAllocations, baseLiveAllocations);

        // Served or dropped, every request released its loader reference
        EXPECT_EQ(blocker.m_refCount.load(), 1);
        for (const ResourceEntry& entry: entries)
            EXPECT_EQ(entry.m_refCount.load(), 1);
    }
}
