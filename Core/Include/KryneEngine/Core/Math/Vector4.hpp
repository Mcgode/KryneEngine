/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include <cstddef>
#include <type_traits>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Math/Vector3.hpp"

namespace KryneEngine::Math
{
    template <class T, class U>
    concept IsVector4Compatible = requires(U _u)
    {
        { _u.x } -> std::convertible_to<T>;
        { _u.y } -> std::convertible_to<T>;
        { _u.z } -> std::convertible_to<T>;
        { _u.w } -> std::convertible_to<T>;
    };

    template <typename T, bool SimdAligned = false>
    struct Vector4Base
    {
        using ScalarType = T;
        static constexpr bool kSimdAligned = SimdAligned;

        static_assert(sizeof(T) >= 4 || !SimdAligned, "Vector4Base element type must be at least 4 bytes to use SIMD");

        static constexpr size_t kSimdOptimalAlignment = Alignment::AlignUpPot(4 * sizeof(T), 4);
        static constexpr size_t kAlignment = SimdAligned ? kSimdOptimalAlignment : alignof(T);

        Vector4Base() = default;

        template <typename U0, typename U1 = U0, typename U2 = U0, typename U3 = U0>
        requires std::is_constructible_v<T, U0>
            && std::is_constructible_v<T, U1>
            && std::is_constructible_v<T, U2>
            && std::is_constructible_v<T, U3>
        constexpr Vector4Base(U0 _x, U1 _y, U2 _z = 0, U3 _w = 0) : x(_x), y(_y), z(_z), w(_w) {}

        template <typename U>
        requires std::is_constructible_v<T, U>
        explicit constexpr Vector4Base(U _value) : Vector4Base(_value, _value, _value, _value) {}

        template <typename U, bool S>
        requires std::is_constructible_v<T, U>
        explicit constexpr Vector4Base(const Vector4Base<U, S> &_other) : Vector4Base(_other.x, _other.y, _other.z, _other.w) {}

        template <typename U0, typename U1>
        requires std::is_constructible_v<T, U0> && std::is_constructible_v<T, U1>
        explicit constexpr Vector4Base(const Vector3Base<U0>& _vec3, U1 _w = 0): Vector4Base(_vec3.x, _vec3.y, _vec3.z, _w) {}

        template <typename U0, typename U1, typename U2>
        requires std::is_constructible_v<T, U0> && std::is_constructible_v<T, U1> && std::is_constructible_v<T, U2>
        explicit constexpr Vector4Base(const Vector2Base<U0>& _vec2, U1 _z = 0, U2 _w = 0): Vector4Base(_vec2.x, _vec2.y, _z, _w) {}

        template<class U> requires IsVector4Compatible<T, U>
        explicit constexpr Vector4Base(const U& _other) : Vector4Base(_other.x, _other.y, _other.z, _other.w) {}

        Vector4Base operator+(const Vector4Base& _other) const;
        Vector4Base operator-(const Vector4Base& _other) const;
        Vector4Base operator*(const Vector4Base& _other) const;
        Vector4Base operator/(const Vector4Base& _other) const;

        template<class U> requires std::is_constructible_v<T, U>
        inline Vector4Base operator+(U _scalar) const { return *this + Vector4Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        inline Vector4Base operator-(U _scalar) const { return *this - Vector4Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        inline Vector4Base operator*(U _scalar) const { return *this * Vector4Base(_scalar); }

        template<class U> requires std::is_constructible_v<T, U>
        inline Vector4Base operator/(U _scalar) const { return *this / Vector4Base(_scalar); }

        void MinComponents(const Vector4Base& _other);
        void MaxComponents(const Vector4Base& _other);

        T& operator[](size_t _index);
        const T& operator[](size_t _index) const;

        T* GetPtr();
        const T* GetPtr() const;

        static constexpr T kEqualsEpsilon = 1e-6f;
        bool operator==(const Vector4Base& _other) const;
        bool Equals(const Vector4Base& _other, T _epsilon = kEqualsEpsilon) const requires std::is_floating_point_v<T>;

        void Normalize() requires std::is_floating_point_v<T>;
        Vector4Base Normalized() const requires std::is_floating_point_v<T>;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
        union alignas(kAlignment)
        {
            struct
            {
                T x = 0;
                T y = 0;
                T z = 0;
                T w = 0;
            };
            struct
            {
                T r;
                T g;
                T b;
                T a;
            };
        };
#pragma clang diagnostic pop
    };

    template<typename T, bool SimdAligned>
    extern T Dot(const Vector4Base<T, SimdAligned>& _a, const Vector4Base<T, SimdAligned>& _b);

    template<typename T>
    concept Vector4Type = requires {
        typename T::ScalarType;
        T::kSimdAligned;
        std::is_same_v<T, Vector4Base<typename T::ScalarType, T::kSimdAligned>>;
    };
}