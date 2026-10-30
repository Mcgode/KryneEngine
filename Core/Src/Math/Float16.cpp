/**
 * @file
 * @author Max Godefroy
 * @date 19/06/2025.
 */

#include "KryneEngine/Core/Math/Float16.hpp"

#if defined(__ARM_NEON) && defined(__ARM_FP16_FORMAT_IEEE)
#   include <arm_neon.h>
#   define KE_ARM_NEON
#elif defined(__x86_64__) || defined(_M_X64)
#   include <immintrin.h>
#   include "KryneEngine/Core/Math/Simd/SimdCommon.hpp"
#   if defined(_MSC_VER) && !defined(__clang__)
#       define KE_F16C_TARGET
#   else
#       define KE_F16C_TARGET __attribute__((target("f16c")))
#   endif
#   define KE_X86_F16C
#endif

namespace KryneEngine::Math
{
#if defined(KE_X86_F16C)
    namespace
    {
        // F16C is not part of the x86-64 baseline, so it is only used once `Simd::g_simdSupport` is initialized.
        // Before that (during static initialization), the software path is used, which produces identical results.
        bool HasF16C()
        {
            return BitUtils::EnumHasAny(Simd::g_simdSupport, Simd::SimdSupport::F16C);
        }

        // Explicit round to nearest even, rather than the current MXCSR mode, so results never depend on thread state.
        KE_F16C_TARGET u16 ConvertToFloat16F16C(const float _value)
        {
            return static_cast<u16>(_cvtss_sh(_value, _MM_FROUND_TO_NEAREST_INT));
        }

        KE_F16C_TARGET float ConvertFromFloat16F16C(const u16 _value)
        {
            return _cvtsh_ss(_value);
        }

        // Converts both values with a single instruction. Lane 0 lands in the low 16 bits, matching the software path.
        KE_F16C_TARGET u32 PackFloat16x2F16C(const float _a, const float _b)
        {
            const __m128 values = _mm_setr_ps(_a, _b, 0.f, 0.f);
            return static_cast<u32>(_mm_cvtsi128_si32(_mm_cvtps_ph(values, _MM_FROUND_TO_NEAREST_INT)));
        }
    }
#endif

    Float16::Float16(float _value)
        : m_data(ConvertToFloat16(_value))
    {}

    Float16& Float16::operator=(float _value)
    {
        m_data = ConvertToFloat16(_value);
        return *this;
    }

    Float16::operator float() const
    {
        return ConvertFromFloat16(m_data);
    }

    u16 Float16::ConvertToFloat16(float _value)
    {
#if defined(KE_ARM_NEON)
        float32x4_t val { _value };
        float16x4_t result = vcvt_f16_f32(val);
        return *reinterpret_cast<u16*>(&result);
#else
#   if defined(KE_X86_F16C)
        if (HasF16C())
            return ConvertToFloat16F16C(_value);
#   endif

        const u32 binaryFloat = *reinterpret_cast<u32*>(&_value);

        const u32 sign = (binaryFloat >> 31) & 0x1;
        const u32 exponent = (binaryFloat >> 23) & 0xff;
        const u32 mantissa = binaryFloat & 0x7fffff;

        // Zero, or float32 subnormal (far below half of the smallest float16 subnormal, so it rounds to zero)
        if (exponent == 0)
        {
            return sign << 15;
        }
        // Handle Nan and Infinity
        else if (exponent == 0xff)
        {
            if (mantissa == 0)
                // Infinity
                return (sign << 15) | (0x1f << 10);
            else
                // NaN
                return (sign << 15) | (0x1f << 10) | 0x3FF;
        }

        const s32 exponent16 = static_cast<s32>(exponent) - 127 + 15;

        // Rounds to nearest, ties to even, matching the IEEE 754 default (and the hardware conversions).
        // A round-up carry may propagate from the mantissa into the exponent, which yields the correct
        // encoding: subnormal -> smallest normal, or largest finite -> infinity.
        const auto roundToNearestEven = [](u32 _truncated, u32 _droppedBits, u32 _droppedBitCount) -> u32
        {
            const u32 halfway = 1u << (_droppedBitCount - 1);
            const u32 remainder = _droppedBits & ((1u << _droppedBitCount) - 1);
            const bool roundUp = remainder > halfway || (remainder == halfway && (_truncated & 1) != 0);
            return _truncated + (roundUp ? 1 : 0);
        };

        // Handle overflow and underflow
        if (exponent16 >= 31)
        {
            // Overflow to infinity
            return (sign << 15) | (0x1f << 10);
        }
        else if (exponent16 <= 0)
        {
            // Underflow or subnormal
            if (exponent16 >= -10)
            {
                // Convert to subnormal
                const u32 fullMantissa = mantissa | 0x800000;
                const u32 shift = 14 - exponent16;
                return (sign << 15) | roundToNearestEven(fullMantissa >> shift, fullMantissa, shift);
            }
            else
            {
                // Underflow to zero
                return sign << 15;
            }
        }
        return (sign << 15) | roundToNearestEven((exponent16 << 10) | (mantissa >> 13), mantissa, 13);
#endif
    }

    float Float16::ConvertFromFloat16(u16 _value)
    {
#if defined(KE_ARM_NEON)
        float16x4_t val { *reinterpret_cast<float16_t*>(&_value) };
        float32x4_t result = vcvt_f32_f16(val);
        return *reinterpret_cast<float*>(&result);
#else
#   if defined(KE_X86_F16C)
        if (HasF16C())
            return ConvertFromFloat16F16C(_value);
#   endif

        // Extract components from 16-bit float
        const u32 sign = (_value >> 15) & 0x1;
        u32 exponent = (_value >> 10) & 0x1F;
        u32 mantissa = _value & 0x3FF;

        u32 result = 0;

        if (exponent == 0) {
            // Zero or subnormal
            if (mantissa == 0) {
                // Zero
                result = sign << 31;
            } else {
                // Subnormal, normalize it
                int e = -14;
                while ((mantissa & 0x400) == 0) {
                    mantissa <<= 1;
                    e--;
                }
                mantissa &= 0x3FF;
                exponent = e + 15;

                // Convert to float32
                result = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
            }
        } else if (exponent == 0x1F) {
            // Infinity or NaN
            result = (sign << 31) | (0xFF << 23);
            if (mantissa != 0) {
                // NaN - preserve mantissa bits, setting at least one bit
                result |= (mantissa << 13) | 0x400000;
            }
        } else {
            // Normal number
            result = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
        }

        return *reinterpret_cast<float*>(&result);
#endif
    }

    u32 Float16::PackFloat16x2(const float a, const float b)
    {
#if defined(KE_ARM_NEON)
        float32x4_t fab { a, b, 0, 0 };
        float16x4_t hab = vcvt_f16_f32(fab);
        u16 out[4];
        vst1_u16(out, vreinterpret_u16_f16(hab));
        return static_cast<u32>(out[0]) | (static_cast<u32>(out[1]) << 16);
#else
#   if defined(KE_X86_F16C)
        if (HasF16C())
            return PackFloat16x2F16C(a, b);
#   endif

        return static_cast<u32>(ConvertToFloat16(a)) | (static_cast<u32>(ConvertToFloat16(b)) << 16);
#endif
    }
}