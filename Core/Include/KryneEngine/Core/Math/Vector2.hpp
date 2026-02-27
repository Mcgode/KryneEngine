/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include <cstddef>
#include <type_traits>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"

namespace KryneEngine::Math
{
    template <typename T, bool SimdOptimal = false>
    struct Vector2Base
    {
        using ScalarType = T;
        static constexpr bool kSimdOptimal = SimdOptimal;

        static_assert(sizeof(T) >= 4 || !SimdOptimal, "Vector2Base element type must be at least 4 bytes to use SIMD");

        static constexpr size_t kSimdOptimalAlignment = Alignment::AlignUpPot(2 * sizeof(T), 4);
        static constexpr size_t kAlignment = SimdOptimal ? kSimdOptimalAlignment : alignof(T);

        Vector2Base()
            : x(), y()
        {
            if constexpr (sizeof(Vector2Base) == 4 * sizeof(T))
            {
                // Ensure padding is zero-initialized
                new (&x + 2) T(0);
                new (&x + 3) T(0);
            }
        }

        template <typename U0, typename U1 = U0>
        requires std::is_constructible_v<T, U0>
            && std::is_constructible_v<T, U1>
        Vector2Base(U0 _x, U1 _y) : x(_x), y(_y)
        {
            if constexpr (sizeof(Vector2Base) == 4 * sizeof(T))
            {
                // Ensure padding is zero-initialized
                new (&x + 2) T(0);
                new (&x + 3) T(0);
            }
        }

        template <typename U>
        requires std::is_constructible_v<T, U>
        explicit Vector2Base(U _value) : Vector2Base(_value, _value) {}

        template <typename U, bool OtherSimdOptimal>
        requires std::is_constructible_v<T, U>
        explicit Vector2Base(const Vector2Base<U, OtherSimdOptimal> &_other) : Vector2Base(_other.x, _other.y)
        {}

        Vector2Base operator+(const Vector2Base& _other) const;
        Vector2Base operator-(const Vector2Base& _other) const;
        Vector2Base operator*(const Vector2Base& _other) const;
        Vector2Base operator/(const Vector2Base& _other) const;

        template <class U> requires std::is_constructible_v<T, U> Vector2Base operator+(U _v) { return Vector2Base(x + _v, y + _v); }
        template <class U> requires std::is_constructible_v<T, U> Vector2Base operator-(U _v) { return Vector2Base(x - _v, y - _v); }
        template <class U> requires std::is_constructible_v<T, U> Vector2Base operator*(U _v) { return Vector2Base(x * _v, y * _v); }
        template <class U> requires std::is_constructible_v<T, U> Vector2Base operator/(U _v) { return Vector2Base(x / _v, y / _v); }

        bool operator==(const Vector2Base& _other) const;

        T* GetPtr() { return &x; }
        const T* GetPtr() const { return &x; }

        void Normalize() requires std::is_floating_point_v<T>;
        Vector2Base Normalized() const requires std::is_floating_point_v<T>;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
        union alignas(kAlignment)
        {
            struct
            {
                T x = 0;
                T y = 0;
            };
            struct
            {
                T r;
                T g;
            };
        };
#pragma clang diagnostic pop
    };

    template<typename T, bool SimdOptimal>
    extern T Dot(const Vector2Base<T, SimdOptimal>& _a, const Vector2Base<T, SimdOptimal>& _b);

    template<typename T>
    concept Vector2Type = requires {
        typename T::ScalarType;
        T::kSimdOptimal;
        std::is_same_v<T, Vector2Base<typename T::ScalarType, T::kSimdOptimal>>;
    };
}