/**
 * @file
 * @author Max Godefroy
 * @date 27/11/2024.
 */

#include "KryneEngine/Core/Graphics/Buffer.hpp"
#include "KryneEngine/Core/Graphics/GraphicsContext.hpp"
#include "KryneEngine/Core/Graphics/MemoryBarriers.hpp"
#include <EASTL/initializer_list.h>
#include <KryneEngine/Core/Common/EastlHelpers.hpp>
#include <gtest/gtest.h>

#include "Common.h"
#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Graphics
{
    TEST(ResourceCopy, StagingBufferCopy)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());

        constexpr size_t payload = 0x0123456789abcdef;

        BufferHandle srcBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "SrcBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        });

        BufferHandle dstBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "DstBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferDstBuffer,
        });


        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        CommandListHandle commandList = graphicsContext->BeginGraphicsCommandList();

        {
            BufferMapping srcMapping { srcBuffer, sizeof(payload) };
            graphicsContext->MapBuffer(srcMapping);
            memcpy(srcMapping.m_ptr, &payload, sizeof(payload));
            graphicsContext->UnmapBuffer(srcMapping);
        }

        BufferMemoryBarrier barriers[] = {
            BufferMemoryBarrier {
                .m_stagesSrc = BarrierSyncStageFlags::All,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferSrc,
                .m_buffer = srcBuffer,
            },
            BufferMemoryBarrier {
                .m_stagesSrc = BarrierSyncStageFlags::All,
                .m_stagesDst = BarrierSyncStageFlags::Transfer,
                .m_accessSrc = BarrierAccessFlags::None,
                .m_accessDst = BarrierAccessFlags::TransferDst,
                .m_buffer = dstBuffer,
            },
        };

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            { barriers },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = srcBuffer,
                .m_bufferDst = dstBuffer,
            });

        graphicsContext->EndGraphicsCommandList(commandList);
        graphicsContext->EndFrame();
        graphicsContext->WaitForLastFrame();

        {
            BufferMapping dstMapping { dstBuffer, sizeof(payload), 0, false };
            graphicsContext->MapBuffer(dstMapping);
            const size_t result = *reinterpret_cast<size_t*>(dstMapping.m_ptr);
            EXPECT_EQ(result, payload);
            graphicsContext->UnmapBuffer(dstMapping);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        graphicsContext->DestroyBuffer(dstBuffer);
        graphicsContext->DestroyBuffer(srcBuffer);

        GraphicsContext::Destroy(graphicsContext);
        catcher.ExpectNoMessage();
    }

    TEST(ResourceCopy, BufferRoundGpuTrip)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());

        constexpr size_t payload = 0x0123456789abcdef;

        BufferHandle srcBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "SrcBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        });

        BufferHandle gpuBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "GpuBuffer",
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferSrcBuffer | MemoryUsage::TransferDstBuffer,
        });

        BufferHandle dstBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "DstBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferDstBuffer,
        });


        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        CommandListHandle commandList = graphicsContext->BeginGraphicsCommandList();

        {
            BufferMapping srcMapping { srcBuffer, sizeof(payload) };
            graphicsContext->MapBuffer(srcMapping);
            memcpy(srcMapping.m_ptr, &payload, sizeof(payload));
            graphicsContext->UnmapBuffer(srcMapping);
        }

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            {
                {
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = srcBuffer,
                    },
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = gpuBuffer,
                    },
                }
            },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = srcBuffer,
                .m_bufferDst = gpuBuffer,
            });

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            {
                {
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::TransferDst,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = gpuBuffer,
                    },
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = dstBuffer,
                    },
                }
            },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = gpuBuffer,
                .m_bufferDst = dstBuffer,
            });

        graphicsContext->EndGraphicsCommandList(commandList);
        graphicsContext->EndFrame();
        graphicsContext->WaitForLastFrame();

        {
            BufferMapping dstMapping { dstBuffer, sizeof(payload), 0, false };
            graphicsContext->MapBuffer(dstMapping);
            const size_t result = *reinterpret_cast<size_t*>(dstMapping.m_ptr);
            EXPECT_EQ(result, payload);
            graphicsContext->UnmapBuffer(dstMapping);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        graphicsContext->DestroyBuffer(dstBuffer);
        graphicsContext->DestroyBuffer(gpuBuffer);
        graphicsContext->DestroyBuffer(srcBuffer);

        GraphicsContext::Destroy(graphicsContext);
        catcher.ExpectNoMessage();
    }

    TEST(ResourceCopy, GpuBufferCopy)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        const GraphicsCommon::ApplicationInfo appInfo = DefaultAppInfo();
        GraphicsContext* graphicsContext = GraphicsContext::Create(appInfo, nullptr, AllocatorInstance());

        constexpr size_t payload = 0x0123456789abcdef;

        BufferHandle srcBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "SrcBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferSrcBuffer,
        });

        BufferHandle gpuBuffer0 = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "GpuBuffer0",
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferSrcBuffer | MemoryUsage::TransferDstBuffer,
        });

        BufferHandle gpuBuffer1 = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "GpuBuffer1",
