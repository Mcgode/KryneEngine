/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#include "KryneEngine/Core/Math/Vector2.hpp"

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/XSimdUtils.hpp"

namespace KryneEngine::Math
{
    struct AddOperator
    {
        template <class T>
        T operator()(const T& _a, const T& _b) { return _a + _b; }
    };

    struct SubtractOperator
    {
        template <class T>
        T operator()(const T& _a, const T& _b)  { return _a - _b; }
    };

    struct MultiplyOperator
    {
        template <class T>
        T operator()(const T& _a, const T& _b) { return _a * _b; }
    };

    struct DivideOperator
    {
        template <class T>
        T operator()(const T& _a, const T& _b) { return _a / _b; }
    };

    template <typename T>
    Vector2Base<T> Vector2Base<T>::operator+(const Vector2Base<T>& _other) const
    {
        return { x + _other.x, y + _other.y };
    }

    template <typename T>
    Vector2Base<T> Vector2Base<T>::operator-(const Vector2Base<T>& _other) const
    {
        return { x - _other.x, y - _other.y };
    }
    template <typename T>
    Vector2Base<T> Vector2Base<T>::operator*(const Vector2Base<T>& _other) const
    {
        return { x * _other.x, y * _other.y };
    }
    template <typename T>
    Vector2Base<T> Vector2Base<T>::operator/(const Vector2Base<T>& _other) const
    {
        return { x / _other.x, y / _other.y };
    }

    template <typename T>
    bool Vector2Base<T>::operator==(const Vector2Base& _other) const
    {
        return x == _other.x && y == _other.y;
    }

    template <typename T>
    void Vector2Base<T>::Normalize()
        requires std::is_floating_point_v<T>
    {
        const T length = std::sqrt(Dot(*this, *this));
        if (length > 0.0f)
        {
            x /= length;
            y /= length;
        }
    }

    template <typename T>
    Vector2Base<T> Vector2Base<T>::Normalized() const
        requires std::is_floating_point_v<T>
    {
        Vector2Base result(*this);
        result.Normalize();
        return result;
    }

    template <typename T>
    T Dot(const Vector2Base<T>& _a, const Vector2Base<T>& _b)
    {
        return _a.x * _b.x + _a.y * _b.y;
    }

#define IMPLEMENT_SIMD(type)                                                                                      \
    template struct Vector2Base<type>;                                                                            \
    template type Dot<type>(const Vector2Base<type>& _a, const Vector2Base<type>& _b)

#define IMPLEMENT(type)                                                                                                 \
    IMPLEMENT_SIMD(type)

    IMPLEMENT(float);
    IMPLEMENT(s32);
    IMPLEMENT(u32);
    IMPLEMENT(double);

#undef IMPLEMENT
#undef IMPLEMENT_SIMD
} // namespace KryneEngine::Math
