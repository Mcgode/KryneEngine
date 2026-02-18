/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2025.
 */

#pragma once

#include "../Common/Types.hpp"
#include "Vector.hpp"
#include <KryneEngine/Core/Math/Vector.hpp>

namespace KryneEngine
{
    struct Color
    {
        float4 m_value { 1, 0, 1, 1 };

        Color() = default;

        explicit constexpr Color(const float4& _value)
            : m_value(_value)
        {}

        explicit constexpr Color(const float3& _value, float _alpha = 1.f)
            : m_value(_value, _alpha)
        {}

        constexpr Color(float _r, float _g, float _b, float _a = 1.f)
            : m_value(_r, _g, _b, _a)
        {}

        Color(u8 _r, u8 _g, u8 _b, u8 _a = 255)
        {
            m_value = float4(_r, _g, _b, _a) / 255.f;
        }

        constexpr explicit Color(u32 _rgba, bool _lowEndian = true)
        {
            if (_lowEndian)
            {
                m_value = {
                    static_cast<float>(_rgba & 0xff) / 255.f,
                    static_cast<float>((_rgba >> 8) & 0xff) / 255.f,
                    static_cast<float>((_rgba >> 16) & 0xff) / 255.f,
                    static_cast<float>((_rgba >> 24) & 0xff) / 255.f,
                };
            }
            else
            {
                m_value = {
                    static_cast<float>((_rgba >> 24) & 0xff) / 255.f,
                    static_cast<float>((_rgba >> 16) & 0xff) / 255.f,
                    static_cast<float>((_rgba >> 8) & 0xff) / 255.f,
                    static_cast<float>(_rgba & 0xff) / 255.f,
                };
            }
        }

        [[nodiscard]] u32 ToRgba8(const bool _lowEndian = true) const
        {
            const float4 unormValue = m_value * 255.0f;
            if (_lowEndian)
            {
                return (static_cast<u32>(unormValue.x) & 0xff)
                       | (static_cast<u32>(unormValue.y) & 0xff) << 8
                       | (static_cast<u32>(unormValue.z) & 0xff) << 16
                       | (static_cast<u32>(unormValue.w) & 0xff) << 24;
            }
            else
            {
                return (static_cast<u32>(unormValue.w) & 0xff)
                       | (static_cast<u32>(unormValue.z) & 0xff) << 8
                       | (static_cast<u32>(unormValue.y) & 0xff) << 16
                       | (static_cast<u32>(unormValue.x) & 0xff) << 24;
            }
        }

        [[nodiscard]] u32 ToArgb8(const bool _lowEndian = true) const
        {
            const float4 unormValue = m_value * 255.0f;
            if (_lowEndian)
            {
                return (static_cast<u32>(unormValue.a) & 0xff)
                       | (static_cast<u32>(unormValue.r) & 0xff) << 8
                       | (static_cast<u32>(unormValue.g) & 0xff) << 16
                       | (static_cast<u32>(unormValue.b) & 0xff) << 24;
            }
            else
            {
                return (static_cast<u32>(unormValue.b) & 0xff)
                       | (static_cast<u32>(unormValue.g) & 0xff) << 8
                       | (static_cast<u32>(unormValue.r) & 0xff) << 16
                       | (static_cast<u32>(unormValue.a) & 0xff) << 24;
            }
        }

        // Based on https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
        [[nodiscard]] Color ToSrgb() const
        {
            const float3_simd rgb { m_value.r, m_value.g, m_value.b };

            const float3_simd s1 = rgb.Sqrt();
            const float3_simd s2 = s1.Sqrt();
            const float3_simd s3 = s2.Sqrt();

            const float3_simd srgb = s1 * 0.585122381 + s2 * 0.783140355 - s3 * 0.368262736;

            return Color(float4(srgb, m_value.a));
        }

        // Based on https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
        [[nodiscard]] Color FromSrgb() const
        {
            const float3_simd srgb { m_value.r, m_value.g, m_value.b };

            const float3_simd rgb = srgb * (srgb * (srgb * 0.305306011 + 0.682171111) + 0.012522878);

            return Color(float4(rgb, m_value.a));
        }
    };

    namespace ColorPalette
    {
        static constexpr Color kWhite = Color(0xFF'FF'FF'FF);
        static constexpr Color kBlackOpaque = Color(0x00'00'00'FF);
        static constexpr Color kBlack = Color(0x00'00'00'00);
    }
}