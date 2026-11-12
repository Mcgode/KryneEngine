/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#include "KryneEngine/Core/Memory/IntrusivePtr.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <mutex>
#include <thread>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests
{
    namespace
    {
        // Exercises IsAllocatorVarIntrusible (bare public m_allocator member), while
        // IntrusiveSharedTestObject below exercises IsAllocatorGetterIntrusible (GetAllocator()).
        struct IntrusiveUniqueTestObject
        {
            u32 m_value;
            AllocatorInstance m_allocator;

            static inline std::atomic<u32> s_instanceCount = 0;

            explicit IntrusiveUniqueTestObject(const AllocatorInstance _allocator, const u32 _value = 0)
                : m_value(_value)
                , m_allocator(_allocator)
            {
                s_instanceCount.fetch_add(1, std::memory_order::relaxed);
            }

            ~IntrusiveUniqueTestObject()
            {
                s_instanceCount.fetch_sub(1, std::memory_order::relaxed);
            }
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

        // Published in a static pointer while alive. Its destructor unpublishes it under `s_lock`, so a reader holding
        // that lock can safely call `IntrusiveSharedPtr::TryAcquire()` on the published pointer.
        struct PublishedSharedTestObject
        {
            s32 m_refCount = 0;

            static inline std::mutex s_lock;
            static inline PublishedSharedTestObject* s_published = nullptr;
            static inline std::atomic<u32> s_instanceCount = 0;

            explicit PublishedSharedTestObject(const AllocatorInstance _allocator)
                : m_allocatorInstance(_allocator)
            {
                s_instanceCount.fetch_add(1, std::memory_order::relaxed);
            }

            ~PublishedSharedTestObject()
            {
                const std::lock_guard lock(s_lock);
                if (s_published == this)
                    s_published = nullptr;
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

    TEST(IntrusivePtr, IntrusiveSharedPtr_MoveAssignmentTransfersReference)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        IntrusiveSharedTestObject::s_instanceCount = 0;

        IntrusiveSharedPtr<IntrusiveSharedTestObject> source =
            MakeIntrusiveSharedPtr<IntrusiveSharedTestObject>(AllocatorInstance {}, 1u);
        IntrusiveSharedPtr<IntrusiveSharedTestObject> destination =
            MakeIntrusiveSharedPtr<IntrusiveSharedTestObject>(AllocatorInstance {}, 2u);
        IntrusiveSharedTestObject* object = source.Get();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // The destination releases its own object, and takes over the source's reference without adding one
        destination = std::move(source);
        EXPECT_EQ(source.Get(), nullptr);
        EXPECT_EQ(destination.Get(), object);
        EXPECT_EQ(object->m_refCount, 1);
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 1u);

        destination.Reset();
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 0u);

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }

    TEST(IntrusivePtr, TryAcquire_TakesReferencesUntilReleased)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        IntrusiveSharedTestObject::s_instanceCount = 0;
        const AllocatorInstance allocator {};

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        EXPECT_EQ(IntrusiveSharedPtr<IntrusiveSharedTestObject>::TryAcquire(nullptr).Get(), nullptr);

        // A new object has no reference yet, but can still be acquired
        auto* object = allocator.New<IntrusiveSharedTestObject>(allocator, 3u);
        ASSERT_EQ(object->m_refCount, 0);

        IntrusiveSharedPtr<IntrusiveSharedTestObject> first = IntrusiveSharedPtr<IntrusiveSharedTestObject>::TryAcquire(object);
        ASSERT_EQ(first.Get(), object);
        EXPECT_EQ(object->m_refCount, 1);

        IntrusiveSharedPtr<IntrusiveSharedTestObject> second = IntrusiveSharedPtr<IntrusiveSharedTestObject>::TryAcquire(object);
        ASSERT_EQ(second.Get(), object);
        EXPECT_EQ(object->m_refCount, 2);

        // Releasing one reference keeps the object alive
        first.Reset();
        EXPECT_EQ(object->m_refCount, 1);
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 1u);

        // Releasing the last one deletes it
        second.Reset();
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 0u);

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }

    TEST(IntrusivePtr, TryAcquire_FailsOnReleasedObject)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        IntrusiveSharedTestObject::s_instanceCount = 0;
        const AllocatorInstance allocator {};

        // An object whose last reference was released, but not deleted yet (as while another thread deletes it)
        auto* object = allocator.New<IntrusiveSharedTestObject>(allocator);
        object->m_refCount = std::numeric_limits<decltype(object->m_refCount)>::min();

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        const IntrusiveSharedPtr<IntrusiveSharedTestObject> ptr =
            IntrusiveSharedPtr<IntrusiveSharedTestObject>::TryAcquire(object);
        EXPECT_EQ(ptr.Get(), nullptr);
        EXPECT_EQ(object->m_refCount, std::numeric_limits<decltype(object->m_refCount)>::min());

        allocator.Delete(object);
        EXPECT_EQ(IntrusiveSharedTestObject::s_instanceCount, 0u);

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }

    TEST(IntrusivePtr, TryAcquire_RacesWithLastRelease)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        PublishedSharedTestObject::s_instanceCount = 0;
        const AllocatorInstance allocator {};

        constexpr u32 kIterations = 5000;

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        // The owner drops the last reference without any lock, while a reader acquires the published object under the
        // lock its destructor takes, as a resource manager would. Whatever the interleaving, the reader either gets a
        // live object or nothing, and the object is deleted exactly once.
        for (u32 i = 0; i < kIterations; i++)
        {
            PublishedSharedTestObject::s_published = allocator.New<PublishedSharedTestObject>(allocator);
            IntrusiveSharedPtr<PublishedSharedTestObject> owner { PublishedSharedTestObject::s_published };

            std::thread releaser([&owner] { owner.Reset(); });

            IntrusiveSharedPtr<PublishedSharedTestObject> reader;
            {
                const std::lock_guard lock(PublishedSharedTestObject::s_lock);
                reader = IntrusiveSharedPtr<PublishedSharedTestObject>::TryAcquire(
                    PublishedSharedTestObject::s_published);
            }
            releaser.join();

            if (reader.Get() != nullptr)
            {
                EXPECT_EQ(PublishedSharedTestObject::s_instanceCount, 1u);
                EXPECT_EQ(reader->m_refCount, 1);
            }
            reader.Reset();

            ASSERT_EQ(PublishedSharedTestObject::s_instanceCount, 0u) << "Object leaked or deleted twice";
            ASSERT_EQ(PublishedSharedTestObject::s_published, nullptr);
        }

        ASSERT_TRUE(catcher.GetCaughtMessages().empty());
    }
}
