/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#include "KryneEngine/Core/Math/Vector4.hpp"

#include <cmath>

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Simd/SimdArithmeticOperations.hpp"
#include "KryneEngine/Core/Math/Simd/VectorUtils.hpp"

namespace KryneEngine::Math
{
    template <typename T, bool SimdAligned>
    Vector4Base<T, SimdAligned> Vector4Base<T, SimdAligned>::operator+(const Vector4Base<T, SimdAligned>& _other) const
    {
        return {
            x + _other.x,
            y + _other.y,
            z + _other.z,
            w + _other.w
        };
    }

    template <typename T, bool SimdAligned>
    Vector4Base<T, SimdAligned> Vector4Base<T, SimdAligned>::operator-(const Vector4Base<T, SimdAligned>& _other) const
    {
        return {
            x - _other.x,
            y - _other.y,
            z - _other.z,
            w - _other.w
        };
    }
    template <typename T, bool SimdAligned>
    Vector4Base<T, SimdAligned> Vector4Base<T, SimdAligned>::operator*(const Vector4Base<T, SimdAligned>& _other) const
    {
        return {
            x * _other.x,
            y * _other.y,
            z * _other.z,
            w * _other.w
        };
    }
    template <typename T, bool SimdAligned>
    Vector4Base<T, SimdAligned> Vector4Base<T, SimdAligned>::operator/(const Vector4Base<T, SimdAligned>& _other) const
    {
        return {
            x / _other.x,
            y / _other.y,
            z / _other.z,
            w / _other.w
        };
    }

    template <typename T, bool SimdAligned>
    void Vector4Base<T, SimdAligned>::MinComponents(const Vector4Base& _other)
    {
        x = eastl::min(x, _other.x);
        y = eastl::min(y, _other.y);
        z = eastl::min(z, _other.z);
        w = eastl::min(w, _other.w);
    }

    template <typename T, bool SimdAligned>
    void Vector4Base<T, SimdAligned>::MaxComponents(const Vector4Base& _other)
    {
        x = eastl::max(x, _other.x);
        y = eastl::max(y, _other.y);
        z = eastl::max(z, _other.z);
        w = eastl::max(w, _other.w);
    }

    template <typename T, bool SimdAligned>
    T& Vector4Base<T, SimdAligned>::operator[](size_t _index)
    {
        _index = eastl::min<size_t>(3u, _index);
        return (&x)[_index];
    }

    template <typename T, bool SimdAligned>
    const T& Vector4Base<T, SimdAligned>::operator[](size_t _index) const
    {
        _index = eastl::min<size_t>(3u, _index);
        return (&x)[_index];
    }

    template <typename T, bool SimdAligned>
    T* Vector4Base<T, SimdAligned>::GetPtr()
    {
        return &x;
    }

    template <typename T, bool SimdAligned>
    const T* Vector4Base<T, SimdAligned>::GetPtr() const
    {
        return &x;
    }

    template <typename T, bool SimdAligned>
    bool Vector4Base<T, SimdAligned>::operator==(const Vector4Base& _other) const
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return Equals(_other);
        }
        else
        {
            return x == _other.x && y == _other.y && z == _other.z && w == _other.w;
        }
    }

    template <typename T, bool SimdAligned>
    bool Vector4Base<T, SimdAligned>::Equals(const Vector4Base& _other, T _epsilon) const
    requires std::is_floating_point_v<T>
    {
        if (_epsilon == 0.0f)
        {
            return x == _other.x && y == _other.y && z == _other.z && w == _other.w;
        }
        return std::abs(x - _other.x) <= _epsilon
               && (std::abs(y - _other.y) < _epsilon)
               && (std::abs(z - _other.z) < _epsilon)
               && (std::abs(w - _other.w) < _epsilon);
    }

    template <typename T, bool SimdAligned>
    void Vector4Base<T, SimdAligned>::Normalize()
        requires std::is_floating_point_v<T>
    {
        const T length = std::sqrt(Dot(*this, *this));
        if (length > 0.0f)
        {
            x /= length;
            y /= length;
            z /= length;
            w /= length;
        }
    }

    template <typename T, bool SimdAligned>
    Vector4Base<T, SimdAligned> Vector4Base<T, SimdAligned>::Normalized() const
        requires std::is_floating_point_v<T>
    {
        Vector4Base result(*this);
        result.Normalize();
        return result;
    }

    template <typename T, bool SimdAligned>
    T Dot(const Vector4Base<T, SimdAligned>& _a, const Vector4Base<T, SimdAligned>& _b)
    {
        using SimdVec = Simd::MatchingVec<T>;

        if constexpr (std::is_void_v<SimdVec>)
        {
            return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
        }
        else
        {
            return Simd::ReduceSum(
                Simd::Multiply(
                    Simd::From(_a),
                    Simd::From(_b)));
        }
    }


#define IMPLEMENT_SIMD(type, simd)                                                                                      \
    template struct Vector4Base<type, simd>;                                                                            \
    template type Dot<type, simd>(const Vector4Base<type, simd>& _a, const Vector4Base<type, simd>& _b)

#define IMPLEMENT(type)                                                                                                 \
    IMPLEMENT_SIMD(type, false);                                                                                        \
    IMPLEMENT_SIMD(type, true)

    IMPLEMENT(float);
    IMPLEMENT(s32);
    IMPLEMENT(u32);
    IMPLEMENT(double);

#undef IMPLEMENT
#undef IMPLEMENT_SIMD
} // namespace KryneEngine::Math
