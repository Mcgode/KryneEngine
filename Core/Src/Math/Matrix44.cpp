/**
 * @file
 * @author Max Godefroy
 * @date 15/03/2025.
 */

#include "KryneEngine/Core/Math/Matrix44.hpp"

#include "KryneEngine/Core/Math/Simd/SimdArithmeticOperations.hpp"
#include "KryneEngine/Core/Math/Simd/SimdMathOperations.hpp"
#include "KryneEngine/Core/Math/Simd/SimdMemoryOperations.hpp"
#include "KryneEngine/Core/Math/Simd/VectorUtils.hpp"

namespace KryneEngine::Math
{
    template <class T, bool RowMajor, bool SimdAligned>
    T& Matrix44Base<T, RowMajor, SimdAligned>::Get(size_t _row, size_t _col)
    {
        if constexpr (RowMajor)
        {
            return m_vectors[_row][_col];
        }
        else
        {
            return m_vectors[_col][_row];
        }
    }

    template <class T, bool RowMajor, bool SimdAligned>
    const T& Matrix44Base<T, RowMajor, SimdAligned>::Get(size_t _row, size_t _col) const
    {
        if constexpr (RowMajor)
        {
            return m_vectors[_row][_col];
        }
        else
        {
            return m_vectors[_col][_row];
        }
    }

    template <typename T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::operator+(const Matrix44Base& _other) const
    {
        return Matrix44Base(
            m_vectors[0] + _other.m_vectors[0],
            m_vectors[1] + _other.m_vectors[1],
            m_vectors[2] + _other.m_vectors[2],
            m_vectors[3] + _other.m_vectors[3]);
    }

    template <typename T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::operator-(const Matrix44Base& _other) const
    {
        return Matrix44Base(
            m_vectors[0] - _other.m_vectors[0],
            m_vectors[1] - _other.m_vectors[1],
            m_vectors[2] - _other.m_vectors[2],
            m_vectors[3] - _other.m_vectors[3]);
    }

    template <typename T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::operator*(const Matrix44Base& _other) const
    {
        if constexpr (std::is_same_v<T, float> && Simd::kIsAvailable)
        {
            const Simd::f32x4x4 a = SimdAligned ? Simd::LoadAlignedMat44(GetPtr()) : Simd::LoadUnalignedMat44(GetPtr());
            const Simd::f32x4x4 b = SimdAligned ? Simd::LoadAlignedMat44(_other.GetPtr()) : Simd::LoadUnalignedMat44(_other.GetPtr());

            const Simd::f32x4x4 r = RowMajor ? Simd::Multiply(a, b) : Simd::Multiply(b, a);

            Matrix44Base result;
            if constexpr (SimdAligned)
                Simd::StoreAlignedMat44(result.m_vectors[0].GetPtr(), r);
            else
                Simd::StoreUnalignedMat44(result.m_vectors[0].GetPtr(), r);
            return result;
        }
        else
        {
            using Vector4 = Vector4Base<T, SimdAligned>;
            const Vector4* vas = RowMajor ? m_vectors : _other.m_vectors;
            const Vector4* vbs = RowMajor ? _other.m_vectors : m_vectors;
            return {
                Vector4 {
                    vas[0].x * vbs[0].x + vas[0].y * vbs[1].x + vas[0].z * vbs[2].x + vas[0].w * vbs[3].x,
                    vas[0].x * vbs[0].y + vas[0].y * vbs[1].y + vas[0].z * vbs[2].y + vas[0].w * vbs[3].y,
                    vas[0].x * vbs[0].z + vas[0].y * vbs[1].z + vas[0].z * vbs[2].z + vas[0].w * vbs[3].z,
                    vas[0].x * vbs[0].w + vas[0].y * vbs[1].w + vas[0].z * vbs[2].w + vas[0].w * vbs[3].w
                },
                Vector4 {
                    vas[1].x * vbs[0].x + vas[1].y * vbs[1].x + vas[1].z * vbs[2].x + vas[1].w * vbs[3].x,
                    vas[1].x * vbs[0].y + vas[1].y * vbs[1].y + vas[1].z * vbs[2].y + vas[1].w * vbs[3].y,
                    vas[1].x * vbs[0].z + vas[1].y * vbs[1].z + vas[1].z * vbs[2].z + vas[1].w * vbs[3].z,
                    vas[1].x * vbs[0].w + vas[1].y * vbs[1].w + vas[1].z * vbs[2].w + vas[1].w * vbs[3].w
                },
                Vector4 {
                    vas[2].x * vbs[0].x + vas[2].y * vbs[1].x + vas[2].z * vbs[2].x + vas[2].w * vbs[3].x,
                    vas[2].x * vbs[0].y + vas[2].y * vbs[1].y + vas[2].z * vbs[2].y + vas[2].w * vbs[3].y,
                    vas[2].x * vbs[0].z + vas[2].y * vbs[1].z + vas[2].z * vbs[2].z + vas[2].w * vbs[3].z,
                    vas[2].x * vbs[0].w + vas[2].y * vbs[1].w + vas[2].z * vbs[2].w + vas[2].w * vbs[3].w
                },
                Vector4 {
                    vas[3].x * vbs[0].x + vas[3].y * vbs[1].x + vas[3].z * vbs[2].x + vas[3].w * vbs[3].x,
                    vas[3].x * vbs[0].y + vas[3].y * vbs[1].y + vas[3].z * vbs[2].y + vas[3].w * vbs[3].y,
                    vas[3].x * vbs[0].z + vas[3].y * vbs[1].z + vas[3].z * vbs[2].z + vas[3].w * vbs[3].z,
                    vas[3].x * vbs[0].w + vas[3].y * vbs[1].w + vas[3].z * vbs[2].w + vas[3].w * vbs[3].w
                }
            };
        }
    }

