/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#include <atomic>
#include <EASTL/algorithm.h>
#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Platform/Cpu.hpp>
#include <KryneEngine/Core/Threads/FibersManager.hpp>
#include <KryneEngine/Modules/EnTT/Context.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::EnTT
{
    namespace
    {
        namespace Ecs = KryneEngine::Modules::EnTT;

        // See FibersManager_UnitTests.cpp: fiber threads are pinned to cores, so their count must not exceed the
        // usable core count.
        u16 ClampFiberThreadCount(const u16 _desired)
        {
            const u32 usableCores = Platform::GetUsableCpuCoreCount();
            return usableCores > 0
                ? eastl::min<u16>(_desired, static_cast<u16>(usableCores))
                : _desired;
        }

        struct Position { float m_value; };
        struct Velocity { float m_value; };
        struct Health { float m_value; };

        enum SystemId: u32
        {
            WritePosition,
            ComputeVelocity,
            WriteHealth,
            CheckVelocityAndHealth,
            Count
        };

        // Records the order in which systems ran, and checks they observed their dependencies' writes.
        struct Recorder
        {
            static constexpr u32 kNotRun = ~0u;

            std::atomic<u32> m_sequence = 0;
            std::atomic<u32> m_order[SystemId::Count] {};
            std::atomic<u32> m_mismatches = 0;
            std::atomic<u32> m_checkedEntities = 0;

            void Reset()
            {
                m_sequence = 0;
                for (auto& order: m_order)
                {
                    order = kNotRun;
                }
                m_mismatches = 0;
                m_checkedEntities = 0;
            }

            void Record(const SystemId _system)
            {
                m_order[_system] = m_sequence++;
            }

            void SystemWritePosition(Ecs::View<Position> _view)
            {
                for (auto [entity, position]: _view.each())
                {
                    position.m_value += 1.f;
                }
                Record(SystemId::WritePosition);
            }

            void SystemComputeVelocity(Ecs::View<const Position, Velocity> _view)
            {
                for (auto [entity, position, velocity]: _view.each())
                {
                    velocity.m_value = position.m_value * 2.f;
                }
                Record(SystemId::ComputeVelocity);
            }

            void SystemWriteHealth(Ecs::View<Health> _view)
            {
                for (auto [entity, health]: _view.each())
                {
                    health.m_value += 10.f;
                }
                Record(SystemId::WriteHealth);
            }

            void SystemCheckVelocityAndHealth(Ecs::View<const Position, const Velocity, const Health> _view)
            {
                for (auto [entity, position, velocity, health]: _view.each())
                {
                    if (velocity.m_value != position.m_value * 2.f || health.m_value != position.m_value * 10.f)
                    {
                        ++m_mismatches;
                    }
                    ++m_checkedEntities;
                }
                Record(SystemId::CheckVelocityAndHealth);
            }
        };

        void IncrementCounter(u32& _counter)
        {
            _counter++;
        }

        void RunDependencyScenario(FibersManager* _fibersManager)
        {
            // -----------------------------------------------------------------------
            // Setup
            // -----------------------------------------------------------------------

            ScopedAssertCatcher catcher;
            Recorder recorder;

            Ecs::Context context({}, 1, _fibersManager);

            // Declared out of dependency order on purpose: WriteHealth shares no resource with the first two systems.
            context.AddSystem<&Recorder::SystemWritePosition>(0, recorder, "WritePosition");
            context.AddSystem<&Recorder::SystemComputeVelocity>(0, recorder, "ComputeVelocity");
            context.AddSystem<&Recorder::SystemWriteHealth>(0, recorder, "WriteHealth");
            context.AddSystem<&Recorder::SystemCheckVelocityAndHealth>(0, recorder, "CheckVelocityAndHealth");

            constexpr u32 kEntityCount = 256;
            Ecs::Registry& registry = context.GetRegistry();
            for (u32 i = 0; i < kEntityCount; i++)
            {
                const entt::entity entity = registry.create();
                registry.emplace<Position>(entity, 0.f);
                registry.emplace<Velocity>(entity, 0.f);
                registry.emplace<Health>(entity, 0.f);
            }

            // -----------------------------------------------------------------------
            // Execute & verify
            // -----------------------------------------------------------------------

            constexpr u32 kIterations = 50;
            for (u32 i = 0; i < kIterations; i++)
            {
                recorder.Reset();
                context.RunPhase(0);

                for (const auto& order: recorder.m_order)
                {
                    ASSERT_NE(order.load(), Recorder::kNotRun);
                }
                EXPECT_LT(recorder.m_order[SystemId::WritePosition], recorder.m_order[SystemId::ComputeVelocity]);
                EXPECT_LT(recorder.m_order[SystemId::ComputeVelocity], recorder.m_order[SystemId::CheckVelocityAndHealth]);
                EXPECT_LT(recorder.m_order[SystemId::WriteHealth], recorder.m_order[SystemId::CheckVelocityAndHealth]);

                EXPECT_EQ(recorder.m_checkedEntities, kEntityCount);
                EXPECT_EQ(recorder.m_mismatches, 0);
            }

            catcher.ExpectNoMessage();
        }
    }

    TEST(EnTTContextPhases, DependencyOrderSequential)
    {
        RunDependencyScenario(nullptr);
    }

    TEST(EnTTContextPhases, DependencyOrderFibers)
    {
        AllocatorInstance allocator {};
        FibersManager fibersManager(ClampFiberThreadCount(4), allocator);

        RunDependencyScenario(&fibersManager);
    }

    TEST(EnTTContextPhases, EmptyPhaseFibers)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        AllocatorInstance allocator {};
        FibersManager fibersManager(ClampFiberThreadCount(4), allocator);

        u32 counter = 0;
        Ecs::Context context({}, 2, &fibersManager);
        context.AddSystem<&IncrementCounter>(1, counter, "Increment");

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        context.RunPhase(0);
        EXPECT_EQ(counter, 0);

        // The empty phase must leave the fibers manager usable
        context.RunPhase(1);
        EXPECT_EQ(counter, 1);

        catcher.ExpectNoMessage();
    }

    TEST(EnTTContextPhases, PhasesRunIndividually)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        u32 counterA = 0;
        u32 counterB = 0;

        Ecs::Context context({}, 2);
        context.AddSystem<&IncrementCounter>(0, counterA, "IncrementA");
        context.AddSystem<&IncrementCounter>(1, counterB, "IncrementB");

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        context.RunPhase(1);
        EXPECT_EQ(counterA, 0);
        EXPECT_EQ(counterB, 1);

        context.RunPhase(0);
        context.RunPhase(0);
        EXPECT_EQ(counterA, 2);
        EXPECT_EQ(counterB, 1);

        catcher.ExpectNoMessage();
    }

    TEST(EnTTContextPhases, SystemsAddedAfterRunAreScheduled)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        u32 counterA = 0;
        u32 counterB = 0;

        Ecs::Context context({}, 1);
        context.AddSystem<&IncrementCounter>(0, counterA, "IncrementA");

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        context.RunPhase(0);
        context.AddSystem<&IncrementCounter>(0, counterB, "IncrementB");
        context.RunPhase(0);

        EXPECT_EQ(counterA, 2);
        EXPECT_EQ(counterB, 1);

        catcher.ExpectNoMessage();
    }

    TEST(EnTTContextPhases, InvalidPhase)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        u32 counter = 0;
        Ecs::Context context({}, 1);

        // -----------------------------------------------------------------------
        // Execute & verify
        // -----------------------------------------------------------------------

        context.AddSystem<&IncrementCounter>(1, counter, "OutOfRange");
        catcher.ExpectMessageCount(1);

        context.RunPhase(1);
        catcher.ExpectMessageCount(2);

        context.RunPhase(0);
        EXPECT_EQ(counter, 0);
    }
}
