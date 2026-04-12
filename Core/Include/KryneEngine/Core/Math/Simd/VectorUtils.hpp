/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Math/Quaternion.hpp"
#include "KryneEngine/Core/Math/Simd/SimdMemoryOperations.hpp"
#include "KryneEngine/Core/Math/Vector.hpp"

namespace KryneEngine::Simd
{
    template <class T>
    using MatchingVec = std::conditional_t<std::is_same_v<T, float>, f32x4,
        std::conditional_t<std::is_same_v<T, u32>, u32x4,
        std::conditional_t<std::is_same_v<T, s32>, s32x4, void>>>;

    template <class T>
    MatchingVec<T> From(const Math::Vector2Base<T>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        return { _vec.x, _vec.y, 0.f, 0.f };
    }

    template <class T>
    MatchingVec<T> From(const Math::Vector3Base<T>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        return { _vec.x, _vec.y, _vec.z, 0.f };
    }

    template <class T, bool SimdAligned>
    MatchingVec<T> From(const Math::Vector4Base<T, SimdAligned>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        if constexpr (SimdAligned)
        {
            return Simd::LoadAligned(_vec.GetPtr());
        }
        else
        {
            return Simd::LoadUnaligned(_vec.GetPtr());
        }
    }

    template <class T>
    void Store(MatchingVec<T> _simdVec, Math::Vector2Base<T>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        _vec.x = _simdVec.x;
        _vec.y = _simdVec.y;
    }

    template <class T>
    void Store(MatchingVec<T> _simdVec, Math::Vector3Base<T>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        _vec.x = _simdVec.x;
        _vec.y = _simdVec.y;
        _vec.z = _simdVec.z;
    }

    template <class T, bool SimdAligned>
    void Store(MatchingVec<T> _simdVec, Math::Vector4Base<T, SimdAligned>& _vec) requires (!std::is_void_v<MatchingVec<T>>)
    {
        if constexpr (SimdAligned)
        {
            Simd::StoreAligned(_vec.GetPtr(), _simdVec);
        }
        else
        {
            Simd::StoreUnaligned(_vec.GetPtr(), _simdVec);
        }
    }
}