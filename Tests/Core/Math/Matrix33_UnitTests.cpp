/**
 * @file
 * @author Max Godefroy
 * @date 21/03/2025.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Math/Matrix.hpp>

#include "Utils/AssertUtils.hpp"

namespace KryneEngine::Tests::Math
{
    using namespace Math;

    namespace
    {
        using float3x3_column_major = KryneEngine::Math::Matrix33Base<float, false>;
        using double3x3_column_major = KryneEngine::Math::Matrix33Base<double, false>;

        // Every value below is exact in floating point, so results are compared with EXPECT_EQ.

        template <KryneEngine::Math::Matrix33Type Matrix>
        void TestDeterminant()
        {
            // Determinant 1
            const Matrix unimodular {
                1.f, 2.f, 3.f,
                0.f, 1.f, 4.f,
                5.f, 6.f, 0.f
            };
            EXPECT_EQ(unimodular.Determinant(), 1);

            const Matrix diagonal {
                2.f, 0.f, 0.f,
                0.f, 3.f, 0.f,
                0.f, 0.f, 4.f
            };
            EXPECT_EQ(diagonal.Determinant(), 24);

            const Matrix mirror {
                -1.f, 0.f, 0.f,
                0.f, 1.f, 0.f,
                0.f, 0.f, 1.f
            };
            EXPECT_EQ(mirror.Determinant(), -1);

            // Rows are linearly dependent
            const Matrix singular {
                1.f, 2.f, 3.f,
                4.f, 5.f, 6.f,
                7.f, 8.f, 9.f
            };
            EXPECT_EQ(singular.Determinant(), 0);
        }

        template <KryneEngine::Math::Matrix33Type Matrix>
        void TestInverse()
        {
            const Matrix unimodular {
                1.f, 2.f, 3.f,
                0.f, 1.f, 4.f,
                5.f, 6.f, 0.f
            };
            const Matrix unimodularInverse {
                -24.f, 18.f, 5.f,
                20.f, -15.f, -4.f,
                -5.f, 4.f, 1.f
            };
            EXPECT_EQ(unimodular.Inverse(), unimodularInverse);
            EXPECT_EQ(unimodular * unimodular.Inverse(), Matrix());
            EXPECT_EQ(unimodular.Inverse() * unimodular, Matrix());

            const Matrix diagonal {
                2.f, 0.f, 0.f,
                0.f, 4.f, 0.f,
                0.f, 0.f, 8.f
            };
            const Matrix diagonalInverse {
                0.5f, 0.f, 0.f,
                0.f, 0.25f, 0.f,
                0.f, 0.f, 0.125f
            };
            EXPECT_EQ(diagonal.Inverse(), diagonalInverse);
        }

        template <KryneEngine::Math::Matrix33Type Matrix>
        void TestVectorMultiplication()
        {
            using Vector = typename Matrix::VectorType;

            const Matrix mat {
                1.f, 2.f, 3.f,
                4.f, 5.f, 6.f,
                7.f, 8.f, 9.f
            };
            const Vector vec { 1.f, 2.f, 3.f };

            // Each component is the dot product of a row with the vector. `vec * mat` would give (30, 36, 42).
            EXPECT_EQ(mat * vec, Vector(14.f, 32.f, 50.f));
            EXPECT_EQ(Matrix() * vec, vec);

            // 90 degrees around Z
            const Matrix rotation {
                0.f, -1.f, 0.f,
                1.f, 0.f, 0.f,
                0.f, 0.f, 1.f
            };
            EXPECT_EQ(rotation * Vector(1.f, 0.f, 0.f), Vector(0.f, 1.f, 0.f));
            EXPECT_EQ(rotation * Vector(0.f, 1.f, 0.f), Vector(-1.f, 0.f, 0.f));
        }
    }

    TEST(Matrix33, Addition)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const float3x3 matA {
            1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f
        };

        const float3x3 matB {
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f
        };

        const float3x3 expectedResult {
            2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f,
            8.0f, 9.0f, 10.0f
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const float3x3 a(matA);
            const float3x3 b(matB);
            const float3x3 result = a + b;
            EXPECT_EQ(result, float3x3(expectedResult));
        }

        {
            const double3x3 a(matA);
            const double3x3 b(matB);
            const double3x3 result = a + b;
            EXPECT_EQ(result, double3x3(expectedResult));
        }
    }

    TEST(Matrix33, Substraction)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const float3x3 matA {
            1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f
        };

        const float3x3 matB {
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f
        };

        const float3x3 expectedResult {
            0.0f, 1.0f, 2.0f,
            3.0f, 4.0f, 5.0f,
            6.0f, 7.0f, 8.0f
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const float3x3 a(matA);
            const float3x3 b(matB);
            const float3x3 result = a - b;
            EXPECT_EQ(result, float3x3(expectedResult));
        }

        {
            const double3x3 a(matA);
            const double3x3 b(matB);
            const double3x3 result = a - b;
            EXPECT_EQ(result, double3x3(expectedResult));
        }
    }

    TEST(Matrix33, Multiplication)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const float3x3 matA {
            1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f
        };

        const float3x3 matB {
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f
        };

        const float3x3 expectedResultAb {
            6.0f, 6.0f, 6.0f,
            15.0f, 15.0f, 15.0f,
            24.0f, 24.0f, 24.0f
        };

        const float3x3 expectedResultBa {
            12.0f, 15.0f, 18.0f,
            12.0f, 15.0f, 18.0f,
            12.0f, 15.0f, 18.0f
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const float3x3 a(matA);
            const float3x3 b(matB);
            const float3x3 resultAb = a * b;
            const float3x3 resultBa = b * a;
            EXPECT_EQ(resultAb, float3x3(expectedResultAb));
            EXPECT_EQ(resultBa, float3x3(expectedResultBa));
        }

        {
            const double3x3 a(matA);
            const double3x3 b(matB);
            const double3x3 resultAb = a * b;
            const double3x3 resultBa = b * a;
            EXPECT_EQ(resultAb, double3x3(expectedResultAb));
            EXPECT_EQ(resultBa, double3x3(expectedResultBa));
        }
    }

    TEST(Matrix33, Transpose)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        const float3x3 matBase {
            1.f, 2.f, 3.f,
            4.f, 5.f, 6.f,
            7.f, 8.f, 9.f
        };

        const float3x3 expected {
            1.f, 4.f, 7.f,
            2.f, 5.f, 8.f,
            3.f, 6.f, 9.f
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        {
            const float3x3 mat(matBase);
            EXPECT_EQ(mat.Transposed(), float3x3(expected));
        }

        {
            const double3x3 mat(matBase);
            EXPECT_EQ(mat.Transposed(), double3x3(expected));
        }
    }

    TEST(Matrix33, Determinant)
    {
        TestDeterminant<float3x3>();
        TestDeterminant<double3x3>();
        TestDeterminant<float3x3_column_major>();
        TestDeterminant<double3x3_column_major>();
    }

    TEST(Matrix33, Inverse)
    {
        TestInverse<float3x3>();
        TestInverse<double3x3>();
        TestInverse<float3x3_column_major>();
        TestInverse<double3x3_column_major>();
    }

    TEST(Matrix33, InverseOfSingularAsserts)
    {
        // -----------------------------------------------------------------------
        // Setup
        // -----------------------------------------------------------------------

        ScopedAssertCatcher catcher;

        const float3x3 singular {
            1.f, 2.f, 3.f,
            4.f, 5.f, 6.f,
            7.f, 8.f, 9.f
        };

        // -----------------------------------------------------------------------
        // Execute
        // -----------------------------------------------------------------------

        (void)singular.Inverse();
        catcher.ExpectMessageCount(1);
    }

    TEST(Matrix33, VectorMultiplication)
    {
        TestVectorMultiplication<float3x3>();
        TestVectorMultiplication<double3x3>();
        TestVectorMultiplication<float3x3_column_major>();
        TestVectorMultiplication<double3x3_column_major>();
    }
}