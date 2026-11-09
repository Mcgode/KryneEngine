/**
 * @file
 * @author Max Godefroy
 * @date 13/03/2025.
 */

#include "KryneEngine/Core/Math/Matrix33.hpp"

namespace KryneEngine::Math
{
    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor>::Matrix33Base()
        : m_vectors {
              VectorType( 1, 0, 0 ),
              VectorType( 0, 1, 0 ),
              VectorType( 0, 0, 1 ),
          }
    {}

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor>::Matrix33Base(T _a11, T _a12, T _a13, T _a21, T _a22, T _a23, T _a31, T _a32, T _a33)
    {
        if constexpr (RowMajor)
        {
            m_vectors[0] = VectorType(_a11, _a12, _a13);
            m_vectors[1] = VectorType(_a21, _a22, _a23);
            m_vectors[2] = VectorType(_a31, _a32, _a33);
        }
        else
        {
            m_vectors[0] = VectorType(_a11, _a21, _a31);
            m_vectors[1] = VectorType(_a12, _a22, _a32);
            m_vectors[2] = VectorType(_a13, _a23, _a33);
        }
    }

    template <class T, bool RowMajor>
    T& Matrix33Base<T, RowMajor>::Get(size_t _row, size_t _col)
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

    template <class T, bool RowMajor>
    const T& Matrix33Base<T, RowMajor>::Get(size_t _row, size_t _col) const
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

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor> Matrix33Base<T, RowMajor>::operator+(
        const Matrix33Base& _other) const
    {
        Matrix33Base result {};
        result.m_vectors[0] = m_vectors[0] + _other.m_vectors[0];
        result.m_vectors[1] = m_vectors[1] + _other.m_vectors[1];
        result.m_vectors[2] = m_vectors[2] + _other.m_vectors[2];
        return result;
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor> Matrix33Base<T, RowMajor>::operator-(
        const Matrix33Base& _other) const
    {
        Matrix33Base result {};
        result.m_vectors[0] = m_vectors[0] - _other.m_vectors[0];
        result.m_vectors[1] = m_vectors[1] - _other.m_vectors[1];
        result.m_vectors[2] = m_vectors[2] - _other.m_vectors[2];
        return result;
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor> Matrix33Base<T, RowMajor>::operator*(
        const Matrix33Base& _other) const
    {
        return Matrix33Base(
            Get(0, 0) * _other.Get(0, 0) + Get(0, 1) * _other.Get(1, 0) + Get(0, 2) * _other.Get(2, 0),
            Get(0, 0) * _other.Get(0, 1) + Get(0, 1) * _other.Get(1, 1) + Get(0, 2) * _other.Get(2, 1),
            Get(0, 0) * _other.Get(0, 2) + Get(0, 1) * _other.Get(1, 2) + Get(0, 2) * _other.Get(2, 2),
            Get(1, 0) * _other.Get(0, 0) + Get(1, 1) * _other.Get(1, 0) + Get(1, 2) * _other.Get(2, 0),
            Get(1, 0) * _other.Get(0, 1) + Get(1, 1) * _other.Get(1, 1) + Get(1, 2) * _other.Get(2, 1),
            Get(1, 0) * _other.Get(0, 2) + Get(1, 1) * _other.Get(1, 2) + Get(1, 2) * _other.Get(2, 2),
            Get(2, 0) * _other.Get(0, 0) + Get(2, 1) * _other.Get(1, 0) + Get(2, 2) * _other.Get(2, 0),
            Get(2, 0) * _other.Get(0, 1) + Get(2, 1) * _other.Get(1, 1) + Get(2, 2) * _other.Get(2, 1),
            Get(2, 0) * _other.Get(0, 2) + Get(2, 1) * _other.Get(1, 2) + Get(2, 2) * _other.Get(2, 2)
        );
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor>::VectorType Matrix33Base<T, RowMajor>::operator*(const VectorType& _vector) const
    {
        // Members are accessed directly: `operator[]` and `Dot` are defined in Vector3.cpp and can't be inlined here.
        const VectorType& v0 = m_vectors[0];
        const VectorType& v1 = m_vectors[1];
        const VectorType& v2 = m_vectors[2];

        if constexpr (RowMajor)
        {
            return {
                v0.x * _vector.x + v0.y * _vector.y + v0.z * _vector.z,
                v1.x * _vector.x + v1.y * _vector.y + v1.z * _vector.z,
                v2.x * _vector.x + v2.y * _vector.y + v2.z * _vector.z
            };
        }
        else
        {
            return {
                v0.x * _vector.x + v1.x * _vector.y + v2.x * _vector.z,
                v0.y * _vector.x + v1.y * _vector.y + v2.y * _vector.z,
                v0.z * _vector.x + v1.z * _vector.y + v2.z * _vector.z
            };
        }
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor>& Matrix33Base<T, RowMajor>::Transpose()
    {
        std::swap<T>(m_vectors[0][1], m_vectors[1][0]);
        std::swap<T>(m_vectors[0][2], m_vectors[2][0]);
        std::swap<T>(m_vectors[1][2], m_vectors[2][1]);
        return *this;
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor> Matrix33Base<T, RowMajor>::Transposed() const
    {
        Matrix33Base transposed = *this;
        transposed.Transpose();
        return transposed;
    }

    template <class T, bool RowMajor>
    T Matrix33Base<T, RowMajor>::Determinant() const
    {
        // det(M^T) = det(M), so formula is layout-independent.

        const T a = m_vectors[0].x,
                b = m_vectors[0].y,
                c = m_vectors[0].z,
                d = m_vectors[1].x,
                e = m_vectors[1].y,
                f = m_vectors[1].z,
                g = m_vectors[2].x,
                h = m_vectors[2].y,
                i = m_vectors[2].z;

        return a * (e * i - f * h)
             + b * (f * g - d * i)
             + c * (d * h - e * g);
    }

    template <class T, bool RowMajor>
    Matrix33Base<T, RowMajor> Matrix33Base<T, RowMajor>::Inverse() const
    requires std::is_floating_point_v<T>
    {
        const T a = m_vectors[0].x,
                b = m_vectors[0].y,
                c = m_vectors[0].z,
                d = m_vectors[1].x,
                e = m_vectors[1].y,
                f = m_vectors[1].z,
                g = m_vectors[2].x,
                h = m_vectors[2].y,
                i = m_vectors[2].z;

        const T det = a * (e * i - f * h)
                    + b * (f * g - d * i)
                    + c * (d * h - e * g);

        KE_ASSERT(det != 0);

        Matrix33Base result;
        T* mat = result.m_vectors[0].GetPtr();

        const T invDet = 1 / det;
        mat[0] = (e * i - f * h) * invDet;
        mat[1] = (c * h - b * i) * invDet;
        mat[2] = (b * f - c * e) * invDet;
        mat[3] = (f * g - d * i) * invDet;
        mat[4] = (a * i - c * g) * invDet;
        mat[5] = (c * d - a * f) * invDet;
        mat[6] = (d * h - e * g) * invDet;
        mat[7] = (b * g - a * h) * invDet;
        mat[8] = (a * e - b * d) * invDet;

        return result;
    }

#define IMPLEMENTATION_INDIVIDUAL(type, rowMajor) \
    template struct Matrix33Base<type, rowMajor>

#define IMPLEMENTATION(type)                  \
    IMPLEMENTATION_INDIVIDUAL(type, true);    \
    IMPLEMENTATION_INDIVIDUAL(type, false)

    IMPLEMENTATION(float);
    IMPLEMENTATION(double);

#undef IMPLEMENTATION
#undef IMPLEMENTATION_INDIVIDUAL
}