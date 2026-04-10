/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Modules/FileSystem/DirectoryTree.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Modules::FileSystem
{
    TEST(DirectoryTree, NoDirectories)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        Tests::ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const DirectoryTree tree { {} };
        catcher.ExpectNoMessage();

        {
            constexpr auto path = "test.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, nullptr);
            EXPECT_EQ(result.m_relativePath, path);
        }

        {
            constexpr auto path = "test/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, nullptr);
            EXPECT_EQ(result.m_relativePath, path);
        }

        {
            constexpr auto path = "../test/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, nullptr);
            EXPECT_EQ(result.m_relativePath, path);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }

    TEST(DirectoryTree, AddDirectory)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        Tests::ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        DirectoryTree tree { {} };

        // Test adding a directory
        u32 valueTest = 0;
        EXPECT_TRUE(tree.AddDirectory("test", &valueTest));

        // Test file in root just before adding root user ptr.
        {
            constexpr auto path = "test0.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, nullptr);
            EXPECT_STREQ(result.m_relativePath.data(), path);
        }

        // Can add root
        u32 valueRoot = 1;
        EXPECT_TRUE(tree.AddDirectory("", &valueRoot));

        // Cannot add directory outside working dir
        EXPECT_FALSE(tree.AddDirectory("../test", &valueTest));

        // Test nested within './test'
        {
            constexpr auto path = "test/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, &valueTest);
            EXPECT_STREQ(result.m_relativePath.data(), "sub.txt");
        }

        // Test outside working dir
        {
            constexpr auto path = "../test/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, nullptr);
            EXPECT_EQ(result.m_relativePath, path);
        }

        // Test slightly deep file where most specific directory is root
        {
            constexpr auto path = "test0/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, &valueRoot);
            EXPECT_STREQ(result.m_relativePath.data(), "test0/sub.txt");
        }

        // Test deeply nested file
        {
            constexpr auto path = "test/test0/1/2/3/4/5/6/sub.txt";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, &valueTest);
            EXPECT_STREQ(result.m_relativePath.data(), "test0/1/2/3/4/5/6/sub.txt");
        }

        // Retrieving a directory is valid.
        {
            constexpr auto path = "test";
            const DirectoryTree::SpecificDirectoryResult result = tree.FindMostSpecificDirectory(path);
            EXPECT_EQ(result.m_directoryPtr, &valueTest);
            EXPECT_EQ(result.m_relativePath.data(), eastl::string_view());
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        catcher.ExpectNoMessage();
    }
}