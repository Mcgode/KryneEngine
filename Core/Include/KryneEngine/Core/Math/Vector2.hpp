/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include <cstddef>
#include <type_traits>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"
#include "KryneEngine/Core/Window/Input/Enums.hpp"

namespace KryneEngine::Math
{
    template <class T, class U>
    concept IsVector2Compatible = requires(U _u)
    {
        { _u.x } -> std::convertible_to<T>;
        { _u.y } -> std::convertible_to<T>;
    };

    template <typename T>
    struct Vector2Base
    {
        using ScalarType = T;

        static constexpr size_t kSimdOptimalAlignment = Alignment::AlignUpPot(2 * sizeof(T), 4);

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

        template <typename U>
        requires std::is_constructible_v<T, U>
        explicit Vector2Base(const Vector2Base<U> &_other) : Vector2Base(_other.x, _other.y)
        {}

        template <class U> requires IsVector2Compatible<T, U>
        explicit Vector2Base(const U& _other) : Vector2Base(_other.x, _other.y) {}

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
        union
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

    template<typename T>
    extern T Dot(const Vector2Base<T>& _a, const Vector2Base<T>& _b);

    template<typename T>
    concept Vector2Type = requires {
        typename T::ScalarType;
        std::is_same_v<T, Vector2Base<typename T::ScalarType>>;
    };
}
