/**
 * @file
 * @author Max Godefroy
 * @date 18/03/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    /**
     * @brief A simple stack allocator that must be manually popped.
     *
     * @details
     * This is an alternative version of a bump allocator, where you can "pop" allocations during execution, instead of
     * needing to clear the entire allocation heap each time.
     * This allocator is particularly useful as a scoped scratch allocator: at the beginning of your scope, you save the
     * current stack index, allocate scratch memory during its execution, and then pop all the allocations at the end of
     * the scope by going back to the saved stack index.
     *
     * The allocator can use a series of heaps that double in size compared to the previous one. These are lazily
     * allocated to limit excessive memory usage.
     *
     * @warning This allocator assumes that the program correctly manages the stack popping / clearing. If not handled
     * properly, this will result in memory leaks.
     */
    class StackAllocator: public IAllocator
    {
    public:
        StackAllocator(
            AllocatorInstance _parentAllocator,
            size_t _initialSize,
            size_t _maxExtraHeapCount = 5,
            const char* _name = "StackScratchAllocator");

        void* Allocate(size_t _size, size_t _alignment) override;

        void Free(void* _ptr, size_t _alignment) override {}

        [[nodiscard]] size_t GetStackIndex() const { return m_stackIndex; }
        void Pop(size_t _toIndex);

        KE_FORCEINLINE void Clear() { Pop(0); }

        /**
         * @brief Clears the allocator and deallocates the extra heaps.
         */
        void Reset();

        struct Scope
        {
            friend class StackAllocator;
        private:
            StackAllocator* m_allocator;
            size_t m_stackIndex;

            explicit Scope(StackAllocator* _allocator)
                : m_allocator(_allocator)
                , m_stackIndex(_allocator->GetStackIndex())
            {}

        public:
            ~Scope() { m_allocator->Pop(m_stackIndex); }

            [[nodiscard]] AllocatorInstance GetAllocator() const { return m_allocator; }
        };

        [[nodiscard]] Scope Scoped() { return Scope(this); }

    private:
        AllocatorInstance m_parentAllocator;
        size_t m_stackIndex = 0;
        size_t m_baseHeapSize;
        DynamicArray<std::byte*> m_heaps;
    };
}