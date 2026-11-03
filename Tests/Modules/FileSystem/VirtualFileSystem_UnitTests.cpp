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
}
