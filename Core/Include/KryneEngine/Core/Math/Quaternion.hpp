/**
 * @file
 * @author Max Godefroy
 * @date 12/03/2025.
 */

#pragma once

#include <cmath>
#include <type_traits>

#include "KryneEngine/Core/Math/Vector3.hpp"

namespace KryneEngine::Math
{
    /**
     * @brief Represents a quaternion in 3D space.
     *
     * @details A quaternion is a mathematical representation often used for rotations in 3D space.
     * It consists of a scalar part (w) and a three-dimensional vector part (x, y, z).
     * Quaternions are particularly useful for representing rotations as they avoid the gimbal lock
     * problem inherent in Euler angles and provide more compact and efficient computations than rotation matrices.
     *
     * @tparam T The scalar type used for the quaternion values. Must be a floating-point type.
     */
    template<class T> requires (std::is_floating_point_v<T>)
    struct QuaternionBase
    {
        using ScalarType = T;

        QuaternionBase()
            : w(1)
            , x(0)
            , y(0)
            , z(0)
        {}

        QuaternionBase(T _w, T _x, T _y, T _z)
            : w(_w)
            , x(_x)
            , y(_y)
            , z(_z)
        {}

        ~QuaternionBase() = default;

        QuaternionBase(const QuaternionBase& _other) = default;
        QuaternionBase(QuaternionBase&& _other) noexcept = default;
        QuaternionBase& operator=(const QuaternionBase& _other) = default;
        QuaternionBase& operator=(QuaternionBase&& _other) noexcept = default;

        template <class U>
            requires std::is_convertible_v<U, T>
        QuaternionBase& FromAxisAngle(Vector3Base<T> _axis, U _angle)
        {
            w = std::cos(_angle * .5f);
            const T s = std::sin(_angle * .5f);
            x =  _axis.x * s;
            y =  _axis.y * s;
            z =  _axis.z * s;
            return *this;
        }

        QuaternionBase operator*(const QuaternionBase& _other) const
        {
            return QuaternionBase(
                w * _other.w - x * _other.x - y * _other.y - z * _other.z,
                w * _other.x + x * _other.w + y * _other.z - z * _other.y,
                w * _other.y - x * _other.z + y * _other.w + z * _other.x,
                w * _other.z + x * _other.y - y * _other.x + z * _other.w);
        }

        bool operator==(const QuaternionBase& _other) const
        {
            return std::abs(w - _other.w) < kQuaternionEpsilon
                && std::abs(x - _other.x) < kQuaternionEpsilon
                && std::abs(y - _other.y) < kQuaternionEpsilon
                && std::abs(z - _other.z) < kQuaternionEpsilon;
        }

        T Length2() const
        {
            return w * w + x * x + y * y + z * z;
        }
        T Length() const
        {
            return std::sqrt(Length2());
        }

        QuaternionBase Normalize()
        {
            const T length = Length();
            w /= length;
            x /= length;
            y /= length;
            z /= length;
            return *this;
        }
        QuaternionBase& Conjugate()
        {
            x = -x;
            y = -y;
            z = -z;
            return *this;
        }
        QuaternionBase& Inverse() const
        {
            return Conjugate();
        }

        static T Dot(const QuaternionBase& _a, const QuaternionBase& _b)
        {
            return _a.w * _b.w + _a.x * _b.x + _a.y * _b.y + _a.z * _b.z;
        }

        QuaternionBase& Slerp(const QuaternionBase& _other, T _t)
        {
            if (_t == 0)
            {
                return *this;
            }
            else if (_t == 1)
            {
                *this = _other;
                return *this;
            }

            const T cosHalfTheta = Dot(*this, _other);

            // If _a == _b or _a = -_b, then theta = 0, we can return a
            if (std::abs(cosHalfTheta) >= 1.0f)
                return *this;

            if (cosHalfTheta < 0)
            {
                w = -w;
                x = -x;
                y = -y;
                z = -z;
                cosHalfTheta = -cosHalfTheta;
            }

            const T halfTheta = std::acos(cosHalfTheta);
            const T sinHalfTheta = std::sqrt(1.0f - cosHalfTheta * cosHalfTheta);

            // If theta = 180°, result is not clearly defined, as we could rotate around any angle
            if (std::abs(sinHalfTheta) < kQuaternionEpsilon)
            {
                w = 0.5 * w + 0.5 * _other.w;
                x = 0.5 * x + 0.5 * _other.x;
                y = 0.5 * y + 0.5 * _other.y;
                z = 0.5 * z + 0.5 * _other.z;
                return *this;
            }

            const T ratioA = std::sin((1.0f - _t) * halfTheta) / sinHalfTheta;
            const T ratioB = std::sin(_t * halfTheta) / sinHalfTheta;
            w = ratioA * w + ratioB * _other.w;
            x = ratioA * x + ratioB * _other.x;
            y = ratioA * y + ratioB * _other.y;
            z = ratioA * z + ratioB * _other.z;
            return *this;
        }

        template<Vector3Type Vec3>
        Vec3 ApplyTo(const Vec3& _vec) const
        {
            Vec3 u(x, y, z);

            return u * 2.0 * Vec3::Dot(u, _vec)
                + _vec * (w * w - Vec3::Dot(u, u))
                + Vec3::CrossProduct(u, _vec) * 2.0 * w;
        }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
        T w;
        T x;
        T y;
        T z;
#pragma clang diagnostic pop

        static constexpr T kQuaternionEpsilon = T(1e-6f);
    };

    template<class T>
    concept QuaternionType = requires {
        typename T::ScalarType;
        std::is_same_v<T, QuaternionBase<typename T::ScalarType>>;
    };

    using Quaternion = QuaternionBase<float>;
}
