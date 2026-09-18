/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#include "KryneEngine/Core/Memory/IntrusivePtr.hpp"
#include <gtest/gtest.h>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    namespace
    {
        // Note: allocator access is exposed via GetAllocator() (IsAllocatorGetterIntrusible) rather than a
        // plain public m_allocator member (IsAllocatorVarIntrusible), since the latter concept's compound
        // requirement `{ T::m_allocator } -> std::same_as<AllocatorInstance>` can never be satisfied by a
        // regular data member (member access is always an lvalue, so decltype((T::m_allocator)) deduces to
        // AllocatorInstance&, never AllocatorInstance). That is a separate, pre-existing issue outside the
        // scope of this regression test.
        struct IntrusiveUniqueTestObject
        {
            u32 m_value;

            static inline std::atomic<u32> s_instanceCount = 0;

            explicit IntrusiveUniqueTestObject(const AllocatorInstance _allocator, const u32 _value = 0)
                : m_value(_value)
                , m_allocatorInstance(_allocator)
            {
                s_instanceCount.fetch_add(1, std::memory_order::relaxed);
            }

            ~IntrusiveUniqueTestObject()
            {
                s_instanceCount.fetch_sub(1, std::memory_order::relaxed);
            }

            [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocatorInstance; }

        private:
            AllocatorInstance m_allocatorInstance;
        };

        struct IntrusiveSharedTestObject
        {
            s64 m_refCount = 0;
            u32 m_value;

            static inline std::atomic<u32> s_instanceCount = 0;

            explicit IntrusiveSharedTestObject(const AllocatorInstance _allocator, const u32 _value = 0)
                : m_value(_value)
                , m_allocatorInstance(_allocator)
            {
                s_instanceCount.fetch_add(1, std::memory_order::relaxed);
            }

            ~IntrusiveSharedTestObject()
            {
                s_instanceCount.fetch_sub(1, std::memory_order::relaxed);
            }

            [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocatorInstance; }

        private:
            AllocatorInstance m_allocatorInstance;
        };
    }

    TEST(IntrusivePtr, MakeIntrusiveUniquePtr_ReturnsUsableObject)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        IntrusiveUniqueTestObject::s_instanceCount = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        IntrusiveUniquePtr<IntrusiveUniqueTestObject> ptr =
            MakeIntrusiveUniquePtr<IntrusiveUniqueTestObject>(AllocatorInstance {}, 42u);

        ASSERT_NE(ptr.Get(), nullptr);
        EXPECT_EQ(ptr->m_value, 42u);
        EXPECT_EQ(IntrusiveUniqueTestObject::s_instanceCount, 1u);

        ptr.Reset();
        EXPECT_EQ(IntrusiveUniqueTestObject::s_instanceCount, 0u);

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }

    TEST(IntrusivePtr, MakeIntrusiveSharedPtr_ReturnsUsableObject)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        IntrusiveSharedTestObject::s_instanceCount = 0;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        IntrusiveSharedPtr<IntrusiveSharedTestObject> ptr =
            MakeIntrusiveSharedPtr<IntrusiveSharedTestObject>(AllocatorInstance {}, 7u);

        ASSERT_NE(ptr.Get(), nullptr);
        EXPECT_EQ(ptr->m_value, 7u);
        EXPECT_EQ(ptr->m_refCount, 1);
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 1u);

        IntrusiveSharedPtr<IntrusiveSharedTestObject> copy = ptr;
        EXPECT_EQ(ptr->m_refCount, 2);
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 1u);

        ptr.Reset();
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 1u);

        copy.Reset();
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 0u);

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }
}
