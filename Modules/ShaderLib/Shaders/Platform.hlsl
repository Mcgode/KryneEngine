
#pragma once

#if !defined(__SLANG__)
float2 unpackHalf2x16ToFloat(in uint _packedHalf2)
{
    return f16tof32(uint2(_packedHalf2 & 0xffff, _packedHalf2 >> 16));
}

float4 unpackUnorm4x8ToFloat(uint _packedUnorm)
{
    const uint4 shifts = { 0, 8, 16, 24 };
    const uint4 rgba = (_packedUnorm >> shifts) & 0xff;
    return float4(rgba) / 255.0f;
}

#   define SV_VulkanVertexID SV_VertexID

#endif

#if defined(KE_TARGET_METALLIB)
#   define KE_PUSH_CONSTANT(i) register(b##i)
#else
#   define KE_PUSH_CONSTANT(i) register(b0, space##i)
#endif

// `float3` with a 12-byte size and 4-byte alignment in constant buffers on every target, so that a
// scalar following it packs into its 4th component the way D3D cbuffer packing (and our C++ structs) expect.
//
// Slang lays out Metal constant buffers with the native MSL rules, where `float3` is 16 bytes, and offers
// no layout override for them (https://github.com/shader-slang/slang/issues/13423). On Metal we replace it
// with a 3-float struct; elsewhere it is a plain `float3`.
//
// Usage rules:
//  - Constant buffer members only. StructuredBuffers are already packed on Metal, and arrays would get a
//    12-byte stride on Metal against 16 bytes in a D3D cbuffer.
//  - Assigning to a `float3` or passing it as a `float3` argument converts implicitly. Operators and
//    intrinsics (`a - b`, `normalize`, ...) do not: use `.xyz`, which is also valid on a real `float3`.
//  - D3D never lets a vector straddle a 16-byte boundary while this struct is only 4-byte aligned, so keep
//    it 16-byte aligned within its struct.
#if defined(KE_TARGET_METALLIB)
struct PackedFloat3
{
    float x, y, z;

    property float3 xyz { get { return float3(x, y, z); } }
};

extension float3
{
    __implicit_conversion(0) __init(PackedFloat3 _value) { this = float3(_value.x, _value.y, _value.z); }
}
#else
typedef float3 PackedFloat3;
#endif

// Screen-space derivatives. Use these instead of ddx/ddy: on SPIR-V, Slang's intrinsics raise the module
// to SPIR-V 1.4 (Vulkan 1.2) even though OpDPdx/OpDPdy are core SPIR-V 1.0, so we emit the instructions
// directly to keep shaders loadable on Vulkan 1.0.
#if defined(KE_TARGET_SPIRV)
float Ddx(float _value) { return spirv_asm { result:$$float = OpDPdx $_value; }; }
float Ddy(float _value) { return spirv_asm { result:$$float = OpDPdy $_value; }; }

__generic<let N : int>
vector<float, N> Ddx(vector<float, N> _value) { return spirv_asm { result:$$vector<float, N> = OpDPdx $_value; }; }
__generic<let N : int>
vector<float, N> Ddy(vector<float, N> _value) { return spirv_asm { result:$$vector<float, N> = OpDPdy $_value; }; }
#else
float Ddx(float _value) { return ddx(_value); }
float Ddy(float _value) { return ddy(_value); }

__generic<let N : int>
vector<float, N> Ddx(vector<float, N> _value) { return ddx(_value); }
__generic<let N : int>
vector<float, N> Ddy(vector<float, N> _value) { return ddy(_value); }
#endif

uint bitfieldExtract(const in uint _value, const in uint _size, const in uint _offset)
{
    const uint bitmask = (1u << _size) - 1u;
    return (_value >> _offset) & bitmask;
}
