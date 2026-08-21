/**
 * @file
 * @author Max Godefroy
 * @date 21/08/2026.
 */

#pragma once


#include <atomic>

#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Threads/HelperFunctions.hpp"


namespace KryneEngine
{
    /**
     * @brief A single-producer single-consumer fixed-capacity lock-free queue.
     *
     * @details
     * This queue is optimized for single-producer single-consumer scenarios, where one thread
     * is responsible for pushing elements into the queue and another thread is responsible for
     * popping elements from the queue. It uses atomic operations to ensure thread safety and
     * provides a fast and efficient way to manage a queue of elements.
     *
     * The data retrieval and popping operations are separated to allow the consumer thread to stagger both operations
     * if they have a need to. For instance, the consumer first retrieves some work and only pops the front element when
     * done processing it.

     *
     * @note
     * Do not use more than one producer or consumer thread with this queue, as it will break the thread safety.
     *
     * @tparam T The type of elements stored in the queue.
     * @tparam NoFalseSharing Avoid unnecessary cache invalidation from atomic operations between unrelated fields, at
     * the expense of increased memory usage.
     */
    template <class T, bool NoFalseSharing = false>
    class SpscQueue
    {
    public:
        explicit SpscQueue(const AllocatorInstance _allocator, const size_t _capacity)
            : m_ringBuffer(_allocator, _capacity)
        {}

        template<class... Args>
        bool TryEmplace(Args... _args)
        {
            const size_t tail = m_tail.load(std::memory_order::relaxed);
            const size_t nextTail = (tail + 1) % m_ringBuffer.Size();

            if (nextTail == m_head.load(std::memory_order::acquire))
                return false;

            m_ringBuffer.Init(tail, _args...);
            m_tail.store(nextTail, std::memory_order::release);

            return true;
        }

        T* Front()
        {
            const size_t head = m_head.load(std::memory_order::relaxed);
            if (head == m_tail.load(std::memory_order::relaxed))
                return nullptr;
            return &m_ringBuffer[head];
        }

        void Pop(const bool _ensureSafe = false)
        {
            const size_t head = m_head.load(std::memory_order::relaxed);
            if (_ensureSafe)
            {
                const size_t tail = m_tail.load(std::memory_order::relaxed);
                if (head == tail)
                    return;
            }
            m_ringBuffer[head].~T();
            m_head.store((head + 1) % m_ringBuffer.Size(), std::memory_order::release);
        }

        bool Empty() const
        {
            return m_head.load(std::memory_order::acquire) == m_tail.load(std::memory_order::acquire);
        }

    private:
        DynamicArray<T> m_ringBuffer;

        static constexpr size_t kAtomicAlignment = NoFalseSharing
            ? Threads::kCacheLineSize
            : alignof(std::atomic<size_t>);

        alignas(kAtomicAlignment) std::atomic<size_t> m_head { 0 };
        alignas(kAtomicAlignment) std::atomic<size_t> m_tail { 0 };
    };
}