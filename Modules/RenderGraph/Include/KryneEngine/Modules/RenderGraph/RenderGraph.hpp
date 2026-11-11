/**
 * @file
 * @author Max Godefroy
 * @date 15/11/2024.
 */

#pragma once

#include <EASTL/hash_map.h>

#include "KryneEngine/Modules/RenderGraph/Declarations/PassDeclaration.hpp"

namespace KryneEngine
{
    class GraphicsContext;
    class FibersManager;
    class StackAllocator;
}

namespace KryneEngine::Modules::RenderGraph
{
    class Builder;
    class Registry;
    class ResourceStateTracker;

    class RenderGraph
    {
    public:
        explicit RenderGraph(AllocatorInstance _allocator);
        ~RenderGraph();

        [[nodiscard]] Registry& GetRegistry() const { return *m_registry; }
        [[nodiscard]] Builder& GetBuilder() const { return *m_builder; }

        [[nodiscard]] Builder& BeginFrame();
        void SubmitFrame(GraphicsContext& _graphicsContext, FibersManager* _fibersManager);

        [[nodiscard]] double GetTargetTimePerCommandList() const { return m_targetTimePerCommandList; }
        void SetTargetTimePerCommandList(double _milliseconds) { m_targetTimePerCommandList = _milliseconds; }

        /// @brief Destroys every cached render pass. They must not be in use by any pending frame.
        void ResetRenderPassCache(GraphicsContext& _graphicsContext);

    private:
        static constexpr size_t kScratchAllocatorSize = 32 << 10; // 32 KiB

        AllocatorInstance m_allocator;
        StackAllocator* m_scratchAllocator;

        Registry* m_registry;
        ResourceStateTracker* m_resourceStateTracker;
        Builder* m_builder = nullptr;

        double m_targetTimePerCommandList = 1.0;

        struct JobData
        {
            RenderGraph* m_renderGraph = nullptr;
            PassExecutionData m_passExecutionData {};
            u32 m_passRangeStart {};
            u32 m_passRangeCount {};
        };

        eastl::hash_map<StringHash, u64> m_previousFramePassPerformance;
        eastl::hash_map<StringHash, u64> m_currentFramePassPerformance;
        u64 m_previousFrameTotalDuration = 0;
        std::atomic<u64> m_currentFrameTotalDuration = 0;

        struct CachedRenderPass
        {
            RenderPassHandle m_handle;
            u64 m_lastUsedFrameId;
        };

        /// @brief A cached render pass unused for this many frames is destroyed once its last frame has executed.
        /// Must stay above the frame context count, so per-swap-chain-image passes are not evicted between uses.
        static constexpr u64 kRenderPassEvictionFrameCount = 8;

        eastl::hash_map<u64, CachedRenderPass> m_renderPassCache;

        RenderPassHandle FetchRenderPass(GraphicsContext& _graphicsContext, PassDeclaration& _passDeclaration);

        void EvictUnusedRenderPasses(GraphicsContext& _graphicsContext);

        static void ExecuteJob(JobData* _jobData, u16 _jobIndex);
    };
} // namespace KryneEngine::Modules::RenderGraph
