/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#include "KryneEngine/Core/Platform/FileSystem.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <thread>

#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"


namespace KryneEngine::Platform
{
    class WindowsDirectoryMonitor
    {
    public:
        explicit WindowsDirectoryMonitor(const AllocatorInstance _allocator)
            : m_dirHandles(_allocator)
            , m_buffer(_allocator)
            , m_events(_allocator)
        {}

        ~WindowsDirectoryMonitor()
        {
            m_shouldStop = true;
            SetEvent(m_events[0]);
            m_thread.join();
        }

        void Start()
        {
            m_thread = std::thread([this]()
            {
                const auto readDirectoryChangesW = [this](const size_t _index)
                {
                    OVERLAPPED overlapped {};
                    overlapped.hEvent = m_events[_index];

                    const BOOL success = ReadDirectoryChangesW(
                        m_dirHandles[_index],
                        m_buffer.Data() + _index * kBufferSize,
                        kBufferSize,
                        true,
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                        nullptr,
                        &overlapped,
                        nullptr);

                    KE_ASSERT(success);
                };

                for (auto i = 0; i < m_dirHandles.Size(); ++i)
                {
                    readDirectoryChangesW(i);
                }

                while (!m_shouldStop)
                {
                    const DWORD result = WaitForMultipleObjects(
                        m_events.Size(),
                        m_events.Data(),
                        false,
                        INFINITE);

                    // To stop the wait, we manually signal the first event, so we need to exit the loop early.
                    if (m_shouldStop)
                        break;

                    KE_ASSERT(result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + m_events.Size());

                    const size_t firstIndex = result - WAIT_OBJECT_0;

                    for (auto index = firstIndex; index < m_dirHandles.Size(); ++index)
                    {
                        if (WaitForSingleObject(m_events[index], 0) != WAIT_OBJECT_0)
                            continue;

                        size_t offset = index * kBufferSize;

                        FILE_NOTIFY_INFORMATION* info;
                        do
                        {
                            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(m_buffer.Data() + offset);
                            offset += info->NextEntryOffset;

                            eastl::wstring_view fileNameW(info->FileName, info->FileNameLength / sizeof(wchar_t));

                            eastl::string fileName { m_events.GetAllocator() };
                            fileName.resize(WideCharToMultiByte(
                                CP_UTF8,
                                0,
                                fileNameW.data(),
                                static_cast<int>(fileNameW.size()),
                                nullptr,
                                0,
                                nullptr,
                                nullptr));
                            WideCharToMultiByte(
                                CP_UTF8,
                                0,
                                fileNameW.data(),
                                static_cast<int>(fileNameW.size()),
                                fileName.data(),
                                static_cast<int>(fileName.size()),
                                nullptr,
                                nullptr);

                            switch (info->Action)
                            {
                            case FILE_ACTION_ADDED:
                                if (m_fileCreatedCallback)
                                    m_fileCreatedCallback(fileName);
                                break;
                            case FILE_ACTION_REMOVED:
                                if (m_fileDeletedCallback)
                                    m_fileDeletedCallback(fileName);
                                break;
                            case FILE_ACTION_MODIFIED:
                                if (m_fileModifiedCallback)
                                    m_fileModifiedCallback(fileName);
                                break;
                            case FILE_ACTION_RENAMED_OLD_NAME:
                                if (!m_oldFileNames[index].empty())
                                {
                                    if (m_fileDeletedCallback)
                                        m_fileDeletedCallback(m_oldFileNames[index]);
                                }
                                m_oldFileNames[index] = fileName;
                                break;
                            case FILE_ACTION_RENAMED_NEW_NAME:
                                if (m_oldFileNames[index].empty())
                                {
                                    if (m_fileCreatedCallback)
                                        m_fileCreatedCallback(fileName);
                                }
                                else
                                {
                                    if (m_fileRenamedCallback)
                                        m_fileRenamedCallback(m_oldFileNames[index], fileName);
                                    m_oldFileNames[index] = "";
                                }
                                break;
                            default:
                                break;
                            }
                        }
                        while (info->NextEntryOffset != 0);

                        ResetEvent(m_events[index]);
                        readDirectoryChangesW(index);
                    }
                }
            });
        }

        std::thread m_thread {};
        volatile bool m_shouldStop = false;
        DynamicArray<HANDLE> m_dirHandles;
        DynamicArray<std::byte> m_buffer;
        DynamicArray<HANDLE> m_events;
        DynamicArray<eastl::string> m_oldFileNames;

        static constexpr size_t kBufferSize = 2048;

        eastl::function<void(eastl::string_view)> m_fileCreatedCallback;
        eastl::function<void(eastl::string_view)> m_fileModifiedCallback;
        eastl::function<void(eastl::string_view, eastl::string_view)> m_fileRenamedCallback;
        eastl::function<void(eastl::string_view)> m_fileDeletedCallback;
    };

    DirectoryMonitorHandle CreateDirectoryMonitor(
        const DirectoryMonitorCreateInfo& _info,
        const AllocatorInstance _allocator)
    {
        KE_ASSERT(!_info.m_directories.empty() && _info.m_directories.size() <= MAXIMUM_WAIT_OBJECTS);

        auto* monitor = _allocator.New<WindowsDirectoryMonitor>(_allocator);

        monitor->m_dirHandles.Resize(_info.m_directories.size());
        monitor->m_buffer.Resize(WindowsDirectoryMonitor::kBufferSize * _info.m_directories.size());
        monitor->m_events.Resize(_info.m_directories.size());
        monitor->m_oldFileNames.Resize(_info.m_directories.size());

        for (auto i = 0; i < _info.m_directories.size(); ++i)
        {
            monitor->m_dirHandles[i] = CreateFile(
                _info.m_directories[i].data(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr);
            if (monitor->m_dirHandles[i] == INVALID_HANDLE_VALUE)
            {
                _allocator.Delete(monitor);
                const DWORD error = GetLastError();
                if (error == ERROR_PATH_NOT_FOUND)
                {
                    return { OpaqueHandle { OpaqueHandle::Error::InvalidPath } };
                }
                else
                {
                    return { OpaqueHandle { OpaqueHandle::Error::Unknown } };
                }
            }

            monitor->m_events[i] = CreateEventA(nullptr, false, false, nullptr);
            if (monitor->m_events[i] == INVALID_HANDLE_VALUE)
            {
                _allocator.Delete(monitor);
                return { OpaqueHandle { OpaqueHandle::Error::Unknown } };
            }

            monitor->m_oldFileNames.Init(i, _allocator);
        }

        monitor->Start();

        return { OpaqueHandle { monitor} };
    }

    void DestroyDirectoryMonitor(const DirectoryMonitorHandle _handle, const AllocatorInstance _allocator)
    {
        KE_ASSERT(_handle.IsValid() && _handle.m_handle != nullptr);

        auto* monitor = static_cast<WindowsDirectoryMonitor*>(_handle.m_handle);
        _allocator.Delete(monitor);
    }
}