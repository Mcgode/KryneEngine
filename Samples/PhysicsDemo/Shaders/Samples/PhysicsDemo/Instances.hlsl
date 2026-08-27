/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once


#include "Math/AffineTransform.hlsl"


struct InstanceData
{
    float3 m_translate;
    uint m_quaternion0;
    uint m_quaternion1;
    float3 m_scale;
};


void ApplyTransform(const in InstanceData _instanceData, inout float3 _position_, inout float3 _normal_)
{
    const float4 quaternion = Quaternion::Unpack64(uint2(_instanceData.m_quaternion0, _instanceData.m_quaternion1));
    _position_ = AffineTransform::ApplyToPosition(_instanceData.m_translate, quaternion, _instanceData.m_scale, _position_);
    _normal_ = AffineTransform::ApplyToNormal(quaternion, _instanceData.m_scale, _normal_);
}