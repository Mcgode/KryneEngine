/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#pragma once

#include <EASTL/vector.h>
#include <entt/entity/organizer.hpp>
#include <entt/entity/registry.hpp>
#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Allocators/StdAllocator.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>

namespace KryneEngine
{
    class FibersManager;
}

namespace KryneEngine::Modules::EnTT
{
    using Registry = entt::basic_registry<entt::entity, StdAllocator<entt::entity>>;
    using Organizer = entt::basic_organizer<Registry>;

    namespace Internal
    {
        template <class Get, class Exclude>
        struct ViewType;

        template <class... Get, class... Exclude>
        struct ViewType<entt::get_t<Get...>, entt::exclude_t<Exclude...>>
        {
            using Type = entt::basic_view<
                entt::get_t<Registry::storage_for_type<Get>...>,
                entt::exclude_t<Registry::storage_for_type<Exclude>...>>;
        };
    }

    /**
     * @brief View types matching `Registry` storages, to be used as system parameters.
     *
     * @details
     * `entt::view` can't be used with `Registry`, as it assumes storages use the default allocator.
     * Const components are read-only, non-const ones are read-write.
     */
    template <class... Get>
    using View = Internal::ViewType<entt::get_t<Get...>, entt::exclude_t<>>::Type;

    /// @brief `View` with excluded components, e.g. `FilteredView<entt::get_t<Position>, entt::exclude_t<const Sleeping>>`
    template <class Get, class Exclude>
    using FilteredView = Internal::ViewType<Get, Exclude>::Type;

    /**
     * @brief Owns an ECS registry, and the systems running on it, sorted by phase.
     *
     * @details
     * Phases are indices in `[0, phaseCount)`, each run individually by the user through `RunPhase()`. The order and
     * rate at which they are run is up to the caller (e.g. fixed-step phases around the physics step, variable-rate
     * ones once per frame), so the context imposes no ordering between phases.
     *
     * Within a phase, systems are ordered following EnTT's organizer rules: dependencies are deduced from the system
     * parameters' resource access (const = read-only, non-const = read-write, `Registry&` = exclusive sync point), and
     * declaration order decides between conflicting systems. Additional resources (e.g. context variables) can be
     * declared through `Req`.
     *
     * When a `FibersManager` is provided, each system is dispatched as a fiber job as soon as all its dependencies are
     * done. The finishing job runs one of the newly ready systems itself and queues the others. Without a
     * `FibersManager`, systems run sequentially in declaration order, which is a valid topological order. Either way,
     * a phase is a full barrier: `RunPhase()` only returns once all its systems are done.
     *
     * Threading contract:
     * - `AddSystem()` must not be called while a phase is running.
     * - `RunPhase()` calls must not overlap each other.
     * - Systems must only access what their signature (and `Req`) declares, as it is what concurrency relies on.
     */
    class Context
    {
    public:
        Context(AllocatorInstance _allocator, u16 _phaseCount, FibersManager* _fibersManager = nullptr);

        /**
         * @brief Declares a free function system.
         * @tparam System Function to run.
         * @tparam Req Additional requirements and/or resource access mode overrides.
         * @param _phase Phase to run the system in.
         * @param _name Optional name, used for profiling. Must outlive the context.
         */
        template <auto System, class... Req>
        void AddSystem(const u16 _phase, const char* _name = nullptr)
        {
            VERIFY_OR_RETURN_VOID(_phase < m_phases.Size());
            m_phases[_phase].m_organizer.template emplace<System, Req...>(_name);
            m_phases[_phase].m_dirty = true;
        }

        /**
         * @brief Declares a member function system, or a free function system with a payload.
         * @tparam System Function or member function to run.
         * @tparam Req Additional requirements and/or resource access mode overrides.
         * @param _phase Phase to run the system in.
         * @param _instance Instance or payload passed to the system. Must outlive the context.
         * @param _name Optional name, used for profiling. Must outlive the context.
         */
        template <auto System, class... Req, class T>
        void AddSystem(const u16 _phase, T& _instance, const char* _name = nullptr)
        {
            VERIFY_OR_RETURN_VOID(_phase < m_phases.Size());
            m_phases[_phase].m_organizer.template emplace<System, Req...>(_instance, _name);
            m_phases[_phase].m_dirty = true;
        }

        void RunPhase(u16 _phase);

        [[nodiscard]] u16 GetPhaseCount() const { return static_cast<u16>(m_phases.Size()); }

        [[nodiscard]] Registry& GetRegistry() { return m_registry; }
        [[nodiscard]] const Registry& GetRegistry() const { return m_registry; }

    private:
        using Graph = decltype(eastl::declval<const Organizer&>().graph());

        struct Phase
        {
            explicit Phase(AllocatorInstance _allocator);

            Organizer m_organizer {};
            Graph m_graph {};
            eastl::vector<u32> m_dependencyWorkVector;

            bool m_dirty = true;
        };

        void BuildGraph(Phase& _phase);

        static void RunSystem(const Organizer::vertex& _vertex, Registry& _registry);

        Registry m_registry;
        FibersManager* m_fibersManager;
        DynamicArray<Phase> m_phases;
    };
}
