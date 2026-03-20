/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#include "KryneEngine/Core/Platform/FileSystem.hpp"

#include <filesystem>
#include <CoreServices/CoreServices.h>
#include <EASTL/vector_map.h>

#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine::Platform
{
    class DarwinDirectoryMonitor
    {
    public:
        explicit DarwinDirectoryMonitor(const AllocatorInstance _allocator)
            : m_inodeToPathMap(_allocator)
        {}

        static void OnFSEvent(
            ConstFSEventStreamRef,
            void* _clientCallBackInfo,
            size_t _numEvents,
            void* _events,
            const FSEventStreamEventFlags _eventFlags[],
            const FSEventStreamEventId _eventIds[])
        {
            auto* monitor = static_cast<DarwinDirectoryMonitor*>(_clientCallBackInfo);
            auto events = static_cast<CFArrayRef>(_events);

            for (size_t i = 0; i < _numEvents; ++i)
            {
                auto event = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(events, static_cast<CFIndex>(i)));

                auto rawFilePath = static_cast<CFStringRef>(CFDictionaryGetValue(event, kFSEventStreamEventExtendedDataPathKey));
                const eastl::string_view filePath {
                    CFStringGetCStringPtr(rawFilePath, kCFStringEncodingUTF8)
                };
                const FSEventStreamEventFlags flags = _eventFlags[i];

                if (flags & kFSEventStreamEventFlagItemIsFile)
                {
                    if (flags & kFSEventStreamEventFlagItemCreated)
                    {
                        if (monitor->m_fileCreatedCallback)
                        {
                            monitor->m_fileCreatedCallback(filePath);
                        }
                    }
                    else if (flags & kFSEventStreamEventFlagItemModified)
                    {
                        if (monitor->m_fileModifiedCallback)
                        {
                            monitor->m_fileModifiedCallback(filePath);
                        }
                    }
                    else if (flags & kFSEventStreamEventFlagItemRenamed)
                    {
                        if (monitor->m_fileRenamedCallback)
                        {
                            // Special case: renames are separated into two events with respectively the new and old
                            // paths. However, the ordering of event means they can be non-consecutive and may be
                            // reordered around (the old name event can happen before the new one, and vice versa).

                            const auto inodeRef = static_cast<CFNumberRef>(CFDictionaryGetValue(event, kFSEventStreamEventExtendedFileIDKey));
                            u64 inode;
                            CFNumberGetValue(inodeRef, kCFNumberLongLongType, &inode);

                            const auto it = monitor->m_inodeToPathMap.find(inode);
                            if (it != monitor->m_inodeToPathMap.end())
                            {
                                if (std::filesystem::exists(filePath.data()))
                                {
                                    monitor->m_fileRenamedCallback(it->second.m_path, filePath);
                                }
                                else if (std::filesystem::exists(it->second.m_path.c_str()))
                                {
                                    monitor->m_fileRenamedCallback(filePath, it->second.m_path);
                                }
                                else
                                {
                                    if (_eventIds[i] > it->second.m_eventId)
                                        monitor->m_fileRenamedCallback(it->second.m_path, filePath);
                                    else
                                        monitor->m_fileRenamedCallback(filePath, it->second.m_path);
                                }
                                monitor->m_inodeToPathMap.erase(it);
                            }
                            else
                            {
                                monitor->m_inodeToPathMap.emplace(inode, RenameEvent {
                                    .m_eventId = _eventIds[i],
                                    .m_path { filePath, monitor->m_inodeToPathMap.get_allocator() },
                                });
                            }
                        }
                    }
                    else if (flags & kFSEventStreamEventFlagItemRemoved)
                    {
                        if (monitor->m_fileDeletedCallback)
                        {
                            monitor->m_fileDeletedCallback(filePath);
                        }
                    }
                }
            }
        }

        FSEventStreamRef m_stream = nullptr;
        dispatch_queue_t m_queue = nullptr;

        eastl::function<void(eastl::string_view)> m_fileCreatedCallback {};
        eastl::function<void(eastl::string_view)> m_fileModifiedCallback {};
        eastl::function<void(eastl::string_view, eastl::string_view)> m_fileRenamedCallback {};
        eastl::function<void(eastl::string_view)> m_fileDeletedCallback {};

        struct RenameEvent
        {
            FSEventStreamEventId m_eventId;
            eastl::string m_path;
        };
        eastl::vector_map<u64, RenameEvent> m_inodeToPathMap;
    };

    DirectoryMonitorHandle CreateDirectoryMonitor(const DirectoryMonitorCreateInfo& _info, AllocatorInstance _allocator)
    {
        CFArrayRef directories;
        {
            DynamicArray<CFStringRef> rawDirectories(_allocator, _info.m_directories.size());
            for (auto i = 0u; i < _info.m_directories.size(); ++i)
                rawDirectories[i] = CFStringCreateWithCString(kCFAllocatorDefault, _info.m_directories[i].data(), kCFStringEncodingUTF8);
            directories = CFArrayCreate(
                kCFAllocatorDefault,
                reinterpret_cast<const void**>(rawDirectories.Data()),
                static_cast<CFIndex>(rawDirectories.Size()),
                nullptr);
        }

        auto* monitor = _allocator.New<DarwinDirectoryMonitor>(_allocator);
        monitor->m_fileCreatedCallback = _info.m_fileCreatedCallback;
        monitor->m_fileModifiedCallback = _info.m_fileModifiedCallback;
        monitor->m_fileRenamedCallback = _info.m_fileRenamedCallback;
        monitor->m_fileDeletedCallback = _info.m_fileDeletedCallback;

        FSEventStreamContext context = {
            .info = monitor,
        };

        constexpr u32 flags = kFSEventStreamCreateFlagFileEvents
            | kFSEventStreamCreateFlagUseCFTypes
            | kFSEventStreamCreateFlagUseExtendedData;
        monitor->m_stream = FSEventStreamCreate(
            kCFAllocatorDefault,
            DarwinDirectoryMonitor::OnFSEvent,
            &context,
            directories,
            kFSEventStreamEventIdSinceNow,
            static_cast<CFTimeInterval>(0),
            flags);
        if (monitor->m_stream == nullptr)
        {
            _allocator.Delete(monitor);
            return { OpaqueHandle(OpaqueHandle::Error::Unknown) };
        }

        monitor->m_queue = dispatch_queue_create(_info.m_threadName.data(), DISPATCH_QUEUE_SERIAL);
        if (monitor->m_queue == nullptr)
        {
            FSEventStreamRelease(monitor->m_stream);
            _allocator.Delete(monitor);
            return { OpaqueHandle(OpaqueHandle::Error::Unknown) };
        }

        FSEventStreamSetDispatchQueue(monitor->m_stream, monitor->m_queue);
        if (!FSEventStreamStart(monitor->m_stream))
        {
            dispatch_release(monitor->m_queue);
            FSEventStreamInvalidate(monitor->m_stream);
            FSEventStreamRelease(monitor->m_stream);
            _allocator.Delete(monitor);
            return { OpaqueHandle(OpaqueHandle::Error::Unknown) };
        }

        return { OpaqueHandle { monitor } };
    }

    void DestroyDirectoryMonitor(const DirectoryMonitorHandle _handle, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_handle.IsValid() && _handle.m_handle != nullptr);

        auto* monitor = static_cast<DarwinDirectoryMonitor*>(_handle.m_handle);

        FSEventStreamStop(monitor->m_stream);
        FSEventStreamInvalidate(monitor->m_stream);
        FSEventStreamRelease(monitor->m_stream);

        dispatch_release(monitor->m_queue);

        _allocator.Delete(monitor);
    }

    std::filesystem::path GetDefaultConfigDirectory(
        const eastl::string_view _appName,
        bool _systemConfig)
    {
        const char* home = _systemConfig ? "" : getenv("HOME");

        if (home == nullptr)
            return  std::filesystem::path("/tmp") / _appName.data();

        return std::filesystem::path(home) / "Library/Application Support" / _appName.data();
    }
}
