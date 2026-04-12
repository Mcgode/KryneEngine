/**
 * @file
 * @author Max Godefroy
 * @date 14/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Math/Matrix33.hpp"
#include "KryneEngine/Core/Math/Matrix44.hpp"

#ifndef KE_DEFAULT_MATRIX_ROW_MAJOR
#   define KE_DEFAULT_MATRIX_ROW_MAJOR 1
#endif

namespace KryneEngine
{
    using float3x3 = Math::Matrix33Base<float, KE_DEFAULT_MATRIX_ROW_MAJOR>;
    using double3x3 = Math::Matrix33Base<double, KE_DEFAULT_MATRIX_ROW_MAJOR>;

    using float4x4 = Math::Matrix44Base<float, KE_DEFAULT_MATRIX_ROW_MAJOR, false>;
    using double4x4 = Math::Matrix44Base<double, KE_DEFAULT_MATRIX_ROW_MAJOR, false>;
    using float4x4_simd = Math::Matrix44Base<float, KE_DEFAULT_MATRIX_ROW_MAJOR, false>;
    using double4x4_simd = Math::Matrix44Base<double, KE_DEFAULT_MATRIX_ROW_MAJOR, false>;

    template<class T, bool RowMajor,  bool SimdAligned>
    Math::Matrix44Base<T, RowMajor, SimdAligned> ToMatrix44(const Math::Matrix33Base<T, RowMajor>& _matrix)
    {
        return Math::Matrix44Base<T, RowMajor, SimdAligned> {
            Math::Vector4Base<T, SimdAligned>{ _matrix.m_vectors[0], 0.f },
            Math::Vector4Base<T, SimdAligned>{ _matrix.m_vectors[1], 0.f },
            Math::Vector4Base<T, SimdAligned>{ _matrix.m_vectors[2], 0.f },
            Math::Vector4Base<T, SimdAligned>{ 0.0f, 0.0f, 0.0f, 1.0f }
        };
    }

    template<class T, bool RowMajor, bool SimdAligned>
    Math::Matrix33Base<T, RowMajor> ToMatrix33(const Math::Matrix44Base<T, RowMajor, SimdAligned>& _matrix)
    {
        using Matrix = Math::Matrix33Base<T, RowMajor>;
        return Matrix {
            Matrix::VectorType(_matrix.m_vectors[0].x, _matrix.m_vectors[0].y, _matrix.m_vectors[0].z),
            Matrix::VectorType(_matrix.m_vectors[1].x, _matrix.m_vectors[1].y, _matrix.m_vectors[1].z),
            Matrix::VectorType(_matrix.m_vectors[2].x, _matrix.m_vectors[2].y, _matrix.m_vectors[2].z)
        };
    }
}