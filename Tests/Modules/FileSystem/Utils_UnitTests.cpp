/**
 * @file
 * @author Max Godefroy
 * @date 09/04/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Modules/FileSystem/Utils.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Modules::FileSystem
{
    TEST(Utils, NormalizePath)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        Tests::ScopedAssertCatcher catcher;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        eastl::string tmpString;

        constexpr const char* basicPath = "foo/bar";
        NormalizePath(basicPath, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithTrailingSlash = "foo/bar/";
        NormalizePath(pathWithTrailingSlash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithBackslash = "foo\\bar";
        NormalizePath(pathWithBackslash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithTrailingSlashAndBackslash = "foo/bar\\";
        NormalizePath(pathWithTrailingSlashAndBackslash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithDoubleSlash = "foo//bar/";
        NormalizePath(pathWithDoubleSlash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithDotStep = "foo/./bar";
        NormalizePath(pathWithDotStep, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithLeadingDot = "./foo/bar";
        NormalizePath(pathWithLeadingDot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithTrailingDot = "foo/bar/.";
        NormalizePath(pathWithTrailingDot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithDotStepAndDot = "foo/././bar";
        NormalizePath(pathWithDotStepAndDot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar");

        constexpr const char* pathWithLeadingDotInName = ".foo/bar";
        NormalizePath(pathWithLeadingDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), ".foo/bar");

        constexpr const char* pathWithTrailingDotInName = "foo/bar.";
        NormalizePath(pathWithTrailingDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar.");

        constexpr const char* pathWithDotInName = "foo/bar.ext";
        NormalizePath(pathWithDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar.ext");

        constexpr const char* pathWithDoubleDotStep = "foo/../bar";
        NormalizePath(pathWithDoubleDotStep, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "bar");

        constexpr const char* pathWithLeadingDoubleDot = "../foo/bar";
        NormalizePath(pathWithLeadingDoubleDot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "../foo/bar");

        constexpr const char* pathWithDoubleDoubleDotStep = "foo/../../bar";
        NormalizePath(pathWithDoubleDoubleDotStep, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "../bar");

        constexpr const char* pathWithLeadingDoubleDotInName = "foo/..bar";
        NormalizePath(pathWithLeadingDoubleDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/..bar");

        constexpr const char* pathWithTrailingDoubleDotInName = "foo../bar";
        NormalizePath(pathWithTrailingDoubleDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo../bar");

        constexpr const char* pathWithDoubleDotInName = "foo/bar..ext";
        NormalizePath(pathWithDoubleDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "foo/bar..ext");


        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        EXPECT_TRUE(catcher.GetCaughtMessages().empty());
    }
}