    template <class T, bool RowMajor, bool SimdAligned>
    Vector4Base<T, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::operator*(const Vector4Base<T, SimdAligned>& _other) const
    {
        if constexpr (std::is_same_v<T, float> && Simd::kIsAvailable)
        {
            // From benchmarking, pre-transposing the matrix then doing vector multiplication is more performant, as
            // row major layout requires the use of sum reduction.

            const Simd::f32x4x4 m = SimdAligned
                ? (RowMajor ? Simd::LoadAlignedMat44Transposed(GetPtr()) : Simd::LoadAlignedMat44(GetPtr()))
                : (RowMajor ? Simd::LoadUnalignedMat44Transposed(GetPtr()) : Simd::LoadUnalignedMat44(GetPtr()));
            const Simd::f32x4 v = Simd::From(_other);

            const Simd::f32x4 r = Simd::MultiplyTransposed(m, v);

            Vector4Base<T, SimdAligned> result;
            Simd::Store(r, result);
            return result;
        }
        else
        {
            if constexpr (RowMajor)
            {
                return {
                    Dot(m_vectors[0], _other),
                    Dot(m_vectors[1], _other),
                    Dot(m_vectors[2], _other),
                    Dot(m_vectors[3], _other),
                };
            }
            else
            {
                return {
                    m_vectors[0][0] * _other[0] + m_vectors[1][0] * _other[1] + m_vectors[2][0] * _other[2] + m_vectors[3][0] * _other[3],
                    m_vectors[0][1] * _other[0] + m_vectors[1][1] * _other[1] + m_vectors[2][1] * _other[2] + m_vectors[3][1] * _other[3],
                    m_vectors[0][2] * _other[0] + m_vectors[1][2] * _other[1] + m_vectors[2][2] * _other[2] + m_vectors[3][2] * _other[3],
                    m_vectors[0][3] * _other[0] + m_vectors[1][3] * _other[1] + m_vectors[2][3] * _other[2] + m_vectors[3][3] * _other[3],
                };
            }
        }
    }

