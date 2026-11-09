/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#include "KryneEngine/Core/Math/Vector3.hpp"

#include <cmath>

#include "KryneEngine/Core/Common/Types.hpp"

namespace KryneEngine::Math
{

    template <typename T>
    Vector3Base<T> Vector3Base<T>::operator+(const Vector3Base& _other) const
    {
        return {
            x + _other.x,
            y + _other.y,
            z + _other.z
        };
    }

    template <typename T>
    Vector3Base<T> Vector3Base<T>::operator-(const Vector3Base& _other) const
    {
        return {
            x - _other.x,
            y - _other.y,
            z - _other.z
        };
    }
    template <typename T>
    Vector3Base<T> Vector3Base<T>::operator*(const Vector3Base& _other) const
    {
        return {
            x * _other.x,
            y * _other.y,
            z * _other.z
        };
    }
    template <typename T>
    Vector3Base<T> Vector3Base<T>::operator/(const Vector3Base& _other) const
    {
        return {
            x / _other.x,
            y / _other.y,
            z / _other.z
        };
    }

    template <typename T>
    Vector3Base<T> Vector3Base<T>::Sqrt() const requires std::is_floating_point_v<T>
    {
        return {
            std::sqrt(x),
            std::sqrt(y),
            std::sqrt(z)
        };
    }


    template <typename T>
    void Vector3Base<T>::MinComponents(const Vector3Base& _other)
    {
        x = eastl::min(x, _other.x);
        y = eastl::min(y, _other.y);
        z = eastl::min(z, _other.z);
    }

    template <typename T>
    void Vector3Base<T>::MaxComponents(const Vector3Base& _other)
    {
        x = eastl::max(x, _other.x);
        y = eastl::max(y, _other.y);
        z = eastl::max(z, _other.z);
    }

    template <typename T>
    T& Vector3Base<T>::operator[](size_t _index)
    {
        return reinterpret_cast<T*>(this)[_index];
    }

    template <typename T>
    const T& Vector3Base<T>::operator[](size_t _index) const
    {
        return reinterpret_cast<const T*>(this)[_index];
    }

    template <typename T>
    bool Vector3Base<T>::operator==(const Vector3Base& _other) const
    {
        return x == _other.x && y == _other.y && z == _other.z;
    }

    template <typename T>
    T Vector3Base<T>::LengthSquared() const
    {
        return Dot(*this, *this);
    }

    template <typename T>
    T Vector3Base<T>::Length() const
    {
        return sqrt(LengthSquared());
    }

    template <typename T>
    void Vector3Base<T>::Normalize()
        requires std::is_floating_point_v<T>
    {
        const T length = Length();
        if (length > 0.0f)
        {
            x /= length;
            y /= length;
            z /= length;
        }
    }

    template <typename T>
    Vector3Base<T> Vector3Base<T>::Normalized() const
        requires std::is_floating_point_v<T>
    {
        Vector3Base result(*this);
        result.Normalize();
        return result;
    }

    template <typename T>
    T Vector3Base<T>::Dot(const Vector3Base& _a, const Vector3Base& _b)
    {
        return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z;
    }

    template <typename T>
    Vector3Base<T> Vector3Base<T>::CrossProduct(const Vector3Base& _a, const Vector3Base& _b)
    {
        return Vector3Base {
            _a.y * _b.z - _a.z * _b.y,
            _a.z * _b.x - _a.x * _b.z,
            _a.x * _b.y - _a.y * _b.x
        };
    }

    template struct Vector3Base<float>;
    template struct Vector3Base<s32>;
    template struct Vector3Base<u32>;
    template struct Vector3Base<double>;
} // namespace KryneEngine::Math