#endif
            },
            .m_usage = MemoryUsage::GpuOnly_UsageType | MemoryUsage::TransferSrcBuffer | MemoryUsage::TransferDstBuffer,
        });

        BufferHandle dstBuffer = graphicsContext->CreateBuffer({
            .m_desc = {
                .m_size = sizeof(payload),
#if !defined(KE_FINAL)
                .m_debugName = "DstBuffer",
#endif
            },
            .m_usage = MemoryUsage::StageOnce_UsageType | MemoryUsage::TransferDstBuffer,
        });


        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        CommandListHandle commandList = graphicsContext->BeginGraphicsCommandList();

        {
            BufferMapping srcMapping { srcBuffer, sizeof(payload) };
            graphicsContext->MapBuffer(srcMapping);
            memcpy(srcMapping.m_ptr, &payload, sizeof(payload));
            graphicsContext->UnmapBuffer(srcMapping);
        }

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            {
                {
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = srcBuffer,
                    },
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = gpuBuffer0,
                    },
                }
            },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = srcBuffer,
                .m_bufferDst = gpuBuffer0,
            });

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            {
                {
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::TransferDst,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = gpuBuffer0,
                    },
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = gpuBuffer1,
                    },
                }
            },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = gpuBuffer0,
                .m_bufferDst = gpuBuffer1,
            });

        graphicsContext->PlaceMemoryBarriers(
            commandList,
            {},
            {
                {
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::Transfer,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::TransferDst,
                        .m_accessDst = BarrierAccessFlags::TransferSrc,
                        .m_buffer = gpuBuffer1,
                    },
                    BufferMemoryBarrier{
                        .m_stagesSrc = BarrierSyncStageFlags::All,
                        .m_stagesDst = BarrierSyncStageFlags::Transfer,
                        .m_accessSrc = BarrierAccessFlags::None,
                        .m_accessDst = BarrierAccessFlags::TransferDst,
                        .m_buffer = dstBuffer,
                    },
                }
            },
            {});

        graphicsContext->CopyBuffer(
            commandList,
            {
                .m_copySize = sizeof(payload),
                .m_bufferSrc = gpuBuffer1,
                .m_bufferDst = dstBuffer,
            });

        graphicsContext->EndGraphicsCommandList(commandList);
        graphicsContext->EndFrame();
        graphicsContext->WaitForLastFrame();

        {
            BufferMapping dstMapping { dstBuffer, sizeof(payload), 0, false };
            graphicsContext->MapBuffer(dstMapping);
            const size_t result = *reinterpret_cast<size_t*>(dstMapping.m_ptr);
            EXPECT_EQ(result, payload);
            graphicsContext->UnmapBuffer(dstMapping);
        }

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------

        graphicsContext->DestroyBuffer(dstBuffer);
        graphicsContext->DestroyBuffer(gpuBuffer1);
        graphicsContext->DestroyBuffer(gpuBuffer0);
        graphicsContext->DestroyBuffer(srcBuffer);

        GraphicsContext::Destroy(graphicsContext);
        catcher.ExpectNoMessage();
    }
}