    template <class T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned>& Matrix44Base<T, RowMajor, SimdAligned>::Transpose()
    {
        if constexpr (std::is_same_v<T, float> && Simd::kIsAvailable)
        {
            const Simd::f32x4x4 mat = SimdAligned
                ? Simd::LoadAlignedMat44Transposed(GetPtr())
                : Simd::LoadUnalignedMat44Transposed(GetPtr());

            if constexpr (SimdAligned)
                Simd::StoreAlignedMat44(GetPtr(), mat);
            else
                Simd::StoreUnalignedMat44(GetPtr(), mat);
        }
        else
        {
            std::swap(m_vectors[0][1], m_vectors[1][0]);
            std::swap(m_vectors[0][2], m_vectors[2][0]);
            std::swap(m_vectors[0][3], m_vectors[3][0]);
            std::swap(m_vectors[1][2], m_vectors[2][1]);
            std::swap(m_vectors[1][3], m_vectors[3][1]);
            std::swap(m_vectors[2][3], m_vectors[3][2]);
        }

        return *this;
    }

    template <typename T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::Transposed() const
    {
        Matrix44Base result { *this };
        result.Transpose();
        return result;
    }

    template <class T, bool RowMajor, bool SimdAligned>
    T Matrix44Base<T, RowMajor, SimdAligned>::Determinant() const
    {
        // Don't care about the layout, as the determinant of the transpose has the same value
        // https://en.wikipedia.org/wiki/Determinant#Transpose

        // Implementation based on the one from Assimp
        const T a0 = m_vectors[0][0],
                a1 = m_vectors[0][1],
                a2 = m_vectors[0][2],
                a3 = m_vectors[0][3],
                b0 = m_vectors[1][0],
                b1 = m_vectors[1][1],
                b2 = m_vectors[1][2],
                b3 = m_vectors[1][3],
                c0 = m_vectors[2][0],
                c1 = m_vectors[2][1],
                c2 = m_vectors[2][2],
                c3 = m_vectors[2][3],
                d0 = m_vectors[3][0],
                d1 = m_vectors[3][1],
                d2 = m_vectors[3][2],
                d3 = m_vectors[3][3];

        return a0*b1*c2*d3 - a0*b1*c3*d2 + a0*b2*c3*d1 - a0*b2*c1*d3 + a0*b3*c1*d2 - a0*b3*c2*d1
            - a1*b2*c3*d0 + a1*b2*c0*d3 - a1*b3*c0*d2 + a1*b3*c2*d0 - a1*b0*c2*d3 + a1*b0*c3*d2
            + a2*b3*c0*d1 - a2*b3*c1*d0 + a2*b0*c1*d3 - a2*b0*c3*d1 + a2*b1*c3*d0 - a2*b1*c0*d3
            - a3*b0*c1*d2 + a3*b0*c2*d1 - a3*b1*c2*d0 + a3*b1*c0*d2 - a3*b2*c0*d1 + a3*b2*c1*d0;
    }

