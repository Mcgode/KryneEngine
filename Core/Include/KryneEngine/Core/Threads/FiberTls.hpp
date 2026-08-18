/**
 * @file
 * @author Max Godefroy
 * @date 23/04/2022.
 */

#pragma once

#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace KryneEngine
{
    class FibersManager;

    template<class T, class Allocator = AllocatorInstance>
    struct FiberTls
    {
    public:
        using Type = T;
        using Ptr = typename DynamicArray<T>::Ptr;
        using Ref = typename DynamicArray<T>::Ref;

        explicit FiberTls(Allocator _allocator)
            : m_array(_allocator)
        {}

        void Init(const FibersManager *_fibersManager, const T &_value);

        template <typename F>
        void InitFunc(const FibersManager* _fibersManager, F _initFunction);

        void InitDefault(const FibersManager* _fibersManager);

        [[nodiscard]] T& Load();

        [[nodiscard]] inline T& Load(u16 _fiberIndex)
        {
            return m_array[_fiberIndex];
        }

    private:
        DynamicArray<T, Allocator> m_array {};
    };
} // KryneEngine