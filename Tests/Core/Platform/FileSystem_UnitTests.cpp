/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2026.
 */


#include <KryneEngine/Core/Platform/FileSystem.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

#include "Utils/AssertUtils.hpp"


namespace KryneEngine::Tests
{
    template<size_t N>
    void MakeRoot(char (&_root)[N], const char* name = "test")
    {
        const auto timePoint = std::chrono::high_resolution_clock::now();
        snprintf(
            _root,
            N,
            "%s_%llx",
            name,
            timePoint.time_since_epoch().count());
    }

    void MakeFile(const char* _filename, const size_t _size, const u64 _salt)
    {
        KE_ASSERT(_size > 0 && _size % 8 == 0);
        std::ofstream file(_filename, std::ios::binary | std::ios::out);
        KE_ASSERT(file);
        for (size_t i = 0; i < _size; i += 8)
        {
            u64 value = _salt ^ (i >> 3);
            file.write(reinterpret_cast<const char*>(&value), sizeof(u64));
        }
        file.close();
    }

    struct Event
    {
        enum class Type
        {
            Created,
            Modified,
            Renamed,
            Deleted,
        };

        Type m_type;
        std::filesystem::path m_path;
        std::filesystem::path m_oldPath;
    };

    class Monitor
    {
    public:
        void OnFileCreated(const eastl::string_view _path)
        {
            m_events.emplace_back(Event{ Event::Type::Created, _path.data() });
        }

        void OnFileModified(const eastl::string_view _path)
        {
            m_events.emplace_back(Event{ Event::Type::Modified, _path.data() });
        }

        void OnFileRenamed(const eastl::string_view _oldPath, const eastl::string_view _newPath)
        {
            m_events.emplace_back(Event{ Event::Type::Renamed, _newPath.data(), _oldPath.data() });
        }

        void OnFileDeleted(const eastl::string_view _path)
        {
            m_events.emplace_back(Event{ Event::Type::Deleted, _path.data() });
        }

        eastl::vector<Event> m_events;
    };

    void WaitForMonitor(Monitor& _monitor, const size_t _expectedEventCount, const u32 _timeoutMs)
    {
        constexpr u32 milliseconds = 10;
        const u32 count = _timeoutMs / milliseconds;
        for (u32 i = 0; i < count; i++)
        {
            for (u32 i = 0; i < _monitor.m_events.size() - 1;)
            {
                if (_monitor.m_events[i].m_type == _monitor.m_events[i + 1].m_type
                    && _monitor.m_events[i].m_path == _monitor.m_events[i + 1].m_path)
                {
                    _monitor.m_events.erase(_monitor.m_events.begin() + i);
                    continue;
                }
                ++i;
            }

            if (_monitor.m_events.size() >= _expectedEventCount)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }

    TEST(DirectoryMonitor, Init)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        AllocatorInstance allocator {};

        eastl::string_view path = "test";
        std::filesystem::create_directory(path.data());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Platform::DirectoryMonitorHandle handle = Platform::CreateDirectoryMonitor(
            {
                .m_directories = { &path, 1 }
            }
            , allocator);

        EXPECT_TRUE(handle.IsValid());
        EXPECT_NE(handle.m_handle, nullptr);

        Platform::DestroyDirectoryMonitor(handle, allocator);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        std::filesystem::remove_all(path.data());
        catcher.ExpectNoMessage();
    }

    TEST(DirectoryMonitor, Created)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        AllocatorInstance allocator {};
        Monitor monitor {};

