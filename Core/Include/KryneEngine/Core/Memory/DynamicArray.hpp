/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#pragma once

#include <EASTL/initializer_list.h>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/Assert.hpp"
#include "KryneEngine/Core/Memory/Allocators/Allocator.hpp"

namespace KryneEngine
{
    template<class T, class Allocator = AllocatorInstance>
    class DynamicArray final
    {
    public:
        using Ptr = T*;
        using Ref = T&;
        using Iterator = Ptr;

        DynamicArray() = default;

        explicit DynamicArray(const Allocator& _allocator): m_allocator(_allocator) {}

        explicit DynamicArray(size_t _count)
        {
            Resize(_count);
        }

        DynamicArray(const Allocator& _allocator, size_t _count)
            : m_allocator(_allocator)
        {
            Resize(_count);
        }

        DynamicArray(size_t _count, const T& _value)
        {
            Resize(_count);
            SetAll(_value);
        }

        DynamicArray(const Allocator& _allocator, size_t _count, const T& _value)
            : m_allocator(_allocator)
        {
            Resize(_count);
            SetAll(_value);
        }

        DynamicArray(const std::initializer_list<T>& _initializerList)
        {
            Resize(_initializerList.size());

            auto it = _initializerList.begin();
            for (Ptr valuePtr = begin(); valuePtr != end(); valuePtr++, it++)
            {
                *valuePtr = *it;
            }
        }

        DynamicArray(const Allocator& _allocator, const std::initializer_list<T>& _initializerList)
            : m_allocator(_allocator)
        {
            Resize(_initializerList.size());

            auto it = _initializerList.begin();
            for (Ptr valuePtr = begin(); valuePtr != end(); valuePtr++, it++)
            {
                *valuePtr = *it;
            }
        }

        DynamicArray(const DynamicArray& _other)
        {
            m_allocator = _other.m_allocator;
            Resize(_other.m_count);
            for (u64 i = 0; i < m_count; i++) { m_array[i] = _other.m_array[i]; }
        }

        DynamicArray& operator=(const DynamicArray& _other)
        {
            // Clear first, to make sure the original buffer is deallocate from the proper allocator
            Clear();
            m_allocator = _other.m_allocator;
            Resize(_other.m_count);
            // No memcpy() to explicitly call copy constructors of entries
            // We use copy constructors instead of operators since the array memory is non-zero'ed and uninitialized.
            for (size_t i = 0; i < m_count; i++) { new (m_array + i) T(_other.m_array[i]); }
            return *this;
        }

        DynamicArray(DynamicArray&& _other) noexcept
        {
            m_allocator = _other.m_allocator;
            m_count = _other.m_count;
            m_array = _other.m_array;

            _other.m_allocator = Allocator();
            _other.m_count = 0;
            _other.m_array = nullptr;
        }

        DynamicArray& operator=(DynamicArray&& _other) noexcept
        {
            m_allocator = _other.m_allocator;
            m_count = _other.m_count;
            m_array = _other.m_array;

            _other.m_allocator = Allocator();
            _other.m_count = 0;
            _other.m_array = nullptr;

            return *this;
        }

        ~DynamicArray()
        {
            Resize(0);
        }

        [[nodiscard]] size_t Size() const
        {
            return m_count;
        }

        [[nodiscard]] bool Empty() const
        {
            return m_count == 0;
        }

        [[nodiscard]] const Allocator& GetAllocator() const { return m_allocator; }
        [[nodiscard]] Allocator& GetAllocator() { return m_allocator; }
        void SetAllocator(const Allocator& _allocator) { m_allocator = _allocator; }

        void Resize(size_t _count)
        {
            if (m_array != nullptr)
            {
                Clear();
            }

            m_count = _count;

            if (m_count > 0)
            {
                const size_t size = sizeof(T) * m_count;
                m_array = static_cast<Ptr>(m_allocator.allocate(size, alignof(T)));
                KE_ASSERT(m_array != nullptr);
            }
        }

        template<class... Args>
        Ptr Init(u64 _index, Args... _args)
        {
            IF_NOT_VERIFY_MSG(_index < m_count, "Beyond max index!")
                return nullptr;
            Ptr memSpace = m_array + _index;
            return new (memSpace) T(_args...);
        }

        template<class... Args>
        void InitAll(Args... _args)
        {
            for (Ptr valuePtr = begin(); valuePtr != end(); valuePtr++)
            {
                new (valuePtr) T(_args...);
            }
        }

        void SetAll(const T& _value)
        {
            for (auto& value: *this)
            {
                value = _value;
            }
        }

        Ref operator[](size_t _index) const
        {
            KE_ASSERT_MSG(_index < m_count, "Beyond max index!");
            return m_array[_index];
        }

        Iterator begin() const
        {
            return m_array;
        }

        Iterator end() const
        {
            return m_array + m_count;
        }

        void Clear()
        {
            for (Ref instance: *this)
            {
                instance.~T();
            }
            ResetLooseMemory();
        }

        void ResetLooseMemory()
        {
            m_allocator.deallocate(m_array, m_count * sizeof(T));
            m_array = nullptr;
            m_count = 0;
        }

        Ptr Data()
        {
            return m_array;
        }

        [[nodiscard]] const Ptr Data() const
        {
            return m_array;
        }

    private:
        Ptr m_array = nullptr;
        size_t m_count = 0;
        Allocator m_allocator;
    };
}
