/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2026.
 */

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <KryneEngine/Core/Platform/StdAlloc.hpp>
#include <KryneEngine/Modules/FileSystem/Archive.hpp>
#include <KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::FileSystem
{
    using namespace KryneEngine::Modules::FileSystem;

    namespace
    {
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

            s64 m_liveAllocations = 0;
        };

        // A temporary directory holding a raw text file, and an archive mounted on `data/` with a copy of it
        struct ArchiveFixture
        {
            static constexpr const char* kContent = "Hello from the archive";

            std::filesystem::path m_directory;
            std::filesystem::path m_rawFile;
            std::filesystem::path m_archive;
            std::filesystem::path m_corruptedArchive;

            ArchiveFixture()
            {
                m_directory = std::filesystem::temp_directory_path() / "KryneEngine_VirtualFileSystemTests";
                std::filesystem::create_directories(m_directory);
                m_rawFile = m_directory / "hello.txt";
                m_archive = m_directory / "test.kar";
                m_corruptedArchive = m_directory / "corrupted.kar";

                {
                    // Valid header, but the tail points to strings inside the header
                    std::ofstream corrupted(m_corruptedArchive, std::ios::binary | std::ios::trunc);
                    constexpr Archive::Header header { Archive::kMagicNumber, Archive::kVersion };
                    constexpr Archive::Tail tail { 0, 0 };
                    corrupted.write(reinterpret_cast<const char*>(&header), sizeof(header));
                    corrupted.write(reinterpret_cast<const char*>(&tail), sizeof(tail));
                }

                {
                    std::ofstream raw(m_rawFile, std::ios::binary | std::ios::trunc);
                    raw << kContent;
                }

                std::ofstream archive(m_archive, std::ios::binary | std::ios::trunc);
                std::ifstream input(m_rawFile, std::ios::binary);
                ArchiveMaker maker(archive, "data", 1);
                maker.AddFile(input, "hello.txt", FileFlags::None);
                maker.Finish();
            }

            ~ArchiveFixture()
            {
                std::error_code error;
                std::filesystem::remove_all(m_directory, error);
            }
        };

        // Plain files written by the test itself, so the tests don't depend on the source tree being available
        // (CI runs them on a machine that only has the build artifacts).
        struct LooseFilesFixture
        {
            static constexpr const char* kContent = "Hello from a loose file";

            // Absolute paths, in a temporary directory
            std::filesystem::path m_directory;
            std::filesystem::path m_firstFile;
            std::filesystem::path m_secondFile;
            std::filesystem::path m_thirdFile;

            // Relative to the working directory, as relative paths can't be built to another drive on Windows
            std::filesystem::path m_relativeDirectory;
            std::filesystem::path m_relativeFile;

            LooseFilesFixture()
            {
                m_directory = std::filesystem::temp_directory_path() / "KryneEngine_VirtualFileSystemTests_Loose";
                std::filesystem::create_directories(m_directory);
                m_firstFile = m_directory / "first.txt";
                m_secondFile = m_directory / "second.txt";
                m_thirdFile = m_directory / "third.txt";

                m_relativeDirectory = "KryneEngine_VirtualFileSystemTests_Relative";
                std::filesystem::create_directories(m_relativeDirectory);
                m_relativeFile = m_relativeDirectory / "relative.txt";

                for (const std::filesystem::path& path: { m_firstFile, m_secondFile, m_thirdFile, m_relativeFile })
                {
                    std::ofstream file(path, std::ios::binary | std::ios::trunc);
                    file << kContent;
                }
            }

            ~LooseFilesFixture()
            {
                std::error_code error;
                std::filesystem::remove_all(m_directory, error);
                std::filesystem::remove_all(m_relativeDirectory, error);
            }
        };
    }

    TEST(VirtualFileSystem, OpenEvictAndDestroy)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        LooseFilesFixture fixture;

        const std::string relativePath = fixture.m_relativeFile.string();
        const std::string absolutePath = fixture.m_secondFile.string();
        ASSERT_TRUE(fixture.m_relativeFile.is_relative());
        ASSERT_TRUE(fixture.m_secondFile.is_absolute());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            // A single open file at a time, so that opening the second file evicts the first one
            VirtualFileSystem fileSystem { AllocatorInstance(), 1 };

            {
                const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(relativePath.c_str());
                ASSERT_TRUE(file.IsValid());

                char header[5] {};
                EXPECT_EQ(file.ReadT(0, header, 5), 5);
                EXPECT_EQ(eastl::string_view(header, 5), "Hello");
            }

            {
                const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(absolutePath.c_str());
                ASSERT_TRUE(file.IsValid()) << "Absolute paths must be supported";
                EXPECT_EQ(file.GetSize(), std::filesystem::file_size(absolutePath));
            }

            // Destroying the file system closes the file still in the cache
        }

        catcher.ExpectNoMessage();
    }

    TEST(VirtualFileSystem, InvalidAndMovedFiles)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        LooseFilesFixture fixture;

        const std::string firstPath = fixture.m_firstFile.string();
        const std::string secondPath = fixture.m_secondFile.string();
        const std::string thirdPath = fixture.m_thirdFile.string();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            // A single open file at a time: a leaked reference would prevent any other file from being opened
            VirtualFileSystem fileSystem { AllocatorInstance(), 1 };

            {
                const ReadOnlyFile defaultFile;
                EXPECT_FALSE(defaultFile.IsValid());

                const ReadOnlyFile missingFile = fileSystem.OpenReadOnlyFile("does/not/exist.txt");
                EXPECT_FALSE(missingFile.IsValid());
            }

            {
                ReadOnlyFile source = fileSystem.OpenReadOnlyFile(firstPath.c_str());
                ASSERT_TRUE(source.IsValid());
                const size_t size = source.GetSize();

                ReadOnlyFile moved(std::move(source));
                EXPECT_FALSE(source.IsValid());
                ASSERT_TRUE(moved.IsValid());
                EXPECT_EQ(moved.GetSize(), size);

                // Move-assigning over an open file releases it
                ReadOnlyFile target;
                target = std::move(moved);
                EXPECT_FALSE(moved.IsValid());
                ASSERT_TRUE(target.IsValid());

                target = ReadOnlyFile();
                EXPECT_FALSE(target.IsValid());
            }

            // All references were released exactly once, so the cached file can be evicted
            const ReadOnlyFile other = fileSystem.OpenReadOnlyFile(secondPath.c_str());
            EXPECT_TRUE(other.IsValid());
        }

        {
            VirtualFileSystem fileSystem { AllocatorInstance(), 1 };
            ReadOnlyFile first = fileSystem.OpenReadOnlyFile(firstPath.c_str());
            ASSERT_TRUE(first.IsValid());

            // Move-assigning a file over another one releases the overwritten one
            first = fileSystem.OpenReadOnlyFile(firstPath.c_str());
            ASSERT_TRUE(first.IsValid());
            first = ReadOnlyFile();

            const ReadOnlyFile third = fileSystem.OpenReadOnlyFile(thirdPath.c_str());
            EXPECT_TRUE(third.IsValid());
        }

        catcher.ExpectNoMessage();
    }

    TEST(VirtualFileSystem, MountArchive)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        ArchiveFixture fixture;
        LooseFilesFixture looseFiles;
        CountingAllocator allocator;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            // Two open files at most: the archive, and one other file
            VirtualFileSystem fileSystem { &allocator, 2 };

            // Not an archive: mounting fails, and must not keep the file open
            EXPECT_FALSE(fileSystem.MountArchive(fixture.m_rawFile.string().c_str()));
            // Corrupted archive: mounting fails, without leaking the partially loaded archive
            EXPECT_FALSE(fileSystem.MountArchive(fixture.m_corruptedArchive.string().c_str()));

            ASSERT_TRUE(fileSystem.MountArchive(fixture.m_archive.string().c_str()));

            {
                const std::string virtualPath = (fixture.m_directory / "data" / "hello.txt").string();
                const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(virtualPath.c_str());
                ASSERT_TRUE(file.IsValid());
                ASSERT_EQ(file.GetSize(), strlen(ArchiveFixture::kContent));

                char content[64] {};
                EXPECT_EQ(file.ReadT(0, content, file.GetSize()), file.GetSize());
                EXPECT_STREQ(content, ArchiveFixture::kContent);
            }

            // The failed mount didn't keep a reference on the raw file, so it can be evicted to open another file
            // next to the archive
            const ReadOnlyFile other = fileSystem.OpenReadOnlyFile(looseFiles.m_firstFile.string().c_str());
            EXPECT_TRUE(other.IsValid());
        }

        EXPECT_EQ(allocator.m_liveAllocations, 0) << "File system leaked some allocations";

        catcher.ExpectNoMessage();
    }

    TEST(VirtualFileSystem, MountArchiveOpensItOnce)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        ArchiveFixture fixture;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // A single open file at a time: the archive must only take one slot of the cache to be mounted, even when
        // its path isn't normalized. Backslashes are used, as in native Windows paths, so the mount point must be
        // found from either separator.
        VirtualFileSystem fileSystem { AllocatorInstance(), 1 };
        const std::string archivePath = fixture.m_directory.string() + "\\.\\" + fixture.m_archive.filename().string();
        ASSERT_TRUE(fileSystem.MountArchive(archivePath.c_str()));

        // Files inside the archive share its descriptor
        const std::string virtualPath = (fixture.m_directory / "data" / "hello.txt").string();
        const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(virtualPath.c_str());
        ASSERT_TRUE(file.IsValid());
        EXPECT_EQ(file.GetSize(), strlen(ArchiveFixture::kContent));

        catcher.ExpectNoMessage();
    }
}
