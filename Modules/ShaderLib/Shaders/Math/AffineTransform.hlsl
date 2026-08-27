/**
 * @file
 * @author Max Godefroy
 * @date 27/08/2026.
 */

#pragma once

#include "Math/Quaternion.hlsl"


namespace AffineTransform
{
    float3 ApplyToPosition(
        const in float3 _translation,
        const in float4 _rotation,
        const in float3 _scale,
        const in float3 _position)
    {
        return _translation + Quaternion::Apply(_rotation, _scale * _position);
    }

    /**
     * Based on https://github.com/graphitemaster/normals_revisited, applied to having direct quat + scale vector
     *
     * Just in case here is an mathematics explanation:
     * - We have our transform matrix `M = RS`(we can ignore translate here, as we work with normals) where R is a
     *     rotation matrix, and S a scale matrix, where `S = diag(s.x, s.y, s.z)`
     * - The general formula for a normal matrix is `N = transpose(inverse(M))`.
     * - If we use the adjugate, we have `inverse(M) = adj(M) / det(M)` so `N = transpose(inverse(M)) =
     *     transpose(adj(M)) / det(M)`.
     * - The transpose of the adjugate is called the cofactor, so `cof(M) = transpose(inverse(M)) * det(M)`.
     * - We can deduce that `cof(AB) = transpose(inverse(AB)) * det(AB) = transpose(inverse(A)) * transpose(inverse(B)) *
     *     det(A) * det(B) = cof(A) * cof(B)`.
     * - As such, we get `N = cof(RS) / det(RS) = cof(R) * cof(S) / (det(R) * det(S))`
     * - The determinant of a rotation matrix is 1, and its transpose inverse is itself, so `cof(R) =
     *     transpose(inverse(R)) * det(R) = R`
     * - S is a diagonal matrix, so `cof(S) = cof(diag(s.x, s.y, s.z)) = diag(s.y*s.z, s.x*s.z, s.x*s.y)`
     * - We end up with `N = R * diag(s.y*s.z, s.x*s.z, s.x*s.y) / det(S)`
     */
    float3 ApplyToNormal(
        const in float4 _rotation,
        const in float3 _scale,
        const in float3 _normal)
    {
        const float3 cofactorScale = {
            _scale.y * _scale.z,
            _scale.x * _scale.z,
            _scale.x * _scale.y,
        };

        const float detS = _scale.x * _scale.y * _scale.z;
        const float signDetS = detS == 0 ? 1.f : sign(detS);

        // To avoid any divide by zero, we normalize instead, and retrieve the normal from the determinant
        return normalize(Quaternion::Apply(_rotation, cofactorScale * _normal)) * signDetS;
    }
}