/**
 * @file
 * @author Max Godefroy
 * @date 15/11/2024.
 */

#include "KryneEngine/Modules/RenderGraph/RenderGraph.hpp"

#include "KryneEngine/Core/Memory/Allocators/StackAllocator.hpp"

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Math/Color.hpp>
#include <KryneEngine/Core/Profiling/TracyGpuScope.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>

#include "KryneEngine/Modules/RenderGraph/Builder.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"
#include "KryneEngine/Modules/RenderGraph/Utils/ResourceStateTracker.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    RenderGraph::RenderGraph(const AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_previousFramePassPerformance(_allocator)
        , m_currentFramePassPerformance(_allocator)
        , m_renderPassCache(_allocator)
    {
        m_scratchAllocator = m_allocator.New<StackAllocator>(_allocator, kScratchAllocatorSize, 5, "Render graph stack allocator");
        m_registry = m_allocator.New<Registry>(_allocator);
        m_resourceStateTracker = m_allocator.New<ResourceStateTracker>(_allocator);
    }

    RenderGraph::~RenderGraph()
    {
        m_allocator.Delete(m_resourceStateTracker);
        m_allocator.Delete(m_registry);
        m_allocator.Delete(m_scratchAllocator);
    }

    Builder& RenderGraph::BeginFrame()
    {
        const AllocatorInstance scratchAllocator { m_scratchAllocator };
        m_builder = scratchAllocator.New<Builder>(GetRegistry(), scratchAllocator);
        return *m_builder;
    }

    void RenderGraph::SubmitFrame(GraphicsContext& _graphicsContext, FibersManager* _fibersManager)
    {
        const AllocatorInstance scratchAllocator = m_scratchAllocator;
        eastl::vector<JobData> jobs { scratchAllocator };

        {
            KE_ZoneScoped("Build and cull render DAG (if not already done)");
            m_builder->BuildDag();
        }

        {
            KE_ZoneScoped("Prepare render jobs");

            m_resourceStateTracker->Process(*m_builder, *m_registry, scratchAllocator);

            const auto initJobData = [&](JobData& _jobData, u32 _start)
            {
                _jobData.m_renderGraph = this;
                _jobData.m_passExecutionData = PassExecutionData {
                    .m_graphicsContext = &_graphicsContext,
                    .m_commandList = _graphicsContext.BeginGraphicsCommandList()
                };
                _jobData.m_passRangeStart = _start;
                _jobData.m_passRangeCount = 0;
            };

            JobData* currentJob = nullptr;

            constexpr double maxOverfillRatio = 1.25; // Accept up to 25% longer command list from one long pass.
            const u64 maxOverfillDuration = static_cast<u64>(m_targetTimePerCommandList * maxOverfillRatio * 1'000'000);
            const u64 targetDuration = static_cast<u64>(m_targetTimePerCommandList * 1'000'000);
            u64 cumulativeDuration = 0;

            for (size_t i = 0; i < m_builder->m_declaredPasses.size(); i++)
            {
                if (!m_builder->m_passAlive[i])
                {
                    continue;
                }

                PassDeclaration& pass = m_builder->m_declaredPasses[i];
                const u64 estimatedPassDuration = m_previousFramePassPerformance[pass.m_name];

                // Prevent the current job from overfilling beyond a certain threshold.
                if (cumulativeDuration + estimatedPassDuration > maxOverfillDuration)
                {
                    cumulativeDuration = 0;
                    currentJob = nullptr;
                }

                if (currentJob == nullptr)
                {
                    currentJob = &jobs.emplace_back();
                    initJobData(*currentJob, i);
                }

                currentJob->m_passRangeCount = i - currentJob->m_passRangeStart + 1;
                cumulativeDuration += estimatedPassDuration;

                // Make sure to cache the render pass.
                if (pass.m_type == PassType::Render)
                {
                    FetchRenderPass(_graphicsContext, pass);
                }

                // Reserve entry in map, so that duration saving is thread-safe
                m_currentFramePassPerformance.emplace(pass.m_name, 0);

                // If beyond the target duration, move to the next job
                if (cumulativeDuration > targetDuration)
                {
                    cumulativeDuration = 0;
                    currentJob = nullptr;
                }
            }
        }

        {
            KE_ZoneScoped("Dispatch & execute render jobs");

            if (_fibersManager != nullptr)
            {
                // Execute the last job in this thread/fiber, schedule the other ones for dispatch.
                // Small optimization.
                if (jobs.size() > 1)
                {
                    const SyncCounterId jobsCounter = _fibersManager->InitAndBatchJobs({
                        .m_function = [&jobs](const u16 _jobIndex)
                        {
                            ExecuteJob(&jobs[_jobIndex], _jobIndex);
                        },
                        .m_jobCount = static_cast<u16>(jobs.size() - 1)
                    });
                    ExecuteJob(&jobs.back(), jobs.size() - 1);
                    _fibersManager->WaitForCounterAndReset(jobsCounter);
                }
                else if (!jobs.empty())
                {
                    ExecuteJob(&jobs.back(), jobs.size() - 1);
                }
            }
            else {
                for (size_t i = 0; i < jobs.size(); i++)
                {
                    ExecuteJob(&jobs[i], i);
                }
            }
            jobs.clear();
        }

        {
            KE_ZoneScoped("Cleanup");

            eastl::swap(m_previousFramePassPerformance, m_currentFramePassPerformance);
            m_currentFramePassPerformance.clear();

            m_previousFrameTotalDuration = m_currentFrameTotalDuration.load(std::memory_order_acquire);
            m_currentFrameTotalDuration.store(0, std::memory_order_relaxed);

            scratchAllocator.Delete(m_builder);
            m_builder = nullptr;

            // Keeps the grown heaps around for reuse next frame, instead of freeing and
            // re-allocating them every frame (which Reset() would do).
            m_scratchAllocator->Clear();
        }
    }

    void RenderGraph::ExecuteJob(JobData* _jobData, const u16 _jobIndex)
    {
        KE_ZoneScopedF("Execute render job #%u", _jobIndex);

        for (auto i = _jobData->m_passRangeStart; i < _jobData->m_passRangeStart + _jobData->m_passRangeCount; i++)
        {
            if (!_jobData->m_renderGraph->m_builder->m_passAlive[i])
                continue;

            const PassDeclaration& pass = _jobData->m_renderGraph->m_builder->m_declaredPasses[i];

            const std::chrono::time_point start = std::chrono::steady_clock::now();
            _jobData->m_passExecutionData.m_graphicsContext->PushDebugMarker(
                _jobData->m_passExecutionData.m_commandList,
                pass.m_name.m_string,
                ColorPalette::kWhite);

            const ResourceStateTracker::PassBarriers passBarriers =
                _jobData->m_renderGraph->m_resourceStateTracker->GetPassBarriers(i);
            const MemoryBarriers passEntryBarriers {
                .m_placementType = BarrierPlacementType::Consumer,
                .m_bufferBarriers = passBarriers.m_bufferMemoryBarriers,
                .m_textureBarriers = passBarriers.m_textureMemoryBarriers,
            };

            if (pass.m_prePassTransferFunction)
            {
                GraphicsContext* graphicsContext = _jobData->m_passExecutionData.m_graphicsContext;
#if defined(KE_FINAL)
                const TransferCommandEncoderHandle transferEncoder = graphicsContext->BeginTransferPass(
                    _jobData->m_passExecutionData.m_commandList, {}, {});
#else
                char name[256];
                snprintf(name, sizeof(name), "%s (Pre-pass transfer", pass.m_name.m_string.c_str());
                const TransferCommandEncoderHandle transferEncoder = graphicsContext->BeginTransferPass(
                    _jobData->m_passExecutionData.m_commandList, {}, name);
#endif
                pass.m_prePassTransferFunction(_jobData->m_passExecutionData.m_graphicsContext, transferEncoder);
                graphicsContext->EndTransferPass(transferEncoder);
            }

            if (pass.m_type == PassType::Render)
            {
                auto it = _jobData->m_renderGraph->m_renderPassCache.find(pass.m_renderPassHash.value());
                KE_ASSERT(it != _jobData->m_renderGraph->m_renderPassCache.end());

                // Render-pass entry barriers go through BeginRenderPass — placing them via
                // PlaceMemoryBarriers inside the encoder is illegal in Vulkan.
                _jobData->m_passExecutionData.m_renderEncoder =
                    _jobData->m_passExecutionData.m_graphicsContext->BeginRenderPass(
                        _jobData->m_passExecutionData.m_commandList, it->second,
                        passEntryBarriers,
                        pass.m_name.m_string);
            }
            else if (pass.m_type == PassType::Compute)
            {
                _jobData->m_passExecutionData.m_computeEncoder =
                    _jobData->m_passExecutionData.m_graphicsContext->BeginComputePass(
                        _jobData->m_passExecutionData.m_commandList,
                        passEntryBarriers,
                        pass.m_name.m_string);
            }
            else if (pass.m_type == PassType::Transfer)
            {
                _jobData->m_passExecutionData.m_transferEncoder =
                    _jobData->m_passExecutionData.m_graphicsContext->BeginTransferPass(
                        _jobData->m_passExecutionData.m_commandList,
                        passEntryBarriers,
                        pass.m_name.m_string);
            }

            {
                KE_GpuZoneScopedF(
                   _jobData->m_passExecutionData.m_graphicsContext,
                   _jobData->m_passExecutionData.m_graphicsContext->GetProfilerContext(),
                   _jobData->m_passExecutionData.m_commandList,
                   "%s",
                   pass.m_name.m_string.c_str());

                KE_ASSERT(pass.m_executeFunction != nullptr);
                pass.m_executeFunction(*_jobData->m_renderGraph, _jobData->m_passExecutionData);
            }

            if (pass.m_type == PassType::Render)
            {
                _jobData->m_passExecutionData.m_graphicsContext->EndRenderPass(
                    _jobData->m_passExecutionData.m_renderEncoder);
            }
            else if (pass.m_type == PassType::Compute)
            {
                _jobData->m_passExecutionData.m_graphicsContext->EndComputePass(
                    _jobData->m_passExecutionData.m_computeEncoder);
            }
            else if (pass.m_type == PassType::Transfer)
            {
                _jobData->m_passExecutionData.m_graphicsContext->EndTransferPass(
                    _jobData->m_passExecutionData.m_transferEncoder);
            }

            _jobData->m_passExecutionData.m_graphicsContext->PopDebugMarker(
                _jobData->m_passExecutionData.m_commandList);

            const std::chrono::time_point end = std::chrono::steady_clock::now();

            const u64 duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            _jobData->m_renderGraph->m_currentFramePassPerformance.find(pass.m_name)->second = duration;
            _jobData->m_renderGraph->m_currentFrameTotalDuration.fetch_add(duration, std::memory_order_relaxed);
        }

        _jobData->m_passExecutionData.m_graphicsContext->EndGraphicsCommandList(_jobData->m_passExecutionData.m_commandList);
    }

    RenderPassHandle RenderGraph::FetchRenderPass(
        GraphicsContext& _graphicsContext,
        PassDeclaration& _passDeclaration)
    {
        const u64 hash = _passDeclaration.GetRenderPassHash();

        const auto it = m_renderPassCache.find(hash);
        if (it != m_renderPassCache.end())
        {
            return it->second;
        }

        RenderPassDesc desc;
        for (const auto& attachment : _passDeclaration.m_colorAttachments)
        {
            desc.m_colorAttachments.push_back(RenderPassDesc::Attachment {
                .m_loadOperation = attachment.m_loadOperation,
                .m_storeOperation = attachment.m_storeOperation,
                .m_initialLayout = attachment.m_layoutBefore,
                .m_finalLayout = attachment.m_layoutAfter,
                .m_rtv = m_registry->GetRenderTargetView(attachment.m_rtv),
                .m_clearColor = attachment.m_clearColor,
            });
        }
        if (_passDeclaration.m_depthAttachment.has_value())
        {
            const PassAttachmentDeclaration attachment = _passDeclaration.m_depthAttachment.value();
            desc.m_depthStencilAttachment = RenderPassDesc::DepthStencilAttachment {
                .m_stencilLoadOperation = attachment.m_stencilLoadOperation,
                .m_stencilStoreOperation = attachment.m_stencilStoreOperation,
                .m_stencilClearValue = attachment.m_clearStencil,
            };
            desc.m_depthStencilAttachment.value().m_loadOperation = attachment.m_loadOperation;
            desc.m_depthStencilAttachment.value().m_storeOperation = attachment.m_storeOperation;
            desc.m_depthStencilAttachment.value().m_initialLayout = attachment.m_layoutBefore;
            desc.m_depthStencilAttachment.value().m_finalLayout = attachment.m_layoutAfter;
            desc.m_depthStencilAttachment.value().m_rtv = m_registry->GetRenderTargetView(attachment.m_rtv);
            desc.m_depthStencilAttachment.value().m_clearColor = float4(attachment.m_clearDepth, 0.0f, 0.0f, 0.0f);
            desc.m_depthStencilAttachment.value().m_readOnly = attachment.m_readOnly;
        }

#if !defined(KE_FINAL)
        desc.m_debugName = _passDeclaration.m_name.m_string;
#endif

        const RenderPassHandle handle = _graphicsContext.CreateRenderPass(desc);
        m_renderPassCache.emplace(hash, handle);
        return handle;
    }

    void RenderGraph::ResetRenderPassCache()
    {
        m_renderPassCache.clear();
    }
} // namespace KryneEngine::Modules::RenderGraph