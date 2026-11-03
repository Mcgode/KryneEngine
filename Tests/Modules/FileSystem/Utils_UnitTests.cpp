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

        constexpr const char* pathWithTwoLeadingDoubleDots = "../../bar";
        NormalizePath(pathWithTwoLeadingDoubleDots, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "../../bar");

        constexpr const char* pathWithDoubleDotStepAfterLeadingDoubleDot = "../foo/../../bar";
        NormalizePath(pathWithDoubleDotStepAfterLeadingDoubleDot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "../../bar");

        constexpr const char* pathWithDoubleDotStepAfterTrailingDoubleDotInName = "foo../../bar";
        NormalizePath(pathWithDoubleDotStepAfterTrailingDoubleDotInName, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "bar");

        constexpr const char* absolutePath = "/foo/bar";
        NormalizePath(absolutePath, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/foo/bar");

        constexpr const char* absolutePathWithBackslash = "\\foo\\bar\\";
        NormalizePath(absolutePathWithBackslash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/foo/bar");

        constexpr const char* absolutePathWithDoubleSlash = "//foo/bar";
        NormalizePath(absolutePathWithDoubleSlash, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/foo/bar");

        constexpr const char* rootPath = "/";
        NormalizePath(rootPath, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/");

        constexpr const char* absolutePathBackToRoot = "/foo/..";
        NormalizePath(absolutePathBackToRoot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/");

        constexpr const char* absolutePathAboveRoot = "/foo/../../bar";
        NormalizePath(absolutePathAboveRoot, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "/bar");

        constexpr const char* driveLetterPath = "C:\\foo\\..\\bar";
        NormalizePath(driveLetterPath, tmpString);
        EXPECT_STREQ(tmpString.c_str(), "C:/bar");


        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        EXPECT_TRUE(catcher.GetCaughtMessages().empty());
    }
}