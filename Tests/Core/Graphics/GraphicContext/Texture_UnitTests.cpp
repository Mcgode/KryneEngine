/**
 * @file
 * @author Max Godefroy
 * @date 19/10/2024.
 */

#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/Texture.hpp"
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

#include "Common.h"

namespace KryneEngine::Tests::Graphics
{
    const TextureCreateDesc defaultTextureCreateDesc {
        .m_desc = {
            .m_dimensions = { 128, 128, 1 },
            .m_format = TextureFormat::RGBA8_UNorm,
            .m_arraySize = 1,
            .m_type = TextureTypes::Single2D,
            .m_mipCount = 1,
#if !defined(KE_FINAL)
            .m_debugName = "Unit Test Texture 0",
#endif
        },
        .m_footprintPerSubResource = {},
        .m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage,
    };

    TEST(Texture, CreateTextureBasic)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32 errorCount = 0;

        {
            const TextureHandle texture = graphicsContext->CreateTexture(defaultTextureCreateDesc);
            catcher.ExpectMessageCount(errorCount);
            EXPECT_NE(texture, GenPool::kInvalidHandle);
            EXPECT_EQ(texture, (GenPool::Handle { 0 }));
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        GraphicsContext::Destroy(graphicsContext);

#if defined(KE_GRAPHICS_API_VK)
        // One error from validation layers, non-destroyed buffer
        if (catcher.GetCaughtMessages().size() < errorCount)
        {
            const eastl::string& message = catcher.GetCaughtMessages()[errorCount].m_message;
            EXPECT_TRUE(
                message.find("Validation Error: [ VUID-vkDestroyDevice-device-05137 ]") != eastl::string::npos
                && message.find("name = Unit Test Texture 0") != eastl::string::npos);
        }
        errorCount += 1;

        // One error from VMA leaks check
        if (catcher.GetCaughtMessages().size() < errorCount)
        {
            const eastl::string& message = catcher.GetCaughtMessages()[errorCount].m_message;
            EXPECT_NE(
                message.find("\"Some allocations were not freed before destruction of this memory block!\""),
                eastl::string::npos);
        }
        errorCount += 1;
#elif defined(KE_GRAPHICS_API_DX12)
        // No leak warning in DirectX 12 :(
#endif

        catcher.ExpectMessageCount(errorCount);
    }

    TEST(Texture, DestroyTexture)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());
        const TextureHandle texture = graphicsContext->CreateTexture(defaultTextureCreateDesc);

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        EXPECT_FALSE(graphicsContext->DestroyTexture({ GenPool::kInvalidHandle }));
        EXPECT_TRUE(graphicsContext->DestroyTexture(texture));
        EXPECT_FALSE(graphicsContext->DestroyTexture(texture));

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        GraphicsContext::Destroy(graphicsContext);
        catcher.ExpectNoMessage();
    }

    TEST(Texture, CreateTextureAdvanced)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());
        eastl::vector<TextureHandle> textures;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        u32 errorCount = 0;
        u16 index = 0;

        // Erroneous textures
        {
            // Texture with any dimension set to 0 is invalid
            {
                {
                    TextureCreateDesc desc = defaultTextureCreateDesc;
                    desc.m_desc.m_dimensions.x = 0;
                    textures.push_back(graphicsContext->CreateTexture(desc));
                }
                errorCount++;
                catcher.ExpectMessageCount(errorCount);
                EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

                {
                    TextureCreateDesc desc = defaultTextureCreateDesc;
                    desc.m_desc.m_dimensions.y = 0;
                    textures.push_back(graphicsContext->CreateTexture(desc));
                }
                errorCount++;
                catcher.ExpectMessageCount(errorCount);
                EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

                {
                    TextureCreateDesc desc = defaultTextureCreateDesc;
                    desc.m_desc.m_dimensions.z = 0;
                    textures.push_back(graphicsContext->CreateTexture(desc));
                }
                errorCount++;
                catcher.ExpectMessageCount(errorCount);
                EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

                {
                    TextureCreateDesc desc = defaultTextureCreateDesc;
                    desc.m_desc.m_arraySize = 0;
                    textures.push_back(graphicsContext->CreateTexture(desc));
                }
                errorCount++;
                catcher.ExpectMessageCount(errorCount);
                EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

                {
                    TextureCreateDesc desc = defaultTextureCreateDesc;
                    desc.m_desc.m_mipCount = 0;
                    textures.push_back(graphicsContext->CreateTexture(desc));
                }
                errorCount++;
                catcher.ExpectMessageCount(errorCount);
                EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);
            }

            // Textures without usage are invalid
            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);
        }

        // Valid textures with only 1 usage
        {
            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferSrcImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::WriteImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::D32F;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});
        }

        // Some textures with multiple usages
        {
            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferDstImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ReadWriteImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::ColorTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::D32F;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::D32F;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::D32F;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage | MemoryUsage::SampledImage | MemoryUsage::ReadImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back().m_handle, GenPool::Handle{index++});
        }

        // Can only have GPU only textures
        {
            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::StageOnce_UsageType | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::StageEveryFrame_UsageType | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_memoryUsage = MemoryUsage::Readback_UsageType | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);
        }

        // DepthStencil usage can only be done with depth or stencil textures
        {
            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::R8_UNorm;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::DepthStencilTargetImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);

            {
                TextureCreateDesc desc = defaultTextureCreateDesc;
                desc.m_desc.m_format = TextureFormat::D32F;
                desc.m_memoryUsage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::SampledImage;
                textures.push_back(graphicsContext->CreateTexture(desc));
            }
            errorCount++;
            catcher.ExpectMessageCount(errorCount);
            EXPECT_EQ(textures.back(), GenPool::kInvalidHandle);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        for (const TextureHandle handle: textures)
        {
            graphicsContext->DestroyTexture(handle);
        }

        GraphicsContext::Destroy(graphicsContext);
        catcher.ExpectMessageCount(errorCount);
    }
}
