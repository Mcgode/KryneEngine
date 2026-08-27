
#define vkPushConstant [[vk::push_constant]]
#define vkLocation(index) [[vk::location(index)]]
#define vkBinding(index, set) [[vk::binding(index, set)]]

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

uint bitfieldExtract(const in uint _value, const in uint _size, const in uint _offset)
{
    const uint bitmask = (1u << _size) - 1u;
    return (_value >> _offset) & bitmask;
}
