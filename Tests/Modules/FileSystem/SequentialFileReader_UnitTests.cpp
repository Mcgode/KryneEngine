/**
 * @file
 * @author Max Godefroy
 * @date 12/11/2026.
 */

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <KryneEngine/Modules/FileSystem/Archive.hpp>
#include <KryneEngine/Modules/FileSystem/SequentialFileReader.hpp>
#include <KryneEngine/Modules/FileSystem/VirtualFileSystem.hpp>
#include <vector>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::FileSystem
{
    using namespace KryneEngine::Modules::FileSystem;

    namespace
    {
        // A temporary directory holding a raw file, and an archive mounted on `data/` with a compressed copy and a
        // stored copy of it
        struct CompressedArchiveFixture
        {
            static constexpr size_t kContentSize = 64 << 10;

            std::vector<std::byte> m_content;
            std::filesystem::path m_directory;
            std::filesystem::path m_rawFile;
            std::filesystem::path m_archive;

            CompressedArchiveFixture()
            {
                // Compressible, but not trivially: long runs would hide stream position errors
                m_content.resize(kContentSize);
                u32 state = 12345;
                for (size_t i = 0; i < kContentSize; i++)
                {
                    state = state * 1664525u + 1013904223u;
                    m_content[i] = static_cast<std::byte>((state >> 24) & 0x0F);
                }

                m_directory = std::filesystem::temp_directory_path() / "KryneEngine_SequentialFileReaderTests";
                std::filesystem::create_directories(m_directory);
                m_rawFile = m_directory / "content.bin";
                m_archive = m_directory / "test.kar";

                {
                    std::ofstream raw(m_rawFile, std::ios::binary | std::ios::trunc);
                    raw.write(reinterpret_cast<const char*>(m_content.data()), static_cast<std::streamsize>(kContentSize));
                }

                std::ofstream archive(m_archive, std::ios::binary | std::ios::trunc);
                ArchiveMaker maker(archive, "data", 2);
                {
                    std::ifstream input(m_rawFile, std::ios::binary);
                    maker.AddFile(input, "compressed.bin", FileFlags::ZstdCompressed);
                }
                {
                    std::ifstream input(m_rawFile, std::ios::binary);
                    maker.AddFile(input, "stored.bin", FileFlags::None);
                }
                maker.Finish();
            }

            ~CompressedArchiveFixture()
            {
                std::error_code error;
                std::filesystem::remove_all(m_directory, error);
            }
        };

        // Reads the whole file in parts of uneven sizes, and compares them to the expected content
        void ExpectSequentialContent(const ReadOnlyFile& _file, const std::vector<std::byte>& _expected)
        {
            SequentialFileReader reader({}, _file);
            ASSERT_TRUE(reader.IsValid());
            ASSERT_EQ(reader.GetSize(), _expected.size());
            EXPECT_EQ(reader.GetPosition(), 0u);

            std::vector<std::byte> content(_expected.size());
            constexpr size_t kPartSizes[] = { 64, 1, 4000, 0, 13 };
            size_t position = 0;
            for (const size_t partSize: kPartSizes)
            {
                ASSERT_TRUE(reader.Read({ content.data() + position, partSize }));
                position += partSize;
                EXPECT_EQ(reader.GetPosition(), position);
            }

            // Reading past the end fails, without moving the position
            std::vector<std::byte> tooLarge(_expected.size());
            EXPECT_FALSE(reader.Read({ tooLarge.data(), tooLarge.size() - position + 1 }));
            EXPECT_EQ(reader.GetPosition(), position);

            ASSERT_TRUE(reader.Read({ content.data() + position, content.size() - position }));
            EXPECT_EQ(reader.GetPosition(), reader.GetSize());
            EXPECT_EQ(content, _expected);

            std::byte extra {};
            EXPECT_FALSE(reader.Read({ &extra, 1 }));
        }
    }

    TEST(SequentialFileReader, ReadsCompressedAndStoredFiles)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        CompressedArchiveFixture fixture;
        VirtualFileSystem fileSystem { {}, 2 };
        ASSERT_TRUE(fileSystem.MountArchive(fixture.m_archive.string().c_str()));

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const std::string path = (fixture.m_directory / "data" / "compressed.bin").string();
            const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(path.c_str());
            ASSERT_TRUE(file.IsValid());
            ASSERT_TRUE(BitUtils::EnumHasAny(file.GetFlags(), FileFlags::ZstdCompressed));
            EXPECT_LT(file.GetSize(), CompressedArchiveFixture::kContentSize);
            ExpectSequentialContent(file, fixture.m_content);
        }

        {
            const std::string path = (fixture.m_directory / "data" / "stored.bin").string();
            const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(path.c_str());
            ASSERT_TRUE(file.IsValid());
            ASSERT_FALSE(BitUtils::EnumHasAny(file.GetFlags(), FileFlags::ZstdCompressed));
            ExpectSequentialContent(file, fixture.m_content);
        }

        {
            const ReadOnlyFile file = fileSystem.OpenReadOnlyFile(fixture.m_rawFile.string().c_str());
            ASSERT_TRUE(file.IsValid());
            ExpectSequentialContent(file, fixture.m_content);
        }

        catcher.ExpectNoMessage();
    }

    TEST(SequentialFileReader, InvalidFile)
    {
        ScopedAssertCatcher catcher;

        const ReadOnlyFile file;
        SequentialFileReader reader({}, file);
        EXPECT_FALSE(reader.IsValid());
        EXPECT_EQ(reader.GetSize(), 0u);

        std::byte value {};
        EXPECT_FALSE(reader.Read({ &value, 1 }));

        catcher.ExpectNoMessage();
    }
}
