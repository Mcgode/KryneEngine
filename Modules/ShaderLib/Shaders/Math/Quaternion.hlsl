/**
 * @file
 * @author Max Godefroy
 * @date 03/04/2025.
 */

namespace Quaternion
{
    float3 Apply(const in float4 _quaternion, const in float3 _vector)
    {
        // Based on https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/

        // const float3 t = 2 * cross(_quaternion.xyz, _vector);
        // return _vector + _quaternion.w * t + cross(_quaternion.xyz, t);

        return 2.f * _quaternion.xyz * dot(_vector, _quaternion.xyz)
            + _vector * (_quaternion.w * _quaternion.w - dot(_quaternion.xyz, _quaternion.xyz))
            + cross(_quaternion.xyz, _vector) * 2.f * _quaternion.w;
    }

    float4 Conjugate(const in float4 _quaternion)
    {
        return float4(-_quaternion.xyz, _quaternion.w);
    }

    float4 Unpack64(const in uint2 _packed)
    {
        const uint highestIndex = bitfieldExtract(_packed.x, 2, 0);
        const uint3 packedValues = uint3(
            bitfieldExtract(_packed.x, 20, 2),
            bitfieldExtract(_packed.x, 10, 22) | (bitfieldExtract(_packed.y, 10, 0) << 10),
            bitfieldExtract(_packed.y, 20, 10)
        );

        const float scale = sqrt(2) / (float(1 << 19) -1.f);
        const float3 lowestComponents = float3(packedValues) * scale;
        const float highestComponent = sqrt(1 - dot(lowestComponents, lowestComponents));

        float4 quaternion;
        quaternion[highestIndex] = highestComponent;
        uint offset = 0;
        for (uint i = 0; i < 3; i++)
        {
            if (i == highestIndex)
                offset++;
            quaternion[i + offset] = lowestComponents[i];
        }

        return quaternion;
    }
}