    template <class T, bool RowMajor, bool SimdAligned>
    Matrix44Base<T, RowMajor, SimdAligned> Matrix44Base<T, RowMajor, SimdAligned>::Inverse() const
    {
        // Transpose of an inverse is the inverse of a transpose, so layout is irrelevant.

        Matrix44Base result;

        if constexpr (std::is_same_v<T, float> && Simd::kIsAvailable)
        {
            const Simd::f32x4x4 mat = SimdAligned
                ? Simd::LoadAlignedMat44(GetPtr())
                : Simd::LoadUnalignedMat44(GetPtr());

            const Simd::f32x4x4 inv = Simd::Inverse(mat);

            if constexpr (SimdAligned)
                Simd::StoreAlignedMat44(result.GetPtr(), inv);
            else
                Simd::StoreUnalignedMat44(result.GetPtr(), inv);
        }
        else
        {
            // Taken from assimp implementation

            const T invDet = 1.0 / Determinant();
            const T a0 = m_vectors[0][0],
                    a1 = m_vectors[0][1],
                    a2 = m_vectors[0][2],
                    a3 = m_vectors[0][3],
                    b0 = m_vectors[1][0],
                    b1 = m_vectors[1][1],
                    b2 = m_vectors[1][2],
                    b3 = m_vectors[1][3],
                    c0 = m_vectors[2][0],
                    c1 = m_vectors[2][1],
                    c2 = m_vectors[2][2],
                    c3 = m_vectors[2][3],
                    d0 = m_vectors[3][0],
                    d1 = m_vectors[3][1],
                    d2 = m_vectors[3][2],
                    d3 = m_vectors[3][3];
            
            result.m_vectors[0] = Vector4Base<T, SimdAligned> {
                invDet * (b1 * (c2 * d3 - c3 * d2) + b2 * (c3 * d1 - c1 * d3) + b3 * (c1 * d2 - c2 * d1)),
                -invDet * (a1 * (c2 * d3 - c3 * d2) + a2 * (c3 * d1 - c1 * d3) + a3 * (c1 * d2 - c2 * d1)),
                invDet * (a1 * (b2 * d3 - b3 * d2) + a2 * (b3 * d1 - b1 * d3) + a3 * (b1 * d2 - b2 * d1)),
                -invDet * (a1 * (b2 * c3 - b3 * c2) + a2 * (b3 * c1 - b1 * c3) + a3 * (b1 * c2 - b2 * c1)),
            };
            result.m_vectors[1] = Vector4Base<T, SimdAligned> {
                -invDet * (b0 * (c2 * d3 - c3 * d2) + b2 * (c3 * d0 - c0 * d3) + b3 * (c0 * d2 - c2 * d0)),
                invDet * (a0 * (c2 * d3 - c3 * d2) + a2 * (c3 * d0 - c0 * d3) + a3 * (c0 * d2 - c2 * d0)),
                -invDet * (a0 * (b2 * d3 - b3 * d2) + a2 * (b3 * d0 - b0 * d3) + a3 * (b0 * d2 - b2 * d0)),
                invDet * (a0 * (b2 * c3 - b3 * c2) + a2 * (b3 * c0 - b0 * c3) + a3 * (b0 * c2 - b2 * c0)),
            };
            result.m_vectors[2] = Vector4Base<T, SimdAligned> {
                invDet * (b0 * (c1 * d3 - c3 * d1) + b1 * (c3 * d0 - c0 * d3) + b3 * (c0 * d1 - c1 * d0)),
                -invDet * (a0 * (c1 * d3 - c3 * d1) + a1 * (c3 * d0 - c0 * d3) + a3 * (c0 * d1 - c1 * d0)),
                invDet * (a0 * (b1 * d3 - b3 * d1) + a1 * (b3 * d0 - b0 * d3) + a3 * (b0 * d1 - b1 * d0)),
                -invDet * (a0 * (b1 * c3 - b3 * c1) + a1 * (b3 * c0 - b0 * c3) + a3 * (b0 * c1 - b1 * c0)),
            };
            result.m_vectors[3] = Vector4Base<T, SimdAligned> {
                -invDet * (b0 * (c1 * d2 - c2 * d1) + b1 * (c2 * d0 - c0 * d2) + b2 * (c0 * d1 - c1 * d0)),
                invDet * (a0 * (c1 * d2 - c2 * d1) + a1 * (c2 * d0 - c0 * d2) + a2 * (c0 * d1 - c1 * d0)),
                -invDet * (a0 * (b1 * d2 - b2 * d1) + a1 * (b2 * d0 - b0 * d2) + a2 * (b0 * d1 - b1 * d0)),
                invDet * (a0 * (b1 * c2 - b2 * c1) + a1 * (b2 * c0 - b0 * c2) + a2 * (b0 * c1 - b1 * c0)),
            };
        }

        return result;
    }

#define IMPLEMENTATION_INDIVIDUAL(type, RowMajor, SimdAligned) \
    template struct Matrix44Base<type, RowMajor, SimdAligned>

#define IMPLEMENTATION(type)                        \
    IMPLEMENTATION_INDIVIDUAL(type, true, true);    \
    IMPLEMENTATION_INDIVIDUAL(type, true, false);   \
    IMPLEMENTATION_INDIVIDUAL(type, false, false);  \
    IMPLEMENTATION_INDIVIDUAL(type, false, true)

    IMPLEMENTATION(float);
    IMPLEMENTATION(double);

#undef IMPLEMENTATION
#undef IMPLEMENTATION_INDIVIDUAL
}