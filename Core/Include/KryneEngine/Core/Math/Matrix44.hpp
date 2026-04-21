/**
 * @file
 * @author Max Godefroy
 * @date 15/03/2025.
 */

#pragma once

#include "KryneEngine/Core/Math/Vector4.hpp"

namespace KryneEngine::Math
{
    template<class T, bool RowMajor, bool SimdAligned>
    struct Matrix44Base
    {
        using ScalarType = T;
        static constexpr bool kSimdAligned = SimdAligned;
        static constexpr bool kRowMajorLayout = RowMajor;
        using VectorType = Vector4Base<T, SimdAligned>;

        Matrix44Base()
            : m_vectors{
                VectorType{ 1, 0, 0, 0 },
                VectorType{ 0, 1, 0, 0 },
                VectorType{ 0, 0, 1, 0 },
                VectorType{ 0, 0, 0, 1 }}
        {}

        ~Matrix44Base() = default;

        Matrix44Base(const Matrix44Base&) = default;
        Matrix44Base(Matrix44Base&&) = default;
        Matrix44Base& operator=(const Matrix44Base&) = default;
        Matrix44Base& operator=(Matrix44Base&&) = default;

        template<class U>
        requires std::is_constructible_v<T, U>
        Matrix44Base(
            U _a11, U _a12, U _a13, U _a14,
            U _a21, U _a22, U _a23, U _a24,
            U _a31, U _a32, U _a33, U _a34,
            U _a41, U _a42, U _a43, U _a44) requires (kRowMajorLayout)
            : m_vectors(
                  VectorType { _a11, _a12, _a13, _a14 },
                  VectorType { _a21, _a22, _a23, _a24 },
                  VectorType { _a31, _a32, _a33, _a34 },
                  VectorType { _a41, _a42, _a43, _a44 }
              )
        {}

        template<class U>
        requires std::is_constructible_v<T, U>
        Matrix44Base(
            U _a11, U _a12, U _a13, U _a14,
            U _a21, U _a22, U _a23, U _a24,
            U _a31, U _a32, U _a33, U _a34,
            U _a41, U _a42, U _a43, U _a44) requires (!kRowMajorLayout)
            : m_vectors (
                  VectorType { _a11, _a21, _a31, _a41 },
                  VectorType { _a12, _a22, _a32, _a42 },
                  VectorType { _a13, _a23, _a33, _a43 },
                  VectorType { _a14, _a24, _a34, _a44 }
              )
        {}

        template<class U, bool S>
        requires std::is_constructible_v<T, U>
        Matrix44Base(Vector4Base<U, S> _v0, Vector4Base<U, S> _v1, Vector4Base<U, S> _v2, Vector4Base<U, S> _v3)
            : m_vectors{
                  VectorType { _v0 },
                  VectorType { _v1 },
                  VectorType { _v2 },
                  VectorType { _v3 },
              }
        {}

        template <class U, bool S>
        requires std::is_constructible_v<T, U>
        explicit Matrix44Base(const Matrix44Base<U, RowMajor, S>& _other)
            : Matrix44Base(_other.m_vectors[0], _other.m_vectors[1], _other.m_vectors[2], _other.m_vectors[3])
        {}

        T& Get(size_t _row, size_t _col);
        const T& Get(size_t _row, size_t _col) const;
        T* GetPtr() { return m_vectors[0].GetPtr(); }
        const T* GetPtr() const { return m_vectors[0].GetPtr(); }

        Matrix44Base operator+(const Matrix44Base& _other) const;
        Matrix44Base operator-(const Matrix44Base& _other) const;
        Matrix44Base operator*(const Matrix44Base& _other) const;

        VectorType operator*(const VectorType& _other) const;

        bool operator==(const Matrix44Base& _other) const
        {
            return m_vectors[0] == _other.m_vectors[0]
                   && m_vectors[1] == _other.m_vectors[1]
                   && m_vectors[2] == _other.m_vectors[2]
                   && m_vectors[3] == _other.m_vectors[3];
        }

        Matrix44Base& Transpose();
        [[nodiscard]] Matrix44Base Transposed() const;

        VectorType m_vectors[4];

        [[nodiscard]] T Determinant() const;
        [[nodiscard]] Matrix44Base Inverse() const;

        /**
         * @brief Converts a given 3x3 matrix from a row-major type to a colum-major type, or vice versa.
         *
         * @details
         * This static method performs a matrix conversion by transposing the input matrix and constructing a new matrix
         * in the target layout and type. The operation assumes that the input matrix is in the opposite row-major/column-major
         * layout compared to the target matrix.
         *
         * @tparam U The type of the elements within the input matrix to be converted. This type must be convertible to the
         *         type of the elements of the target matrix `T`.
         *
         * @param _other The input matrix to be converted to the target type and layout. This matrix must be in the opposite
         *        row-major/column-major layout relative to the target matrix.
         *
         * @return A new Matrix33Base object with the specified type `T`, layout, and optimization, containing the transposed
         *         and converted data from the input matrix.
         *
         * @note The input matrix's type and layout constraints are enforced through a `requires` clause using `eastl::is_convertible_v<U, T>`.
         */
        template <class U, bool S>
        requires eastl::is_convertible_v<U, T>
        static Matrix44Base Convert(const Matrix44Base<U, !RowMajor, S>& _other)
        {
            Matrix44Base<U, !RowMajor, S> transposed = _other;
            transposed.Transpose();
            return Matrix44Base(transposed.m_vectors[0], transposed.m_vectors[1], transposed.m_vectors[2], transposed.m_vectors[3]);
        }
    };

    template<class T>
    concept Matrix44Type = requires {
        typename T::ScalarType;
        T::kSimdAligned;
        T::kRowMajorLayout;
        std::is_same_v<Matrix44Base<typename T::ScalarType, T::kRowMajorLayout, T::kSimdAligned>, T>;
    };
}
