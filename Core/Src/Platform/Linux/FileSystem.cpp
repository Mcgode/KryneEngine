/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include "KryneEngine/Core/Platform/FileSystem.hpp"

#include <EASTL/fixed_vector.h>
#include <EASTL/vector_map.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Common/Types.hpp"


namespace KryneEngine::Platform
{
    class LinuxDirectoryMonitor
    {
    public:
        explicit LinuxDirectoryMonitor(const AllocatorInstance _allocator)
            : m_rootDirectories(_allocator)
            , m_watchedDirs(_allocator)
        {}

        ~LinuxDirectoryMonitor()
        {
            m_stopThread = true;
            if (m_watchThread.joinable())
                m_watchThread.join();
        }

        struct WatchedDir
        {
            std::filesystem::path m_path {};
            s32 m_rootWd {};
        };

        void RunThread()
        {
            m_watchThread = std::thread([this]()
            {
                std::byte buffer[1024 * (sizeof(inotify_event) + NAME_MAX + 1)];
                fd_set watchSet;

                FD_ZERO(&watchSet);
                FD_SET(m_instance, &watchSet);

                timeval timeout { 0, 100'000 };

                struct Pending
                {
                    std::filesystem::path m_path {};
                    u32 m_cookie {};
                    s32 m_mask {};
                };

                eastl::fixed_vector<Pending, 1024> pendingEvents;
                std::filesystem::path path {};

                while (!m_stopThread)
                {
                    const size_t length = read(m_instance, buffer, sizeof(buffer));

                    if (length > 0)
                    {
                        std::byte* ptr = buffer;

                        while (ptr < buffer + length)
                        {
                            const auto* event = reinterpret_cast<const inotify_event*>(ptr);
                            ptr += sizeof(inotify_event) + event->len;

                            if (event->wd == -1 || event->mask & IN_Q_OVERFLOW)
                            {
                                KE_ERROR("Overflow");
                                return;
                            }

                            if (event->len == 0)
                                continue;

                            WatchedDir& parent = m_watchedDirs[event->wd];
                            path = (parent.m_path / event->name).lexically_normal();
                            std::filesystem::path& root = m_rootDirectories[parent.m_rootWd];

                            if (event->mask & IN_ISDIR)
                            {
                                if (event->mask & (IN_CREATE | IN_MOVED_TO))
                                {
                                    const s32 wd = inotify_add_watch(m_instance, path.c_str(), m_watchFlags);
                                    m_watchedDirs.emplace(wd, WatchedDir { path, parent.m_rootWd });
                                }
                                else if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                                {
                                    const auto it = eastl::find(
                                        m_watchedDirs.begin(),
                                        m_watchedDirs.end(),
                                        path,
                                        [](const auto& pair, const std::filesystem::path& _path)
                                        {
                                            return pair.second.m_path == _path;
                                        });

                                    if (it != m_watchedDirs.end())
                                    {
                                        inotify_rm_watch(m_instance, it->first);
                                        m_watchedDirs.erase(it);
                                    }
                                }
                            }
                            else
                            {
                                if (event->mask & IN_CREATE && m_fileCreatedCallback != nullptr)
                                {
                                    m_fileCreatedCallback((root / path).c_str());
                                }
                                if (event->mask & IN_MODIFY && m_fileModifiedCallback != nullptr)
                                {
                                    m_fileModifiedCallback((root / path).c_str());
                                }
                                if (event->mask & IN_DELETE && m_fileDeletedCallback != nullptr)
                                {
                                    m_fileDeletedCallback((root / path).c_str());
                                }
                                if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO) && m_fileRenamedCallback != nullptr)
                                {
                                    auto* it = eastl::find(
                                        pendingEvents.begin(),
                                        pendingEvents.end(),
                                        event->cookie,
                                        [](const Pending& _pending, const s32 _cookie)
                                        {
                                            return _pending.m_cookie == _cookie;
                                        });

                                    if (it != pendingEvents.end())
                                    {
                                        if (event->mask & IN_MOVED_FROM)
                                        {
                                            m_fileRenamedCallback(it->m_path.c_str(), (root / path).c_str());
                                        }
                                        else
                                        {
                                            m_fileRenamedCallback((root / path).c_str(), it->m_path.c_str());
                                        }
                                        pendingEvents.erase(it);
                                    }
                                    else
                                    {
                                        pendingEvents.push_back(Pending {
                                            .m_path = root / path,
                                            .m_cookie = event->cookie,
                                            .m_mask = (event->mask & IN_MOVED_TO) ? IN_MOVED_TO : IN_MOVED_FROM,
                                        });
                                    }
                                }
                            }
                        }
                    }

                    for (auto& pending : pendingEvents)
                    {
                        if (pending.m_mask & IN_MOVED_FROM && m_fileDeletedCallback != nullptr)
                        {
                            m_fileDeletedCallback(pending.m_path.c_str());
                        }
                        else if (pending.m_mask & IN_MOVED_TO && m_fileCreatedCallback != nullptr)
                        {
                            m_fileCreatedCallback(pending.m_path.c_str());
                        }
                    }
                    pendingEvents.clear();

                    select(m_instance + 1, &watchSet, nullptr, nullptr, &timeout);
                }
            });
        }

        s32 m_instance {};
        s32 m_watchFlags {};
        eastl::vector_map<s32, std::filesystem::path> m_rootDirectories {};
        eastl::vector_map<s32, WatchedDir> m_watchedDirs {};
        std::thread m_watchThread {};
        bool m_stopThread = false;

