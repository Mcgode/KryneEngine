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

    // The three lowest components are packed as 20-bit two's complement signed integers; shifting
    // the field up against the top of a 32-bit word and back down with an arithmetic shift sign-
    // extends it.
    int SignExtendBitfield(const in uint _value, const in uint _bits)
    {
        const uint shift = 32u - _bits;
        return asint(_value << shift) >> shift;
    }

    float4 Unpack64(const in uint2 _packed)
    {
        const uint highestIndex = bitfieldExtract(_packed.x, 2, 0);
        const uint3 packedValues = uint3(
            bitfieldExtract(_packed.x, 20, 2),
            bitfieldExtract(_packed.x, 10, 22) | (bitfieldExtract(_packed.y, 10, 0) << 10),
            bitfieldExtract(_packed.y, 20, 10)
        );
        const int3 signedValues = int3(
            SignExtendBitfield(packedValues.x, 20),
            SignExtendBitfield(packedValues.y, 20),
            SignExtendBitfield(packedValues.z, 20));

        // Matches the encoding scale used by QuaternionBase<T>::Pack64() on the CPU:
        // component = rawValue / (sqrt(2) * ((1 << 19) - 1))
        const float scale = (sqrt(2) * 0.5f) / (float(1 << 19) - 1.f);
        const float3 lowestComponents = float3(signedValues) * scale;
        const float highestComponent = sqrt(saturate(1 - dot(lowestComponents, lowestComponents)));

        float4 quaternion;
        quaternion[highestIndex] = highestComponent;
        uint offset = 0;
        for (uint i = 0; i < 3; i++)
        {
            if (i == highestIndex)
                offset++;
            quaternion[i + offset] = lowestComponents[i];
        }

        // Quaternion are stored in wxyz order on the CPU and packed based on this order.
        // The GPU meanwhile uses xyzw order.
        return quaternion.yzwx;
    }
}