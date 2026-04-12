/**
 * @file
 * @author Max Godefroy
 * @date 12/04/2026.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Common/Utils/Macros.hpp"
#include "KryneEngine/Core/Math/Simd/SimdTypes.hpp"

#if defined(__SSE2__)
#   include <emmintrin.h>
#endif

namespace KryneEngine::Simd
{
    KE_FORCEINLINE f32x4 LoadAligned(const float* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32(_data);
#elif defined(__SSE2__)
        return _mm_load_ps(_data);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 LoadUnaligned(const float* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32(_data);
#elif defined(__SSE2__)
        return _mm_loadu_ps(_data);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE f32x4 From(const float value)
    {
#if defined(__ARM_NEON)
        return vdupq_n_f32(value);
#elif defined(__SSE2__)
        return _mm_set1_ps(value);
#else
        f32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = value;
        return result;
#endif
    }

    KE_FORCEINLINE void StoreAligned(float* data, const f32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_f32(data, value);
#elif defined(__SSE2__)
        _mm_store_ps(data, value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    KE_FORCEINLINE void StoreUnaligned(float* data, const f32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_f32(data, value);
#elif defined(__SSE2__)
        _mm_storeu_ps(data, value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    KE_FORCEINLINE u32x4 LoadAligned(const u32* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_u32(_data);
#elif defined(__SSE2__)
        return _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE u32x4 LoadUnaligned(const u32* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_u32(_data);
#elif defined(__SSE2__)
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(_data));
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE u32x4 From(const u32 value)
    {
#if defined(__ARM_NEON)
        return vdupq_n_u32(value);
#elif defined(__SSE2__)
        return _mm_set1_epi32(value);
#else
        u32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = value;
        return result;
#endif
    }

    KE_FORCEINLINE void StoreAligned(u32* data, const u32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_u32(data, value);
#elif defined(__SSE2__)
        _mm_store_si128(reinterpret_cast<__m128i*>(data), value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    KE_FORCEINLINE void StoreUnaligned(u32* data, const u32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_u32(data, value);
#elif defined(__SSE2__)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data), value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    KE_FORCEINLINE s32x4 LoadAligned(const s32* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_s32(_data);
#elif defined(__SSE2__)
        return _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 LoadUnaligned(const s32* _data)
    {
#if defined(__ARM_NEON)
        return vld1q_s32(_data);
#elif defined(__SSE2__)
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(_data));
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = _data[i];
        return result;
#endif
    }

    KE_FORCEINLINE s32x4 From(const s32 value)
    {
#if defined(__ARM_NEON)
        return vdupq_n_s32(value);
#elif defined(__SSE2__)
        return _mm_set1_epi32(value);
#else
        s32x4 result;
        for (int i = 0; i < 4; ++i)
            result.m_value[i] = value;
        return result;
#endif
    }

    KE_FORCEINLINE void StoreAligned(s32* data, const s32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_s32(data, value);
#elif defined(__SSE2__)
        _mm_store_si128(reinterpret_cast<__m128i*>(data), value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    KE_FORCEINLINE void StoreUnaligned(s32* data, const s32x4 value)
    {
#if defined(__ARM_NEON)
        vst1q_s32(data, value);
#elif defined(__SSE2__)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data), value);
#else
        for (int i = 0; i < 4; ++i)
            data[i] = value.m_value[i];
#endif
    }

    /**
     * @defgroup f32x4x3 Memory Operations
     */

    KE_FORCEINLINE f32x4x3 LoadAlignedMat34(const float* data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32_x3(data);
#else
        f32x4x3 result;
        for (int i = 0; i < 3; ++i)
            result[i] = LoadAligned(data + i * 4);
        return result;
#endif
    }

    KE_FORCEINLINE f32x4x3 LoadUnalignedMat34(const float* data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32_x3(data);
#else
        f32x4x3 result;
        for (int i = 0; i < 3; ++i)
            result[i] = LoadUnaligned(data + i * 4);
        return result;
#endif
    }

    KE_FORCEINLINE void StoreAlignedMat34(float* data, const f32x4x3& value)
    {
#if defined(__ARM_NEON)
        vst1q_f32_x3(data, value);
#else
        for (int i = 0; i < 3; ++i)
            StoreAligned(data + i * 4, value[i]);
#endif
    }

    KE_FORCEINLINE void StoreUnalignedMat34(float* data, const f32x4x3& value)
    {
#if defined(__ARM_NEON)
        vst1q_f32_x3(data, value);
#else
        for (int i = 0; i < 3; ++i)
            StoreUnaligned(data + i * 4, value[i]);
#endif
    }

    /**
     * @defgroup f32x4x4 Memory Operations
     */

    KE_FORCEINLINE f32x4x4 LoadAlignedMat44(const float* data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32_x4(data);
#else
        f32x4x4 result;
        for (int i = 0; i < 4; ++i)
            result[i] = LoadAligned(data + i * 4);
        return result;
#endif
    }

    KE_FORCEINLINE f32x4x4 LoadUnalignedMat44(const float* data)
    {
#if defined(__ARM_NEON)
        return vld1q_f32_x4(data);
#else
        f32x4x4 result;
        for (int i = 0; i < 4; ++i)
            result[i] = LoadUnaligned(data + i * 4);
        return result;
#endif
    }

    KE_FORCEINLINE void StoreAlignedMat44(float* data, const f32x4x4& value)
    {
#if defined(__ARM_NEON)
        vst1q_f32_x4(data, value);
#else
        for (int i = 0; i < 4; ++i)
            StoreAligned(data + i * 4, value[i]);
#endif
    }

    KE_FORCEINLINE void StoreUnalignedMat44(float* data, const f32x4x4& value)
    {
#if defined(__ARM_NEON)
        vst1q_f32_x4(data, value);
#else
        for (int i = 0; i < 4; ++i)
            StoreUnaligned(data + i * 4, value[i]);
#endif
    }
}