        eastl::function<void(eastl::string_view)> m_fileCreatedCallback;
        eastl::function<void(eastl::string_view)> m_fileModifiedCallback;
        eastl::function<void(eastl::string_view, eastl::string_view)> m_fileRenamedCallback;
        eastl::function<void(eastl::string_view)> m_fileDeletedCallback;
    };

    DirectoryMonitorHandle CreateDirectoryMonitor(
        const DirectoryMonitorCreateInfo& _info,
        const AllocatorInstance _allocator)
    {
        auto* monitor = _allocator.New<LinuxDirectoryMonitor>(_allocator);

        monitor->m_instance = inotify_init();

        s32 watchFlags = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
        if (_info.m_fileModifiedCallback != nullptr)
            watchFlags |= IN_MODIFY;
        monitor->m_watchFlags = watchFlags;

        monitor->m_fileCreatedCallback = _info.m_fileCreatedCallback;
        monitor->m_fileModifiedCallback = _info.m_fileModifiedCallback;
        monitor->m_fileRenamedCallback = _info.m_fileRenamedCallback;
        monitor->m_fileDeletedCallback = _info.m_fileDeletedCallback;

        for (const auto& directory : _info.m_directories)
        {
            std::filesystem::path directoryPath = std::filesystem::canonical(directory.data());

            s32 wd = inotify_add_watch(monitor->m_instance, directoryPath.c_str(), watchFlags);
            monitor->m_rootDirectories.emplace(wd, directoryPath);

            monitor->m_watchedDirs.emplace(
                wd,
                LinuxDirectoryMonitor::WatchedDir{
                    std::filesystem::relative(directoryPath, directoryPath),
                    wd
                });

            for (auto& entry: std::filesystem::recursive_directory_iterator(directoryPath))
            {
                if (!entry.is_directory())
                    continue;

                wd = inotify_add_watch(monitor->m_instance, entry.path().c_str(), watchFlags);
                monitor->m_watchedDirs.emplace(
                    wd,
                    LinuxDirectoryMonitor::WatchedDir{
                        std::filesystem::relative(entry.path(), directoryPath),
                        wd
                    });
            }
        }

        monitor->RunThread();

        return { OpaqueHandle { monitor } };
    }

    void DestroyDirectoryMonitor(const DirectoryMonitorHandle _handle, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_handle.IsValid() && _handle.m_handle != nullptr);

        auto* monitor = static_cast<LinuxDirectoryMonitor*>(_handle.m_handle);
        for (auto& [wd, dir] : monitor->m_watchedDirs)
        {
            inotify_rm_watch(monitor->m_instance, wd);
        }
        close(monitor->m_instance);

        _allocator.Delete(monitor);
    }

    std::filesystem::path GetDefaultConfigDirectory(
        const eastl::string_view _appName,
        const bool _systemConfig)
    {
        const std::filesystem::path home = _systemConfig
            ? "/etc"
            : std::filesystem::path(getenv("HOME")) / ".config";

        return home / _appName.data();
    }

    ReadOnlyFileDescriptor OpenReadOnlyFile(const eastl::string_view _path, const AllocatorInstance _allocator)
    {
        const s32 fd = open(_path.data(), O_RDONLY);
        if (fd == -1)
        {
            switch (errno)
            {
            case EACCES:
                return { OpaqueHandle(OpaqueHandle::Error::AccessDenied) };
            case ENOENT:
                return { OpaqueHandle(OpaqueHandle::Error::InvalidPath) };
            case ENAMETOOLONG:
                return { OpaqueHandle(OpaqueHandle::Error::PathTooLong) };
            default:
                return { OpaqueHandle(OpaqueHandle::Error::Unknown) };
            }
        }

        if constexpr (sizeof(void*) > sizeof(s32))
        {
            uintptr_t ptr = fd;
            ptr <<= 1;
            return { OpaqueHandle { reinterpret_cast<void*>(ptr) } };
        }
        else
        {
            return { OpaqueHandle { _allocator.New<s32>(fd) } };
        }
    }

    KE_FORCEINLINE s32 RetrieveFd(const ReadOnlyFileDescriptor _fd)
    {
        if constexpr (sizeof(void*) > sizeof(s32))
        {
            auto ptr = reinterpret_cast<uintptr_t>(_fd.m_handle);
            ptr >>= 1;
            return static_cast<s32>(ptr);
        }
        else
        {
            return *static_cast<s32*>(_fd.m_handle);
        }
    }

    size_t GetFileSize(const ReadOnlyFileDescriptor _fd)
    {
        KE_ASSERT(_fd.IsValid());
        const s32 fd = RetrieveFd(_fd);

        struct stat st {};
        if (fstat(fd, &st) == -1)
        {
            return 0;
        }

        return st.st_size;
    }

    size_t ReadFile(const ReadOnlyFileDescriptor _fd, const size_t _position, const eastl::span<std::byte> _dstBuffer)
    {
        KE_ASSERT(_fd.IsValid());
        const s32 fd = RetrieveFd(_fd);

        const size_t readSize = pread(fd, _dstBuffer.data(), _dstBuffer.size(), static_cast<off_t>(_position));
        return readSize == -1 ? 0 : readSize;
    }

    void CloseReadOnlyFile(const ReadOnlyFileDescriptor _fd, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_fd.IsValid());
        s32 fd;

        if constexpr (sizeof(void*) > sizeof(s32))
        {
            fd = RetrieveFd(_fd);
        }
        else
        {
            auto* ptr = static_cast<s32*>(_fd.m_handle);
            fd = *ptr;
            _allocator.Delete(ptr);
        }

        close(fd);
    }
}