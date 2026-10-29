/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#include "KryneEngine/Modules/EnTT/Context.hpp"

#include <EASTL/sort.h>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>

namespace KryneEngine::Modules::EnTT
{
    Context::Phase::Phase(const AllocatorInstance _allocator)
        : m_dependencyWorkVector(_allocator)
    {}

    Context::Context(const AllocatorInstance _allocator, const u16 _phaseCount, FibersManager* _fibersManager)
        : m_registry(StdAllocator<entt::entity>(_allocator))
        , m_fibersManager(_fibersManager)
        , m_phases(_allocator, _phaseCount)
    {
        m_phases.InitAll(_allocator);
    }

    void Context::RunPhase(const u16 _phase)
    {
        VERIFY_OR_RETURN_VOID(_phase < m_phases.Size());

        Phase& phase = m_phases[_phase];
        if (phase.m_dirty)
        {
            BuildGraph(phase);
        }

        if (phase.m_graph.empty())
        {
            return;
        }

        if (m_fibersManager == nullptr)
        {
            for (const auto& vertex: phase.m_graph)
            {
                // The organizer enforces a topological order, so we can run the systems sequentially.
                RunSystem(vertex, m_registry);
            }
        }
        else
        {
            for (size_t i = 0; i < phase.m_dependencyWorkVector.size(); i++)
            {
                phase.m_dependencyWorkVector[i] = phase.m_graph[i].in_edges().size();
            }

            struct CommonData
            {
                const Graph& m_graph;
                Registry& m_registry;
                eastl::vector<u32>& m_dependencyWorkVector;
                FibersManager* m_fibersManager;
                eastl::function<void(u16)> m_dispatch;
                SyncCounterId m_syncCounterId;
            } commonData {
                .m_graph = phase.m_graph,
                .m_registry = m_registry,
                .m_dependencyWorkVector = phase.m_dependencyWorkVector,
                .m_fibersManager = m_fibersManager,
            };

            constexpr auto priority = FiberJob::Priority::Medium;

            commonData.m_dispatch = [data = &commonData] (const u16 _jobIndex) -> void
            {
                u32 dispatchIndex = _jobIndex;
                // Use iterative design over recursion, to prevent stack overflow on fibers
                while (dispatchIndex != ~0u)
                {
                    RunSystem(data->m_graph[dispatchIndex], data->m_registry);

                    u32 firstDispatchIndex = ~0u;
                    for (const size_t o: data->m_graph[dispatchIndex].out_edges())
                    {
                        const u32 value = std::atomic_ref(data->m_dependencyWorkVector[o]).fetch_sub(1, std::memory_order_acq_rel);
                        if (value == 1) // Reached zero after sub
                        {
                            if (firstDispatchIndex == ~0u)
                            {
                                firstDispatchIndex = o;
                            }
                            else
                            {
                                SyncCounterId counter = data->m_syncCounterId;
                                KE_VERIFY(data->m_fibersManager->InitAndBatchJobs(
                                    {
                                        .m_function = data->m_dispatch,
                                        .m_jobCount = 1,
                                        .m_jobOffset = static_cast<u16>(o),
                                        .m_priority = priority,
                                    },
                                    counter) == counter);
                            }
                        }
                    }
                    dispatchIndex = firstDispatchIndex;
                }
            };

            commonData.m_syncCounterId = m_fibersManager->GetSyncCounterPool().AcquireCounter(1);

            for (size_t i = 1; i < phase.m_graph.size(); i++)
            {
                if (phase.m_graph[i].top_level())
                {
                    KE_VERIFY(m_fibersManager->InitAndBatchJobs(
                        {
                            .m_function = commonData.m_dispatch,
                            .m_jobCount = 1,
                            .m_jobOffset = static_cast<u16>(i),
                            .m_priority = priority,
                        },
                        commonData.m_syncCounterId) == commonData.m_syncCounterId);
                }
            }
            commonData.m_dispatch(0);
            m_fibersManager->GetSyncCounterPool().DecrementCounterValue(commonData.m_syncCounterId);
            m_fibersManager->WaitForCounterAndReset(commonData.m_syncCounterId);
        }

        // for (size_t level = 0; level + 1 < phase.m_levelOffsets.size(); level++)
        // {
        //     const u32 begin = phase.m_levelOffsets[level];
        //     const u32 count = phase.m_levelOffsets[level + 1] - begin;
        //
        //     if (m_fibersManager == nullptr || count == 1)
        //     {
        //         for (u32 i = begin; i < begin + count; i++)
        //         {
        //             RunSystem(phase.m_graph[phase.m_order[i]], m_registry);
        //         }
        //     }
        //     else
        //     {
        //         // Lives on the stack, which is fine as we wait for the batch below. Capturing a single pointer keeps
        //         // the job function within eastl::function's local storage.
        //         struct LevelData
        //         {
        //             const Graph& m_graph;
        //             const u32* m_order;
        //             Registry& m_registry;
        //         } levelData {
        //             .m_graph = phase.m_graph,
        //             .m_order = phase.m_order.data() + begin + 1,
        //             .m_registry = m_registry
        //         };
        //
        //         const SyncCounterId counter = m_fibersManager->InitAndBatchJobs({
        //             .m_function = [data = &levelData](const u16 _jobIndex)
        //             {
        //                 RunSystem(data->m_graph[data->m_order[_jobIndex]], data->m_registry);
        //             },
        //             .m_jobCount = static_cast<u16>(count - 1),
        //             .m_priority = FiberJob::Priority::High,
        //         });
        //         RunSystem(phase.m_graph[phase.m_order[begin]], m_registry); // Run first job on this fiber
        //         m_fibersManager->WaitForCounterAndReset(counter);
        //     }
        // }
    }

