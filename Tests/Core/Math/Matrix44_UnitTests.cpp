/**
 * @file
 * @author Max Godefroy
 * @date 21/03/2025.
 */

#include <gtest/gtest.h>
#include <KryneEngine/Core/Math/Matrix.hpp>
#include <KryneEngine/Core/Math/Projection.hpp>
#include <KryneEngine/Core/Math/Transform.hpp>

namespace KryneEngine::Tests::Math
{
    using namespace KryneEngine::Math;

    using float4x4_column_major = Matrix44Base<float, false, false>;
    using float4x4_simd_column_major = Matrix44Base<float, false, true>;
    using double4x4_column_major = Matrix44Base<double, false, false>;
    using double4x4_simd_column_major = Matrix44Base<double, false, true>;

    namespace Conversion
    {
        // Both matrices are defined the same way, only data storage differs.

        const float4x4 matRowMajor {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };

        const float4x4_column_major matColumnMajor {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };

        TEST(Matrix44, Conversion_float4x4)
        {
            const float4x4 mr(matRowMajor);
            const float4x4_column_major mc(matColumnMajor);

            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    EXPECT_EQ(mc.Get(i, j), mr.Get(i, j));
                }
            }

            EXPECT_EQ(mc, float4x4_column_major::Convert(mr));
            EXPECT_EQ(mr, float4x4::Convert(mc));
        }

        TEST(Matrix44, Conversion_float4x4_simd)
        {
            const float4x4_simd mr(matRowMajor);
            const float4x4_simd_column_major mc(matColumnMajor);

            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    EXPECT_EQ(mc.Get(i, j), mr.Get(i, j));
                }
            }

            EXPECT_EQ(mc, float4x4_simd_column_major::Convert(mr));
            EXPECT_EQ(mr, float4x4_simd::Convert(mc));
        }

        TEST(Matrix44, Conversion_double4x4)
        {
            const double4x4 mr(matRowMajor);
            const double4x4_column_major mc(matColumnMajor);

            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    EXPECT_EQ(mc.Get(i, j), mr.Get(i, j));
                }
            }

            EXPECT_EQ(mc, double4x4_column_major::Convert(mr));
            EXPECT_EQ(mr, double4x4::Convert(mc));
        }

        TEST(Matrix44, Conversion_double4x4_simd)
        {
            const double4x4_simd mr(matRowMajor);
            const double4x4_simd_column_major mc(matColumnMajor);

            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    EXPECT_EQ(mc.Get(i, j), mr.Get(i, j));
                }
            }

            EXPECT_EQ(mc, double4x4_simd_column_major::Convert(mr));
            EXPECT_EQ(mr, double4x4_simd::Convert(mc));
        }
    }
    
    namespace Addition
    {
        const float4x4 matA {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };

        const float4x4 matB {
            0.f, -1.f, 2.f, -3.f,
            4.f, -5.f, 6.f, -7.f,
            8.f, -9.f, 10.f, -11.f,
            12.f, -13.f, 14.f, -15.f
        };

        const float4x4 expectedResult {
            1.0f, 1.0f, 5.0f, 1.0f,
            9.0f, 1.0f, 13.0f, 1.0f,
            17.0f, 1.0f, 21.0f, 1.0f,
            25.0f, 1.0f, 29.0f, 1.0f
        };

        TEST(Matrix44, Addition_float4x4)
        {
            const float4x4 a(matA);
            const float4x4 b(matB);
            const float4x4 result = a + b;
            EXPECT_EQ(result, expectedResult);
        }

        TEST(Matrix44, Addition_float4x4_simd)
        {
            const float4x4_simd a(matA);
            const float4x4_simd b(matB);
            const float4x4_simd result = a + b;
            EXPECT_EQ(result, float4x4_simd(expectedResult));
        }

        TEST(Matrix44, Addition_double4x4)
        {
            const double4x4 a(matA);
            const double4x4 b(matB);
            const double4x4 result = a + b;
            EXPECT_EQ(result, double4x4(expectedResult));
        }

        TEST(Matrix44, Addition_double4x4_simd)
        {
            const double4x4_simd a(matA);
            const double4x4_simd b(matB);
            const double4x4_simd result = a + b;
            EXPECT_EQ(result, double4x4_simd(expectedResult));
        }

        TEST(Matrix44, Addition_float4x4_column_major)
        {
            const float4x4_column_major a = float4x4_column_major::Convert(matA);
            const float4x4_column_major b = float4x4_column_major::Convert(matB);
            const float4x4_column_major result = a + b;
            EXPECT_EQ(result, float4x4_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Addition_float4x4_simd_column_major)
        {
            const float4x4_simd_column_major a = float4x4_simd_column_major::Convert(matA);
            const float4x4_simd_column_major b = float4x4_simd_column_major::Convert(matB);
            const float4x4_simd_column_major result = a + b;
            EXPECT_EQ(result, float4x4_simd_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Addition_double4x4_column_major)
        {
            const double4x4_column_major a = double4x4_column_major::Convert(matA);
            const double4x4_column_major b = double4x4_column_major::Convert(matB);
            const double4x4_column_major result = a + b;
            EXPECT_EQ(result, double4x4_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Addition_double4x4_simd_column_major)
        {
            const double4x4_simd_column_major a = double4x4_simd_column_major::Convert(matA);
            const double4x4_simd_column_major b = double4x4_simd_column_major::Convert(matB);
            const double4x4_simd_column_major result = a + b;
            EXPECT_EQ(result, double4x4_simd_column_major::Convert(expectedResult));
        }
    }

    namespace Subtraction
    {
        const float4x4 matA {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };

        const float4x4 matB {
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        };

        const float4x4 expectedResult {
            0.0f, 1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f, 7.0f,
            8.0f, 9.0f, 10.0f, 11.0f,
            12.0f, 13.0f, 14.0f, 15.0f
        };

        TEST(Matrix44, Substraction_float4x4)
        {
            const float4x4 a(matA);
            const float4x4 b(matB);
            const float4x4 result = a - b;
            EXPECT_EQ(result, expectedResult);
        }

        TEST(Matrix44, Substraction_float4x4_simd)
        {
            const float4x4_simd a(matA);
            const float4x4_simd b(matB);
            const float4x4_simd result = a - b;
            EXPECT_EQ(result, float4x4_simd(expectedResult));
        }

        TEST(Matrix44, Substraction_double4x4)
        {
            const double4x4 a(matA);
            const double4x4 b(matB);
            const double4x4 result = a - b;
            EXPECT_EQ(result, double4x4(expectedResult));
        }

        TEST(Matrix44, Substraction_double4x4_simd)
        {
            const double4x4_simd a(matA);
            const double4x4_simd b(matB);
            const double4x4_simd result = a - b;
            EXPECT_EQ(result, double4x4_simd(expectedResult));
        }

        TEST(Matrix44, Substraction_float4x4_column_major)
        {
            const float4x4_column_major a = float4x4_column_major::Convert(matA);
            const float4x4_column_major b = float4x4_column_major::Convert(matB);
            const float4x4_column_major result = a - b;
            EXPECT_EQ(result, float4x4_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Substraction_float4x4_simd_column_major)
        {
            const float4x4_simd_column_major a = float4x4_simd_column_major::Convert(matA);
            const float4x4_simd_column_major b = float4x4_simd_column_major::Convert(matB);
            const float4x4_simd_column_major result = a - b;
            EXPECT_EQ(result, float4x4_simd_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Substraction_double4x4_column_major)
        {
            const double4x4_column_major a = double4x4_column_major::Convert(matA);
            const double4x4_column_major b = double4x4_column_major::Convert(matB);
            const double4x4_column_major result = a - b;
            EXPECT_EQ(result, double4x4_column_major::Convert(expectedResult));
        }

        TEST(Matrix44, Substraction_double4x4_simd_column_major)
        {
            const double4x4_simd_column_major a = double4x4_simd_column_major::Convert(matA);
            const double4x4_simd_column_major b = double4x4_simd_column_major::Convert(matB);
            const double4x4_simd_column_major result = a - b;
            EXPECT_EQ(result, double4x4_simd_column_major::Convert(expectedResult));
        }
    }

    namespace Multiplication
    {
        const float4x4 matA {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };

        const float4x4 matB {
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        };

        const float4x4 expectedResultAb {
            10.f, 10.f, 10.f, 10.f,
            26.f, 26.f, 26.f, 26.f,
            42.f, 42.f, 42.f, 42.f,
            58.f, 58.f, 58.f, 58.f
        };

        const float4x4 expectedResultBa {
            28.f, 32.f, 36.f, 40.f,
            28.f, 32.f, 36.f, 40.f,
            28.f, 32.f, 36.f, 40.f,
            28.f, 32.f, 36.f, 40.f
        };

        TEST(Matrix44, Multiplication_float4x4)
        {
            const float4x4 resultAb = matA * matB;
            const float4x4 resultBa = matB * matA;
            EXPECT_EQ(resultAb, expectedResultAb);
            EXPECT_EQ(resultBa, expectedResultBa);
        }

        TEST(Matrix44, Multiplication_float4x4_simd)
        {
            const float4x4_simd a(matA);
            const float4x4_simd b(matB);
            const float4x4_simd resultAb = a * b;
            const float4x4_simd resultBa = b * a;
            EXPECT_EQ(resultAb, float4x4_simd(expectedResultAb));
            EXPECT_EQ(resultBa, float4x4_simd(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_double4x4)
        {
            const double4x4 a(matA);
            const double4x4 b(matB);
            const double4x4 resultAb = a * b;
            const double4x4 resultBa = b * a;
            EXPECT_EQ(resultAb, double4x4(expectedResultAb));
            EXPECT_EQ(resultBa, double4x4(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_double4x4_simd)
        {
            const double4x4_simd a(matA);
            const double4x4_simd b(matB);
            const double4x4_simd resultAb = a * b;
            const double4x4_simd resultBa = b * a;
            EXPECT_EQ(resultAb, double4x4_simd(expectedResultAb));
            EXPECT_EQ(resultBa, double4x4_simd(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_float4x4_column_major)
        {
            const float4x4_column_major a = float4x4_column_major::Convert(matA);
            const float4x4_column_major b = float4x4_column_major::Convert(matB);
            const float4x4_column_major resultAb = a * b;
            const float4x4_column_major resultBa = b * a;
            EXPECT_EQ(resultAb, float4x4_column_major::Convert(expectedResultAb));
            EXPECT_EQ(resultBa, float4x4_column_major::Convert(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_float4x4_simd_column_major)
        {
            const float4x4_simd_column_major a = float4x4_simd_column_major::Convert(matA);
            const float4x4_simd_column_major b = float4x4_simd_column_major::Convert(matB);
            const float4x4_simd_column_major resultAb = a * b;
            const float4x4_simd_column_major resultBa = b * a;
            EXPECT_EQ(resultAb, float4x4_simd_column_major::Convert(expectedResultAb));
            EXPECT_EQ(resultBa, float4x4_simd_column_major::Convert(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_double4x4_column_major)
        {
            const double4x4_column_major a = double4x4_column_major::Convert(matA);
            const double4x4_column_major b = double4x4_column_major::Convert(matB);
            const double4x4_column_major resultAb = a * b;
            const double4x4_column_major resultBa = b * a;
            EXPECT_EQ(resultAb, double4x4_column_major::Convert(expectedResultAb));
            EXPECT_EQ(resultBa, double4x4_column_major::Convert(expectedResultBa));
        }

        TEST(Matrix44, Multiplication_double4x4_simd_column_major)
        {
            const double4x4_simd_column_major a = double4x4_simd_column_major::Convert(matA);
            const double4x4_simd_column_major b = double4x4_simd_column_major::Convert(matB);
            const double4x4_simd_column_major resultAb = a * b;
            const double4x4_simd_column_major resultBa = b * a;
            EXPECT_EQ(resultAb, double4x4_simd_column_major::Convert(expectedResultAb));
            EXPECT_EQ(resultBa, double4x4_simd_column_major::Convert(expectedResultBa));
        }
    }
    
    namespace MultiplyVector
    {
        constexpr float4 testVector { 1.0f, 2.0f, 3.0f, 4.0f };
        
        const float4x4 identityMat {};
        constexpr float4 identityVector { 1.0f, 2.0f, 3.0f, 4.0f };

        const auto translationMat = ComputeTransformMatrix<float4x4>(
            float3(1.0f, 2.0f, 3.0f),
            Quaternion(),
            float3(1.0f)
        );
        constexpr float4 translationVec { 5.0f, 10.0f, 15.0f, 4.f };

        const auto scaleMat = ComputeTransformMatrix<float4x4>(
            float3(),
            Quaternion(),
            float3(1.0f, 0.5f, 1.2f)
        );
        constexpr float4 scaleVector { 1.f, 1.f, 3.6f, 4.f };

        const auto rotationMat = ComputeTransformMatrix<float4x4>(
            float3(),
            Quaternion().FromAxisAngle(float3(1.0f, 1.0f, 0.0f).Normalized(), 0.5),
            float3(1.0f)
        );
        constexpr float4 rotationVector { 2.0782238766551018, 0.92177611216902733, 2.9717527031898499, 4 };

        const auto transformMat = ComputeTransformMatrix<float4x4>(
            float3(1.0f, 2.0f, 3.0f),
            Quaternion().FromAxisAngle(float3(1.0f, 1.0f, 0.0f).Normalized(), 0.5f),
            float3(1.0f, 0.5f, 1.2f)
        );
        constexpr float4 transformVector { 6.2204182408750057, 7.7795817516744137, 15.159297466278076, 4 };

        const auto perspectiveMat = PerspectiveProjection<float4x4>(1.5f, 1.3333f, 0.1, 1024, false);
        constexpr float4 perspectiveVector { 0.80508971214294434, 3.2202783823013306, 1.6001561880111694, 2 };
        
        template<Matrix44Type T>
        T Copy(const float4x4& _mat)
        {
            if constexpr (T::kRowMajorLayout)
                return T(_mat);
            else
                return T::Convert(_mat);
        }

        template<Matrix44Type T>
        static void Test()
        {
            using Vector = T::VectorType;
            {
                const T identity = Copy<T>(identityMat);
                const Vector test = Vector(testVector);
                
                const Vector vec = identity * test;
                EXPECT_EQ(vec, Vector(identityVector)) << "Identity matrix apply is invalid";
            }

            {
                const T translation = Copy<T>(translationMat);
                const Vector test = Vector(testVector);
                
                const Vector vec = translation * test;
                EXPECT_EQ(vec, Vector(translationVec)) << "Translation matrix apply is invalid";
            }

            {
                const T scale = Copy<T>(scaleMat);
                const Vector test = Vector(testVector);

                const Vector vec = scale * test;
                EXPECT_EQ(vec, Vector(scaleVector)) << "Scale matrix apply is invalid";
            }

            {
                const T rotation = Copy<T>(rotationMat);
                const Vector test = Vector(testVector);

                const Vector vec = rotation * test;
                EXPECT_EQ(vec, Vector(rotationVector)) << "Rotation matrix apply is invalid";
            }

            {
                const T transform = Copy<T>(transformMat);
                const Vector test = Vector(testVector);

                const Vector vec = transform * test;
                EXPECT_EQ(vec, Vector(transformVector)) << "Transform matrix apply is invalid";
            }

            {
                const T perspective = Copy<T>(perspectiveMat);
                const Vector test = Vector(testVector);

                const Vector vec = perspective * test;
                EXPECT_EQ(vec, Vector(perspectiveVector)) << "Perspective matrix apply is invalid";
            }
        }
        
        TEST(Matrix44, MultiplyVector_float4x4)
        {
            MultiplyVector::Test<float4x4>();
        }

        TEST(Matrix44, MultiplyVector_float4x4_simd)
        {
            MultiplyVector::Test<float4x4_simd>();
        }

        TEST(Matrix44, MultiplyVector_double4x4)
        {
            MultiplyVector::Test<double4x4>();
        }

        TEST(Matrix44, MultiplyVector_double4x4_simd)
        {
            MultiplyVector::Test<double4x4_simd>();
        }

        TEST(Matrix44, MultiplyVector_float4x4_column_major)
        {
            MultiplyVector::Test<float4x4_column_major>();
        }

        TEST(Matrix44, MultiplyVector_float4x4_simd_column_major)
        {
            MultiplyVector::Test<float4x4_simd_column_major>();
        }

        TEST(Matrix44, MultiplyVector_double4x4_column_major)
        {
            MultiplyVector::Test<double4x4_column_major>();
        }

        TEST(Matrix44, MultiplyVector_double4x4_simd_column_major)
        {
            MultiplyVector::Test<double4x4_simd_column_major>();
        }
    }
    
    namespace Transpose
    {
        const float4x4 matBase {
            1.f, 2.f, 3.f, 4.f,
            5.f, 6.f, 7.f, 8.f,
            9.f, 10.f, 11.f, 12.f,
            13.f, 14.f, 15.f, 16.f
        };

        const float4x4 expected {
            1.f, 5.f, 9.f, 13.f,
            2.f, 6.f, 10.f, 14.f,
            3.f, 7.f, 11.f, 15.f,
            4.f, 8.f, 12.f, 16.f
        };

        TEST(Matrix44, Transpose_float4x4)
        {
            const float4x4 m(matBase);
            const float4x4 result = m.Transposed();
            EXPECT_EQ(result, float4x4(expected));
        }

        TEST(Matrix44, Transpose_float4x4_simd)
        {
            const float4x4_simd m(matBase);
            const float4x4_simd result = m.Transposed();
            EXPECT_EQ(result, float4x4_simd(expected));
        }

        TEST(Matrix44, Transpose_double4x4)
        {
            const double4x4 m(matBase);
            const double4x4 result = m.Transposed();
            EXPECT_EQ(result, double4x4(expected));
        }

        TEST(Matrix44, Transpose_double4x4_simd)
        {
            const double4x4_simd m(matBase);
            const double4x4_simd result = m.Transposed();
            EXPECT_EQ(result, double4x4_simd(expected));
        }

        TEST(Matrix44, Transpose_float4x4_column_major)
        {
            const float4x4_column_major m = float4x4_column_major::Convert(matBase);
            const float4x4_column_major result = m.Transposed();
            EXPECT_EQ(result, float4x4_column_major::Convert(expected));
        }

        TEST(Matrix44, Transpose_float4x4_simd_column_major)
        {
            const float4x4_simd_column_major m = float4x4_simd_column_major::Convert(matBase);
            const float4x4_simd_column_major result = m.Transposed();
            EXPECT_EQ(result, float4x4_simd_column_major::Convert(expected));
        }

        TEST(Matrix44, Transpose_double4x4_column_major)
        {
            const double4x4_column_major m = double4x4_column_major::Convert(matBase);
            const double4x4_column_major result = m.Transposed();
            EXPECT_EQ(result, double4x4_column_major::Convert(expected));
        }

        TEST(Matrix44, Transpose_double4x4_simd_column_major)
        {
            const double4x4_simd_column_major m = double4x4_simd_column_major::Convert(matBase);
            const double4x4_simd_column_major result = m.Transposed();
            EXPECT_EQ(result, double4x4_simd_column_major::Convert(expected));
        }
    }

    namespace Inverse
    {
        const float4x4 identityMat {};

        const auto translationMat = ComputeTransformMatrix<float4x4>(
            float3(1.0f, 2.0f, 3.0f),
            Quaternion(),
            float3(1.0f)
        );

        const auto scaleMat = ComputeTransformMatrix<float4x4>(
            float3(),
            Quaternion(),
            float3(1.0f, 0.5f, 1.2f)
        );

        const auto rotationMat = ComputeTransformMatrix<float4x4>(
            float3(),
            Quaternion(),
            float3(1.0f)
        );

        const auto transformMat = ComputeTransformMatrix<float4x4>(
            float3(1.0f, 2.0f, 3.0f),
            Quaternion().FromAxisAngle(float3(1.0f, 1.0f, 0.0f).Normalized(), 0.5f),
            float3(1.0f, 0.5f, 1.2f)
        );

        const auto perspectiveMat = PerspectiveProjection<float4x4>(1.5f, 1.3333f, 0.1, 1024, false);

        const float4 testVector { 1.0f, 2.0f, 3.0f, 4.0f };

        template<Matrix44Type T>
        T Copy(const float4x4& _mat)
        {
            if constexpr (T::kRowMajorLayout)
                return T(_mat);
            else
                return T::Convert(_mat);
        }

        template<Matrix44Type T>
        static void Test()
        {
            using Vector = T::VectorType;
            {
                const T identity = Copy<T>(identityMat);
                const Vector test = Vector(testVector);

                const Vector vec = identity * test;
                const T inverse = identity.Inverse();
                EXPECT_EQ(identity, inverse) << "Identity matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Identity matrix inverse is invalid";
            }

            {
                const T translation = Copy<T>(translationMat);
                const Vector test = Vector(testVector);

                const Vector vec = translation * test;
                const T inverse = translation.Inverse();
                const T mul = translation * inverse;
                EXPECT_EQ(mul, T()) << "Translation matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Translation matrix inverse is invalid";
            }

            {
                const T scale = Copy<T>(scaleMat);
                const Vector test = Vector(testVector);

                const Vector vec = scale * test;
                const T inverse = scale.Inverse();
                const T mul = scale * inverse;
                EXPECT_EQ(mul, T()) << "Scale matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Scale matrix inverse is invalid";
            }

            {
                const T rotation = Copy<T>(rotationMat);
                const Vector test = Vector(testVector);

                const Vector vec = rotation * test;
                const T inverse = rotation.Inverse();
                const T mul = rotation * inverse;
                EXPECT_EQ(mul, T()) << "Rotation matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Rotation matrix inverse is invalid";
            }

            {
                const T transform = Copy<T>(transformMat);
                const Vector test = Vector(testVector);

                const Vector vec = transform * test;
                const T inverse = transform.Inverse();
                const T mul = transform * inverse;
                EXPECT_EQ(mul, T()) << "Transform matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Transform matrix inverse is invalid";
            }

            {
                const T perspective = Copy<T>(perspectiveMat);
                const Vector test = Vector(testVector);

                const Vector vec = perspective * test;
                const T inverse = perspective.Inverse();
                const T mul = perspective * inverse;
                EXPECT_EQ(mul, T()) << "Perspective matrix inverse is invalid";
                EXPECT_EQ(test, inverse * vec) << "Perspective matrix inverse is invalid";
            }
        }

        TEST(Matrix44, Inverse_float4x4)
        {
            Inverse::Test<float4x4>();
        }

        TEST(Matrix44, Inverse_float4x4_simd)
        {
            Inverse::Test<float4x4_simd>();
        }

        TEST(Matrix44, Inverse_double4x4)
        {
            Inverse::Test<double4x4>();
        }

        TEST(Matrix44, Inverse_double4x4_simd)
        {
            Inverse::Test<double4x4_simd>();
        }

        TEST(Matrix44, Inverse_float4x4_column_major)
        {
            Inverse::Test<float4x4_column_major>();
        }

        TEST(Matrix44, Inverse_float4x4_simd_column_major)
        {
            Inverse::Test<float4x4_simd_column_major>();
        }

        TEST(Matrix44, Inverse_double4x4_column_major)
        {
            Inverse::Test<double4x4_column_major>();
        }

        TEST(Matrix44, Inverse_double4x4_simd_column_major)
        {
            Inverse::Test<double4x4_simd_column_major>();
        }
    }
}