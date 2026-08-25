/**
 * @file
 * @author Max Godefroy
 * @date 15/11/2024.
 */

#include "KryneEngine/Modules/RenderGraph/RenderGraph.hpp"

#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Profiling/TracyGpuScope.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Core/Graphics/ResourceViews/TextureView.hpp>
#include <KryneEngine/Core/Math/Color.hpp>

#include "KryneEngine/Modules/RenderGraph/Builder.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"
#include "KryneEngine/Modules/RenderGraph/Utils/ResourceStateTracker.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    RenderGraph::RenderGraph()
    {
        m_registry = eastl::make_unique<Registry>();
        m_resourceStateTracker = eastl::make_unique<ResourceStateTracker>();
    }

    RenderGraph::~RenderGraph() = default;

    Builder& RenderGraph::BeginFrame(GraphicsContext& _graphicsContext)
    {
        m_builder = eastl::make_unique<Builder>(GetRegistry());
        return *m_builder;
    }

    void RenderGraph::SubmitFrame(GraphicsContext& _graphicsContext, FibersManager* _fibersManager)
    {
        {
            KE_ZoneScoped("Build and cull render DAG (if not already done)");
            m_builder->BuildDag();
        }

        {
            KE_ZoneScoped("Prepare render jobs");

            m_resourceStateTracker->Process(*m_builder, *m_registry);

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
                    currentJob = &m_jobs.emplace_back();
                    initJobData(*currentJob, i);
                }

                currentJob->m_passRangeCount = i - currentJob->m_passRangeStart + 1;
                cumulativeDuration += estimatedPassDuration;

                // Make sure to cache the render pass.
                if (pass.m_type == PassType::Render)
                {
                    RenderPassHandle renderPass = FetchRenderPass(_graphicsContext, pass);
                    if (pass.m_renderPassCallback != nullptr)
                    {
                        pass.m_renderPassCallback(&_graphicsContext, renderPass);
                    }
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
                if (m_jobs.size() > 1)
                {
                    const SyncCounterId jobsCounter = _fibersManager->InitAndBatchJobs({
                        .m_function = [this](const u16 _jobIndex)
                        {
                            ExecuteJob(&m_jobs[_jobIndex], _jobIndex);
                        },
                        .m_jobCount = static_cast<u16>(m_jobs.size() - 1)
                    });
                    ExecuteJob(&m_jobs.back(), m_jobs.size() - 1);
                    _fibersManager->WaitForCounterAndReset(jobsCounter);
                }
                else if (!m_jobs.empty())
                {
                    ExecuteJob(&m_jobs.back(), m_jobs.size() - 1);
                }
            }
            else {
                for (u16 i = 0; i < m_jobs.size(); i++)
                {
                    ExecuteJob(&m_jobs[i], i);
                }
            }
            m_jobs.clear();
        }

        {
            KE_ZoneScoped("Cleanup");

            eastl::swap(m_previousFramePassPerformance, m_currentFramePassPerformance);
            m_currentFramePassPerformance.clear();

            m_previousFrameTotalDuration = m_currentFrameTotalDuration.load(std::memory_order_acquire);
            m_currentFrameTotalDuration.store(0, std::memory_order_relaxed);

            m_builder.reset();
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

            KE_GpuZoneScopedF(
                _jobData->m_passExecutionData.m_graphicsContext,
                _jobData->m_passExecutionData.m_graphicsContext->GetProfilerContext(),
                _jobData->m_passExecutionData.m_commandList,
                "%s",
                pass.m_name.m_string.c_str());

            const std::chrono::time_point start = std::chrono::steady_clock::now();
            _jobData->m_passExecutionData.m_graphicsContext->PushDebugMarker(
                _jobData->m_passExecutionData.m_commandList,
                pass.m_name.m_string,
                ColorPalette::kWhite);

            {
                const ResourceStateTracker::PassBarriers barriers = _jobData->m_renderGraph->m_resourceStateTracker->GetPassBarriers(i);
                if (!barriers.m_bufferMemoryBarriers.empty() || !barriers.m_textureMemoryBarriers.empty())
                {
                    KE_GpuZoneScoped(
                        _jobData->m_passExecutionData.m_graphicsContext,
                        _jobData->m_passExecutionData.m_graphicsContext->GetProfilerContext(),
                        _jobData->m_passExecutionData.m_commandList,
                        "Dispatching memory barriers");
                    _jobData->m_passExecutionData.m_graphicsContext->PlaceMemoryBarriers(
                        _jobData->m_passExecutionData.m_commandList,
                        {
                            .m_placementType = BarrierPlacementType::Consumer,
                            .m_bufferBarriers = barriers.m_bufferMemoryBarriers,
                            .m_textureBarriers = barriers.m_textureMemoryBarriers,
                        });
                }
            }

            if (pass.m_type == PassType::Render)
            {
                auto it = _jobData->m_renderGraph->m_renderPassCache.find(pass.m_renderPassHash.value());
                KE_ASSERT(it != _jobData->m_renderGraph->m_renderPassCache.end());

                _jobData->m_passExecutionData.m_renderEncoder =
                    _jobData->m_passExecutionData.m_graphicsContext->BeginRenderPass(
                        _jobData->m_passExecutionData.m_commandList,
                        it->second);
            }
            else if (pass.m_type == PassType::Compute)
            {
                _jobData->m_passExecutionData.m_graphicsContext->BeginComputePass(
                    _jobData->m_passExecutionData.m_commandList);
            }

            if (pass.m_type == PassType::Render && GraphicsContext::RenderPassNeedsUsageDeclaration()
                || pass.m_type == PassType::Compute && GraphicsContext::ComputePassNeedsUsageDeclaration())
            {
                _jobData->m_renderGraph->HandleResourceUsage(
                    _jobData->m_passExecutionData.m_graphicsContext,
                    _jobData->m_passExecutionData.m_commandList,
                    pass);
            }
            // TODO: handle usage as well when compute passes are set up

            KE_ASSERT(pass.m_executeFunction != nullptr);
            pass.m_executeFunction(*_jobData->m_renderGraph, _jobData->m_passExecutionData);

            if (pass.m_type == PassType::Render)
            {
                _jobData->m_passExecutionData.m_graphicsContext->EndRenderPass(
                    _jobData->m_passExecutionData.m_renderEncoder);
            }
            else if (pass.m_type == PassType::Compute)
            {
                _jobData->m_passExecutionData.m_graphicsContext->EndComputePass(
                    _jobData->m_passExecutionData.m_commandList);
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

    void RenderGraph::HandleResourceUsage(
        GraphicsContext* _graphicsContext,
        CommandListHandle _commandList,
        const PassDeclaration& _pass)
    {
        eastl::vector<TextureViewHandle> textureViews;
        eastl::vector<BufferViewHandle> bufferViews;

        const auto markUsage = [&](const auto& _dependencies, BufferViewAccessType _bufferAccessType, TextureViewAccessType _textureAccessType)
        {
            textureViews.clear();
            textureViews.reserve(_dependencies.size());

            bufferViews.clear();
            bufferViews.reserve(_dependencies.size());

            for (const auto& dependency : _dependencies)
            {
                const Resource& resource = m_registry->GetResource(dependency.m_resource);

                switch (resource.m_type)
                {
                case ResourceType::TextureView:
                    textureViews.push_back(resource.m_textureViewData.m_textureView);
                    break;
                case ResourceType::BufferView:
                    bufferViews.push_back(resource.m_bufferViewData.m_bufferView);
                    break;
                default:
                    KE_ERROR("Unhandled resource type");
                    break;
                }
            }

            _graphicsContext->DeclarePassTextureViewUsage(
                _commandList,
                textureViews,
                _textureAccessType);
            _graphicsContext->DeclarePassBufferViewUsage(
                _commandList,
                bufferViews,
                _bufferAccessType);
        };

        markUsage(
            _pass.m_readDependencies,
            BufferViewAccessType::Read,
            TextureViewAccessType::Read); // Don't differentiate between read and constant here.
        markUsage(
            _pass.m_writeDependencies,
            BufferViewAccessType::Write,
            TextureViewAccessType::Write);
    }
} // namespace KryneEngine::Modules::RenderGraph