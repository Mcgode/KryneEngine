/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include <cstddef>
#include <type_traits>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Math/Vector2.hpp"

namespace KryneEngine::Math
{
    template <class T, class U>
    concept IsVector3Compatible = requires(U _u)
    {
        { _u.x } -> std::convertible_to<T>;
        { _u.y } -> std::convertible_to<T>;
        { _u.z } -> std::convertible_to<T>;
    };

    template <typename T, bool SimdOptimal = false>
    struct Vector3Base
    {
        using ScalarType = T;
        static constexpr bool kSimdOptimal = SimdOptimal;

        static_assert(sizeof(T) >= 4 || !SimdOptimal, "Vector3Base element type must be at least 4 bytes to use SIMD");

        static constexpr size_t kSimdOptimalAlignment = Alignment::AlignUpPot(3 * sizeof(T), 4);
        static constexpr size_t kAlignment = SimdOptimal ? kSimdOptimalAlignment : alignof(T);

        Vector3Base(): x(), y(), z()
        {
            if constexpr (SimdOptimal)
            {
                // Ensure padding is zero-initialized
                new (&x + 3) T(0);
            }
        }

        template <typename U0, typename U1 = U0, typename U2 = U0>
        requires std::is_constructible_v<T, U0> && std::is_constructible_v<T, U1> && std::is_constructible_v<T, U2>
        Vector3Base(U0 _x, U1 _y, U2 _z = 0) : x(_x), y(_y), z(_z)
        {
            if constexpr (SimdOptimal)
            {
                // Ensure padding is zero-initialized
                new (&x + 3) T(0);
            }
        }

        template <typename U>
        requires std::is_constructible_v<T, U>
        explicit Vector3Base(U _value) : Vector3Base(_value, _value, _value) {}

        template <typename U, bool S>
        requires std::is_constructible_v<T, U>
        explicit Vector3Base(const Vector3Base<U, S> &_other) : Vector3Base(_other.x, _other.y, _other.z) {}

        template <typename U0, typename U1, bool S>
            requires std::is_constructible_v<T, U0> && std::is_constructible_v<T, U1>
        explicit Vector3Base(const Vector2Base<U0, S>& _vec2, U1 _z = 0): Vector3Base(_vec2.x, _vec2.y, _z) {}

        template <class U> requires IsVector3Compatible<T, U>
        explicit Vector3Base(const U& _other) : Vector3Base(_other.x, _other.y, _other.z) {}

        Vector3Base operator+(const Vector3Base& _other) const;
        Vector3Base operator-(const Vector3Base& _other) const;
        Vector3Base operator*(const Vector3Base& _other) const;
        Vector3Base operator/(const Vector3Base& _other) const;

        Vector3Base Sqrt() const requires std::is_floating_point_v<T>;

        template<class U> requires std::is_constructible_v<T, U>
        Vector3Base operator+(U _scalar) const { return *this + Vector3Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        Vector3Base operator-(U _scalar) const { return *this - Vector3Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        Vector3Base operator*(U _scalar) const { return *this * Vector3Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        Vector3Base operator/(U _scalar) const { return *this / Vector3Base(_scalar); }

        void MinComponents(const Vector3Base& _other);
        void MaxComponents(const Vector3Base& _other);

        T& operator[](size_t _index);
        const T& operator[](size_t _index) const;

        T* GetPtr() { return &x; }
        const T* GetPtr() const { return &x; }

        bool operator==(const Vector3Base& _other) const;

        T LengthSquared() const;
        T Length() const;

        void Normalize() requires std::is_floating_point_v<T>;
        Vector3Base Normalized() const requires std::is_floating_point_v<T>;

        static T Dot(const Vector3Base& _a, const Vector3Base& _b);
        static Vector3Base CrossProduct(const Vector3Base& _a, const Vector3Base& _b);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
        union alignas(kAlignment)
        {
            struct
            {
                T x = 0;
                T y = 0;
                T z = 0;
            };
            struct
            {
                T r;
                T g;
                T b;
            };
        };
#pragma clang diagnostic pop
    };

    template<typename T>
    concept Vector3Type = requires {
        typename T::ScalarType;
        T::kSimdOptimal;
        std::is_same_v<T, Vector3Base<typename T::ScalarType, T::kSimdOptimal>>;
    };
}