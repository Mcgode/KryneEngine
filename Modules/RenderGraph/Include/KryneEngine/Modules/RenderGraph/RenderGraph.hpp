/**
 * @file
 * @author Max Godefroy
 * @date 15/11/2024.
 */

#pragma once

#include <EASTL/hash_map.h>
#include <EASTL/unique_ptr.h>

#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"

namespace KryneEngine
{
    class GraphicsContext;
    class FibersManager;
}

namespace KryneEngine::Modules::RenderGraph
{
    class Builder;
    class Registry;
    class ResourceStateTracker;

    class RenderGraph
    {
    public:
        RenderGraph();
        ~RenderGraph();

        [[nodiscard]] Registry& GetRegistry() const { return *m_registry; }
        [[nodiscard]] Builder& GetBuilder() const { return *m_builder; }

        [[nodiscard]] Builder& BeginFrame(GraphicsContext& _graphicsContext);
        void SubmitFrame(GraphicsContext& _graphicsContext, FibersManager* _fibersManager);

        [[nodiscard]] double GetTargetTimePerCommandList() const { return m_targetTimePerCommandList; }
        void SetTargetTimePerCommandList(double _milliseconds) { m_targetTimePerCommandList = _milliseconds; }

        RenderPassHandle FetchRenderPass(GraphicsContext& _graphicsContext, PassDeclaration& _passDeclaration);
        void ResetRenderPassCache();

    private:
        eastl::unique_ptr<Registry> m_registry;
        eastl::unique_ptr<Builder> m_builder;

        double m_targetTimePerCommandList = 1.0;

        struct JobData
        {
            RenderGraph* m_renderGraph;
            PassExecutionData m_passExecutionData;
            u32 m_passRangeStart;
            u32 m_passRangeCount;
        };
        eastl::vector<JobData> m_jobs;

        eastl::hash_map<StringHash, u64> m_previousFramePassPerformance;
        eastl::hash_map<StringHash, u64> m_currentFramePassPerformance;
        u64 m_previousFrameTotalDuration = 0;
        std::atomic<u64> m_currentFrameTotalDuration = 0;

        eastl::hash_map<u64, RenderPassHandle> m_renderPassCache;

        eastl::unique_ptr<ResourceStateTracker> m_resourceStateTracker;

        static void ExecuteJob(JobData* _jobData, u16 _jobIndex);
    };
} // namespace KryneEngine::Modules::RenderGraph
