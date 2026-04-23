/**
 * @file
 * @author Max Godefroy
 * @date 13/03/2025.
 */

#pragma once

#include <cstddef>
#include <EASTL/type_traits.h>

#include "KryneEngine/Core/Math/Vector4.hpp"

namespace KryneEngine::Math
{
    template<class T, bool RowMajor>
    struct Matrix33Base
    {
        using ScalarType = T;
        static constexpr bool kRowMajorLayout = RowMajor;

        using VectorType = Vector3Base<T>;

        Matrix33Base();
        ~Matrix33Base() = default;

        Matrix33Base(const Matrix33Base& _other) = default;
        Matrix33Base(Matrix33Base&& _other) = default;

        Matrix33Base& operator=(const Matrix33Base& _other) = default;
        Matrix33Base& operator=(Matrix33Base&& _other) = default;

        Matrix33Base(T _a11, T _a12, T _a13, T _a21, T _a22, T _a23, T _a31, T _a32, T _a33);

        template<class U>
        requires eastl::is_convertible_v<U, T>
        Matrix33Base(U _a11, U _a12, U _a13, U _a21, U _a22, U _a23, U _a31, U _a32, U _a33)
            : Matrix33Base(_a11, _a12, _a13, _a21, _a22, _a23, _a31, _a32, _a33) {}

        /**
         * Constructs a 3x3 matrix from three 3D vectors.
         *
         * This constructor initializes the rows or columns of the matrix (depending on
         * the matrix layout, i.e., row-major or column-major) using the input vectors.
         *
         * @tparam U The type of the elements in the input vectors, which must be
         *         convertible to the type of the elements in the matrix (T).
         * @tparam VAlign The alignment requirement of the input vector type.
         * @param _v1 The first 3D vector to populate the first row/column of the matrix.
         * @param _v2 The second 3D vector to populate the second row/column of the matrix.
         * @param _v3 The third 3D vector to populate the third row/column of the matrix.
         *
         * @note Requires that the type `U` is implicitly convertible to the type `T` of
         * the matrix through a constraint using `eastl::is_convertible_v<U, T>`.
         */
        template<class U>
        requires eastl::is_convertible_v<U, T>
        Matrix33Base(
            const Vector3Base<U>& _v1,
            const Vector3Base<U>& _v2,
            const Vector3Base<U>& _v3)
                : m_vectors {
                    VectorType { _v1 },
                    VectorType { _v2 },
                    VectorType { _v3 },
                }
        {}

        template<class U>
        requires eastl::is_convertible_v<U, T>
        explicit Matrix33Base(const Matrix33Base<U, RowMajor>& _other)
            : m_vectors {
                VectorType(_other.m_vectors[0].x, _other.m_vectors[0].y, _other.m_vectors[0].z),
                VectorType(_other.m_vectors[1].x, _other.m_vectors[1].y, _other.m_vectors[1].z),
                VectorType(_other.m_vectors[2].x, _other.m_vectors[2].y, _other.m_vectors[2].z)
            }
        {}

        [[nodiscard]] bool IsRowMajor() const { return RowMajor; }
        [[nodiscard]] bool IsColumnMajor() const { return !RowMajor; }

        T& Get(size_t _row, size_t _col);
        const T& Get(size_t _row, size_t _col) const;

        Matrix33Base operator+(const Matrix33Base& _other) const;
        Matrix33Base operator-(const Matrix33Base& _other) const;
        Matrix33Base operator*(const Matrix33Base& _other) const;

        bool operator==(const Matrix33Base& _other) const
        {
            return m_vectors[0] == _other.m_vectors[0]
                   && m_vectors[1] == _other.m_vectors[1]
                   && m_vectors[2] == _other.m_vectors[2];
        }

        /**
         * Performs an in-place transposition of the 3x3 matrix.
         *
         * This function modifies the matrix by transposing its elements.
         * In a row-major matrix, rows are converted to columns and vice versa, while in a column-major matrix, columns
         * are converted to rows.
         *
         * @return A reference to the transposed matrix.
         */
        Matrix33Base& Transpose();
        [[nodiscard]] Matrix33Base Transposed() const;

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
        template <class U>
        requires eastl::is_convertible_v<U, T>
        static Matrix33Base Convert(const Matrix33Base<U, !RowMajor>& _other)
        {
            Matrix33Base<U, !RowMajor> transposed = _other;
            transposed.Transpose();
            return Matrix33Base(transposed.m_vectors[0], transposed.m_vectors[1], transposed.m_vectors[2]);
        }

        VectorType m_vectors[3];
    };

    template<class T>
    concept Matrix33Type = requires {
        typename T::ScalarType;
        T::kRowMajorLayout;
        std::is_same_v<Matrix33Base<typename T::ScalarType, T::kRowMajorLayout>, T>;
    };
}
