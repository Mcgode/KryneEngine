/**
 * @file
 * @author Max Godefroy
 * @date 19/06/2025.
 */

#include "KryneEngine/Modules/SdfTexture/Generator.hpp"

#include <cmath>

namespace KryneEngine::Modules::SdfTexture
{
    Generator::Generator(AllocatorInstance _allocator)
        : m_allocator(_allocator)
    {}

    Generator::~Generator()
    {
        if (m_outputBuffer != nullptr)
        {
            m_allocator.deallocate(m_outputBuffer);
        }
    }

    void Generator::ForceDimensions(const uint3& _dimensions)
    {
        KE_ASSERT(_dimensions.x > kMinDimension && _dimensions.y > kMinDimension && _dimensions.z > kMinDimension);

        const float3 texelSizes = m_boundingBox.GetSize() / float3(_dimensions - 1);
        m_texelSize = std::fmax(texelSizes.x, std::fmax(texelSizes.y, texelSizes.z));
        m_dimensions = _dimensions;
    }

    void Generator::ComputeDimensionsFromBudget(u32 _texelBudget)
    {
        KE_ASSERT(m_boundingBox.IsValid());
        KE_ASSERT(_texelBudget >= kMinDimension * kMinDimension * kMinDimension);

        const float3 size = m_boundingBox.GetSize();
        KE_ASSERT(size.x > 0 && size.y > 0 && size.z > 0);

        u32 biggestComponent;
        if (size.x >= size.y)
        {
            biggestComponent = size.x >= size.z ? 0 : 2;
        }
        else
        {
            biggestComponent = size.y >= size.z ? 1 : 2;
        }
        const float3 relativeSize = size / size[biggestComponent];

        const float k = std::pow(float(_texelBudget) / (relativeSize.x * relativeSize.y * relativeSize.z), 1.f / 3.f);
        float referential = std::round(std::fmin(k, float(_texelBudget) / (kMinDimension * kMinDimension)));

        u32 totalUsedTexels = 0;

        while (referential > kMinDimension)
        {
            // The -1 and +1 are for setting up a half texel border to the texture.

            const float texelSize = size[biggestComponent] / (referential - 1);
            float3 sdfSize {};
            totalUsedTexels = static_cast<u32>(referential);
            m_dimensions[biggestComponent] = static_cast<u32>(referential);

            {
                const u32 index = (biggestComponent + 1) % 3;
                const float requiredTexelCount = std::fmax(std::ceil(size[index] / texelSize) + 1.f, float(kMinDimension));
                totalUsedTexels *= static_cast<u32>(requiredTexelCount);
                m_dimensions[index] = static_cast<u32>(requiredTexelCount);
            }

            {
                const u32 index = (biggestComponent + 2) % 3;
                const float requiredTexelCount = std::fmax(std::ceil(size[index] / texelSize) + 1.f, float(kMinDimension));
                totalUsedTexels *= static_cast<u32>(requiredTexelCount);
                m_dimensions[index] = static_cast<u32>(requiredTexelCount);
            }

            if (totalUsedTexels <= _texelBudget)
            {
                m_texelSize = texelSize;
                break;
            }

            referential -= 1.f;
        }
        KE_ASSERT(totalUsedTexels <= _texelBudget && referential > kMinDimension);
    }

    void Generator::Generate(
        eastl::span<const std::byte> _indexBuffer,
        eastl::span<const std::byte> _vertexBuffer,
        bool _16BitsIndex,
        u64 _vertexStride,
        u64 _vertexPositionOffset)
    {
        const size_t indexCount = _indexBuffer.size_bytes() / (_16BitsIndex ? sizeof(u16) : sizeof(u32));
        const size_t vertexCount = _vertexBuffer.size_bytes() / _vertexStride;

        const auto loadIndex = [&](const size_t _index)
        {
            return _16BitsIndex
                ? *(reinterpret_cast<const u16*>(_indexBuffer.data() + _index * sizeof(u16)))
                : *(reinterpret_cast<const u32*>(_indexBuffer.data() + _index * sizeof(u32)));
        };

        const auto loadPosition = [&](const size_t _index)
        {
            const size_t vertexOffset = _index * _vertexStride;
            const float3 position = *(reinterpret_cast<const float3*>(_vertexBuffer.data() + vertexOffset + _vertexPositionOffset));
            return float3 { position };
        };

        const float3 positionStart {
            m_boundingBox.GetCenter()
            - float3 { m_dimensions } * m_texelSize * 0.5f
        };

        m_outputBuffer = m_allocator.Allocate<Math::Float16>(m_dimensions.x * m_dimensions.y * m_dimensions.z);

        for (u32 z = 0; z < m_dimensions.z; z++)
        {
            for (u32 y = 0; y < m_dimensions.y; y++)
            {
                for (u32 x = 0; x < m_dimensions.x; x++)
                {
                    const float3 offset = (float3 { x, y, z } + 0.5f) * m_texelSize;
                    const float3 position = positionStart + offset;
                    float dist = FLT_MAX;

                    for (u32 i = 0; i < indexCount; i += 3)
                    {
                        const float3 a = loadPosition(loadIndex(i));
                        const float3 b = loadPosition(loadIndex(i + 1));
                        const float3 c = loadPosition(loadIndex(i + 2));

                        const float sdf = TriangleSdf(position, a, b, c);
                        if (std::fabs(sdf) < std::fabs(dist))
                        {
                            dist = sdf;
                        }
                    }

                    m_outputBuffer[x + y * m_dimensions.x + z * m_dimensions.x * m_dimensions.y] = Math::Float16 { dist };
                }
            }
        }
    }

    float
    Generator::TriangleSdf(const float3& _p, const float3& _a, const float3& _b, const float3& _c)
    {
        const float3 ba = _b - _a;
        const float3 cb = _c - _b;
        const float3 ac = _a - _c;

        const float3 pa = _p - _a;
        const float3 pb = _p - _b;
        const float3 pc = _p - _c;

        const float3 n = float3::CrossProduct(ba, ac);

        const s32 signs =
            std::signbit(float3::Dot(float3::CrossProduct(ba, n), pa))
            + std::signbit(float3::Dot(float3::CrossProduct(cb, n), pb))
            + std::signbit(float3::Dot(float3::CrossProduct(ac, n), pc));

        const float sign = float3::Dot(pa, n) >= 0 ? 1.f : -1.f;

        constexpr auto saturate = [](const float _value)
        {
            return std::fmin(std::fmax(_value, 0.f), 1.f);
        };

        if (signs < 2)
        {
            const float abEdgeSdf = (ba * saturate(float3::Dot(ba, pa) / ba.LengthSquared()) - pa).LengthSquared();
            const float bcEdgeSdf = (cb * saturate(float3::Dot(cb, pb) / cb.LengthSquared()) - pb).LengthSquared();
            const float caEdgeSdf = (ac * saturate(float3::Dot(ac, pc) / ac.LengthSquared()) - pc).LengthSquared();
            return sign * std::sqrt(std::fmin(std::fmin(abEdgeSdf, bcEdgeSdf), caEdgeSdf));
        }
        else
        {
            return sign * std::sqrt(float3::Dot(n, pa) * float3::Dot(n, pa) / n.LengthSquared());
        }
    }
}