        char rootBuffer[256];
        MakeRoot(rootBuffer, "testCreated");
        eastl::string_view rootSv = rootBuffer;
        const std::filesystem::path root = rootBuffer;
        std::filesystem::create_directory(root);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Platform::DirectoryMonitorHandle handle = Platform::CreateDirectoryMonitor(
            {
                .m_directories = { &rootSv, 1 },
                .m_fileCreatedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileCreated(_sv); },
                .m_fileModifiedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileModified(_sv); },
                .m_fileRenamedCallback = [&monitor](const eastl::string_view _oldSv, const eastl::string_view _newSv) { monitor.OnFileRenamed(_oldSv, _newSv); },
                .m_fileDeletedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileDeleted(_sv); },
            }
            , allocator);

        EXPECT_TRUE(handle.IsValid());
        EXPECT_NE(handle.m_handle, nullptr);

        const std::filesystem::path file = root / "file.txt";
        MakeFile(file.c_str(), 1024, 0);

        WaitForMonitor(monitor, 1, 1'000);

#if defined(__APPLE__)
        for (auto it = monitor.m_events.begin(); it != monitor.m_events.end();)
        {
            if (it->m_path.filename() == ".DS_Store")
                monitor.m_events.erase(it);
            else
                ++it;
        }
#endif

        EXPECT_EQ(monitor.m_events.size(), 1);
        if (monitor.m_events.size() >= 1)
        {
            EXPECT_EQ(monitor.m_events[0].m_type, Event::Type::Created);
            EXPECT_EQ(monitor.m_events[0].m_path, std::filesystem::canonical(file));
        }

        Platform::DestroyDirectoryMonitor(handle, allocator);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        std::filesystem::remove_all(root);
        catcher.ExpectNoMessage();
    }

    TEST(DirectoryMonitor, Deleted)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        AllocatorInstance allocator {};
        Monitor monitor {};

        char rootBuffer[256];
        MakeRoot(rootBuffer, "testDeleted");
        eastl::string_view rootSv = rootBuffer;
        const std::filesystem::path root = rootBuffer;
        std::filesystem::create_directory(root);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Platform::DirectoryMonitorHandle handle = Platform::CreateDirectoryMonitor(
            {
                .m_directories = { &rootSv, 1 },
                .m_fileCreatedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileCreated(_sv); },
                .m_fileModifiedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileModified(_sv); },
                .m_fileRenamedCallback = [&monitor](const eastl::string_view _oldSv, const eastl::string_view _newSv) { monitor.OnFileRenamed(_oldSv, _newSv); },
                .m_fileDeletedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileDeleted(_sv); },
            }
            , allocator);

        EXPECT_TRUE(handle.IsValid());
        EXPECT_NE(handle.m_handle, nullptr);

        const std::filesystem::path file = std::filesystem::canonical(root) / "file.txt";
        MakeFile(file.c_str(), 1024, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        std::filesystem::remove(file);

#if defined(__linux__)
        constexpr size_t expectedCount = 3;
#else
        constexpr size_t expectedCount = 2;
#endif

        WaitForMonitor(monitor, expectedCount, 1'000);

#if defined(__APPLE__)
        for (auto it = monitor.m_events.begin(); it != monitor.m_events.end();)
        {
            if (it->m_path.filename() == ".DS_Store")
                monitor.m_events.erase(it);
            else
                ++it;
        }
#endif

        EXPECT_EQ(monitor.m_events.size(), expectedCount);
        if (monitor.m_events.size() >= expectedCount)
        {
            EXPECT_EQ(monitor.m_events[0].m_type, Event::Type::Created);
            EXPECT_EQ(monitor.m_events[0].m_path, file);

#if defined(__linux__)
            EXPECT_EQ(monitor.m_events[1].m_type, Event::Type::Modified);
            EXPECT_EQ(monitor.m_events[1].m_path, file);

            EXPECT_EQ(monitor.m_events[2].m_type, Event::Type::Deleted);
            EXPECT_EQ(monitor.m_events[2].m_path, file);
#else
            EXPECT_EQ(monitor.m_events[1].m_type, Event::Type::Deleted);
            EXPECT_EQ(monitor.m_events[1].m_path, file);
#endif
        }

        Platform::DestroyDirectoryMonitor(handle, allocator);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        std::filesystem::remove_all(root);
        catcher.ExpectNoMessage();
    }

    TEST(DirectoryMonitor, Modified)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        AllocatorInstance allocator {};
        Monitor monitor {};

        char rootBuffer[256];
        MakeRoot(rootBuffer, "testModified");
        eastl::string_view rootSv = rootBuffer;
        const std::filesystem::path root = rootBuffer;
        std::filesystem::create_directory(root);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const Platform::DirectoryMonitorHandle handle = Platform::CreateDirectoryMonitor(
            {
                .m_directories = { &rootSv, 1 },
                .m_fileCreatedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileCreated(_sv); },
                .m_fileModifiedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileModified(_sv); },
                .m_fileRenamedCallback = [&monitor](const eastl::string_view _oldSv, const eastl::string_view _newSv) { monitor.OnFileRenamed(_oldSv, _newSv); },
                .m_fileDeletedCallback = [&monitor](const eastl::string_view _sv) { monitor.OnFileDeleted(_sv); },
            }
            , allocator);

        EXPECT_TRUE(handle.IsValid());
        EXPECT_NE(handle.m_handle, nullptr);

        const std::filesystem::path file = std::filesystem::canonical(root) / "file.txt";
        MakeFile(file.c_str(), 1024, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        MakeFile(file.c_str(), 2048, 0x1234);

        WaitForMonitor(monitor, 2, 1'000);

#if defined(__APPLE__)
        for (auto it = monitor.m_events.begin(); it != monitor.m_events.end();)
        {
            if (it->m_path.filename() == ".DS_Store")
                monitor.m_events.erase(it);
            else
                ++it;
        }
#endif

        EXPECT_EQ(monitor.m_events.size(), 2);
        if (monitor.m_events.size() >= 2)
        {
            EXPECT_EQ(monitor.m_events[0].m_type, Event::Type::Created);
            EXPECT_EQ(monitor.m_events[0].m_path, file);

            EXPECT_EQ(monitor.m_events[1].m_type, Event::Type::Modified);
            EXPECT_EQ(monitor.m_events[1].m_path, file);
        }

        Platform::DestroyDirectoryMonitor(handle, allocator);

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        std::filesystem::remove_all(root);
        catcher.ExpectNoMessage();
    }
}