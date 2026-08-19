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

        QuaternionBase operator*(const QuaternionBase& _other) const;

        bool operator==(const QuaternionBase& _other) const;

        T Length2() const;
        T Length() const;

        QuaternionBase& Normalize();
        QuaternionBase& Conjugate();
        QuaternionBase& Inverse();

        static T Dot(const QuaternionBase& _a, const QuaternionBase& _b);

        QuaternionBase& Slerp(const QuaternionBase& _other, T _t);

        template <Vector3Type Vec3>
        Vec3 ApplyTo(const Vec3& _vec) const
        {
            Vec3 u(x, y, z);

            return u * 2.0 * Vec3::Dot(u, _vec)
                + _vec * (w * w - Vec3::Dot(u, u))
                + Vec3::CrossProduct(u, _vec) * 2.0 * w;
        }

        T* Ptr() { return &w; }
        const T* Ptr() const { return &w; }

        [[nodiscard]] u32 Pack32() const;
        [[nodiscard]] u64 Pack64() const;

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
