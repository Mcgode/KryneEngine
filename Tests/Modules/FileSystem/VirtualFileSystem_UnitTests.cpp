/**
 * @file
 * @author Max Godefroy
 * @date 03/11/2026.
 */

#include <filesystem>
#include <gtest/gtest.h>
#include <KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::FileSystem
{
    using namespace KryneEngine::Modules::FileSystem;

    TEST(VirtualFileSystem, OpenEvictAndDestroy)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        const std::filesystem::path thisFile = __FILE__;
        const std::string relativePath = std::filesystem::relative(thisFile).string();
        const std::string absolutePath = std::filesystem::absolute(thisFile.parent_path() / "CMakeLists.txt").string();
        ASSERT_TRUE(std::filesystem::exists(relativePath));
        ASSERT_TRUE(std::filesystem::exists(absolutePath));

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            // A single open file at a time, so that opening the second file evicts the first one
            VirtualFileSystem fileSystem { AllocatorInstance(), 1 };

            {
                const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(relativePath.c_str());
                ASSERT_TRUE(file.IsValid());

                char header[3] {};
                EXPECT_EQ(file.ReadT(0, header, 3), 3);
                EXPECT_EQ(eastl::string_view(header, 3), "/**");
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

        const std::filesystem::path thisFile = std::filesystem::absolute(__FILE__);
        const std::string firstPath = thisFile.string();
        const std::string secondPath = (thisFile.parent_path() / "CMakeLists.txt").string();
        const std::string thirdPath = (thisFile.parent_path() / "Utils_UnitTests.cpp").string();

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
}
