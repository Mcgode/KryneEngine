/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include <xsimd/xsimd.hpp>

#include "KryneEngine/Core/Common/Utils/Alignment.hpp"

namespace KryneEngine::Math
{
#if XSMID_WITH_SSE4_2
    using XsimdArch128 = xsimd::sse4_2;
#elif XSMID_WITH_SSE4_1
    using XsimdArch128 = xsimd::sse4_1;
#elif XSMID_WITH_SSE3
    using XsimdArch128 = xsimd::sse3;
#elif XSIMD_WITH_SSE2
    using XsimdArch128 = xsimd::sse2;
#elif XSIMD_WITH_SSE
    using XsimdArch128 = xsimd::sse;
#elif XSIMD_WITH_NEON64
    using XsimdArch128 = xsimd::neon64;
#elif XSIMD_WITH_NEON
    using XsimdArch128 = xsimd::neon;
#else
    // Fall back to basic scalar operations
    using XsimdArch128 = xsimd::unavailable;
#endif

#if XSIMD_WITH_AVX
    using XsimdArch256 = xsimd::avx;
#elif XSIMD_WITH_SVE
    using XsimdArch256 = xsimd::sve<256>;
#elif XSIMD_WITH_RVV
    using XsimdArch256 = xsimd::rvv<256>;
#else
    // Fall back to 128 bits SIMD
    using XsimdArch256 = XsimdArch128;
#endif

#if XSIMD_WITH_AVX512F
    using XsimdArch512 = xsimd::avx512f;
#elif XSIMD_WITH_SVE
    using XsimdArch512 = xsimd::sve<512>;
#elif XSIMD_WITH_RVV
    using XsimdArch512 = xsimd::rvv<512>;
#else
    // Fall back to 256 bits SIMD
    using XsimdArch512 = XsimdArch256;
#endif

    using SimdHighestArch =
        std::conditional_t<std::is_same_v<xsimd::unavailable, XsimdArch128>, xsimd::unavailable,
        std::conditional_t<std::is_same_v<XsimdArch128, XsimdArch256>, XsimdArch128,
        std::conditional_t<std::is_same_v<XsimdArch256, XsimdArch512>, XsimdArch256, XsimdArch512>>>;
    constexpr bool kSimdAvailable = !std::is_same_v<SimdHighestArch, xsimd::unavailable>;

    template <class T, class Container>
    struct SimdOperability
    {
        using OptimalArch = std::conditional_t<(sizeof(Container) == 32), XsimdArch256, XsimdArch128>;
        static constexpr size_t kBatchSize = xsimd::batch<T, OptimalArch>::size;
        static constexpr size_t kBatchCount = (kBatchSize > 4 || kBatchSize == 0) ? 1 : 4 / kBatchSize;
        static constexpr bool kSimdOperable =
            kBatchSize > 1 && Alignment::IsAligned(sizeof(Container), OptimalArch::alignment());
    };

    template<bool Aligned, class T, class A>
    xsimd::batch<T, A> XsimdLoad(const T* _ptr)
    {
        if constexpr (Aligned)
        {
            return xsimd::load_aligned(_ptr);
        }
        else
        {
            return xsimd::load_unaligned(_ptr);
        }
    }

    template<bool Aligned, class T, class A>
    void XsimdStore(T* _ptr, const xsimd::batch<T, A>& _batch)
    {
        if constexpr (Aligned)
        {
            _batch.store_aligned(_ptr);
        }
        else
        {
            _batch.store_unaligned(_ptr);
        }
    }
}
