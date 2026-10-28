/**
 * @file
 * @author Max Godefroy
 * @date 28/10/2026.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Platform/StdAlloc.hpp>
#include <KryneEngine/Modules/EnTT/Context.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Modules::EnTT
{
    namespace
    {
        class CountingAllocator final: public IAllocator
        {
        public:
            CountingAllocator(): IAllocator("CountingAllocator") {}

            void* Allocate(const size_t _size, const size_t _alignment) override
            {
                m_allocationCount++;
                m_liveAllocations++;
                return StdAlloc::MemAlign(_size, _alignment);
            }

            void Free(void* _ptr, size_t) override
            {
                m_liveAllocations--;
                StdAlloc::Free(_ptr);
            }

            size_t m_allocationCount = 0;
            size_t m_liveAllocations = 0;
        };

        struct Position
        {
            float m_x;
            float m_y;
        };
    }

    TEST(EnTTContext, RegistryUsesAllocator)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;
        CountingAllocator allocator;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            KryneEngine::Modules::EnTT::Context context(&allocator);
            auto& registry = context.GetRegistry();

            for (u32 i = 0; i < 100; i++)
            {
                const entt::entity entity = registry.create();
                registry.emplace<Position>(entity, static_cast<float>(i), 0.f);
                if (i % 2 == 0)
                {
                    registry.emplace<u32>(entity, i);
                }
            }

            u32 count = 0;
            for (auto [entity, position, value]: registry.view<Position, u32>().each())
            {
                EXPECT_EQ(position.m_x, static_cast<float>(value));
                count++;
            }
            EXPECT_EQ(count, 50);

            EXPECT_GT(allocator.m_allocationCount, 0);
            EXPECT_GT(allocator.m_liveAllocations, 0);
        }

        // -----------------------------------------------------------------------
        // Verify
        // -----------------------------------------------------------------------

        EXPECT_EQ(allocator.m_liveAllocations, 0);
        catcher.ExpectNoMessage();
    }
}
