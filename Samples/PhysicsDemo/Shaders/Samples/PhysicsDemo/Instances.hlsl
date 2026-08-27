/**
 * @file
 * @author Max Godefroy
 * @date 20/08/2026.
 */

#pragma once

struct InstanceData
{
    float3 m_position;
    uint m_quaternion0;
    uint m_quaternion1;
    float3 m_scale;
};


float4x4 UnpackTransform(const in InstanceData _instanceData)
{
    float4x4 mat = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    return mat;
}