    void Context::BuildGraph(Phase& _phase)
    {
        KE_ZoneScopedFunction("Context::BuildGraph");

        _phase.m_graph = _phase.m_organizer.graph();
        _phase.m_dependencyWorkVector.resize(_phase.m_graph.size());

        // // Split the graph in levels with Kahn's algorithm: each level holds the vertices whose dependencies are all
        // // in previous levels.
        // eastl::vector<u32> remainingDependencies(vertexCount, m_phases.GetAllocator());
        // for (u32 i = 0; i < vertexCount; i++)
        // {
        //     remainingDependencies[i] = _phase.m_graph[i].in_edges().size();
        // }
        //
        // _phase.m_order.clear();
        // _phase.m_order.reserve(vertexCount);
        // _phase.m_levelOffsets.clear();
        //
        // for (u32 i = 0; i < vertexCount; i++)
        // {
        //     if (remainingDependencies[i] == 0)
        //     {
        //         _phase.m_order.push_back(i);
        //     }
        // }
        //
        // u32 levelBegin = 0;
        // while (levelBegin < _phase.m_order.size())
        // {
        //     const u32 levelEnd = _phase.m_order.size();
        //     _phase.m_levelOffsets.push_back(levelBegin);
        //
        //     for (u32 i = levelBegin; i < levelEnd; i++)
        //     {
        //         for (const size_t child: _phase.m_graph[_phase.m_order[i]].out_edges())
        //         {
        //             if (--remainingDependencies[child] == 0)
        //             {
        //                 _phase.m_order.push_back(child);
        //             }
        //         }
        //     }
        //
        //     // Keep declaration order within a level, for deterministic sequential execution.
        //     eastl::sort(_phase.m_order.begin() + levelEnd, _phase.m_order.end());
        //     levelBegin = levelEnd;
        // }
        // _phase.m_levelOffsets.push_back(_phase.m_order.size());
        //
        // KE_ASSERT_MSG(_phase.m_order.size() == vertexCount, "Cycle in the system dependency graph");

        // Create every storage the systems will access now, as lazily creating them while running systems concurrently
        // would be a data race on the registry's pool map.
        for (const Organizer::vertex& vertex: _phase.m_graph)
        {
            vertex.prepare(m_registry);
        }

        _phase.m_dirty = false;
    }

    void Context::RunSystem(const Organizer::vertex& _vertex, Registry& _registry)
    {
        KE_ZoneScopedF("%s", _vertex.name() != nullptr ? _vertex.name() : "Unnamed system");

        _vertex.callback()(_vertex.data(), _registry);
    }
}
