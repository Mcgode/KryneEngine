/**
 * @file
 * @author Max Godefroy
 * @date 28/11/2024.
 */

#pragma once

#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Math/Vector2.hpp"
#include "KryneEngine/Core/Math/Vector3.hpp"
#include "KryneEngine/Core/Math/Vector4.hpp"

namespace KryneEngine
{
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"

    using float2 = Math::Vector2Base<float>;
    using int2 = Math::Vector2Base<s32>;
    using uint2 = Math::Vector2Base<u32>;
    using double2 = Math::Vector2Base<double>;

    using float3 = Math::Vector3Base<float>;
    using int3 = Math::Vector3Base<s32>;
    using uint3 = Math::Vector3Base<u32>;
    using double3 = Math::Vector3Base<double>;

    using float4 = Math::Vector4Base<float>;
    using int4 = Math::Vector4Base<s32>;
    using uint4 = Math::Vector4Base<u32>;
    using double4 = Math::Vector4Base<double>;

    using float4_simd = Math::Vector4Base<float, true>;
    using int4_simd = Math::Vector4Base<s32, true>;
    using uint4_simd = Math::Vector4Base<u32, true>;
    using double4_simd = Math::Vector4Base<double, true>;

#pragma clang diagnostic pop
}