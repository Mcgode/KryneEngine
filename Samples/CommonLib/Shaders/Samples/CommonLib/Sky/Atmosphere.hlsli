/**
 * @file
 * @author Max Godefroy
 * @date 18/09/2026.
 */

#pragma once

#include "Math/Constants.hlsl"

static const float kAtmospherePlanetRadius = 6.360e6f;
static const float kAtmosphereRadius       = 6.420e6f;

static const float3 kBetaR = float3(3.8e-6f, 13.5e-6f, 33.1e-6f);
static const float3 kBetaM = float3(21.0e-6f.xxx);

static const float kHR = 7994.f;
static const float kHM = 1200.f;

static const uint kAtmoSampleSteps      = 16;
static const uint kAtmoLightSampleSteps = 8;

struct AtmoRay    { float3 origin; float3 direction; };
struct AtmoSphere { float3 origin; float radius; };

bool AtmoRaySphereIntersect(
    const in AtmoRay    _ray,
    const in AtmoSphere _sphere,
    inout float         t0_,
    inout float         t1_)
{
    const float3 rc      = _sphere.origin - _ray.origin;
    const float  radiusSq = _sphere.radius * _sphere.radius;
    const float  tca     = dot(rc, _ray.direction);
    const float  d2      = dot(rc, rc) - tca * tca;
    if (d2 > radiusSq)
        return false;
    const float thc = sqrt(radiusSq - d2);
    t0_ = tca - thc;
    t1_ = tca + thc;
    return true;
}

float AtmoRayleighPhase(const in float _LdotV)
{
    return 3.f * (1.f + _LdotV * _LdotV) / (16.f * kPi);
}

float AtmoHenyeyGreensteinPhase(const in float _LdotV, const in float _g = .76f)
{
    return (1.f - _g * _g) / (4.f * kPi * pow(1.f + _g * _g - 2.f * _g * _LdotV, 1.5f));
}

static const AtmoSphere kAtmoSphereW = { 0.xxx, kAtmosphereRadius };

// Returns false when the light path goes through the planet.
bool AtmoGetSunLight(
    const in float3  _positionW,
    const in float3  _sunLightDirection,
    out    float     opticalDepthLightR_,
    out    float     opticalDepthLightM_)
{
    const AtmoRay rayW = { _positionW, -_sunLightDirection };
    float t0, t1;
    AtmoRaySphereIntersect(rayW, kAtmoSphereW, t0, t1);

    opticalDepthLightR_ = 0.f;
    opticalDepthLightM_ = 0.f;

    const float step = t1 / float(kAtmoLightSampleSteps);

    for (uint i = 0; i < kAtmoLightSampleSteps; i++)
    {
        const float3 s      = rayW.origin + rayW.direction * (0.5f + float(i)) * step;
        const float  height = length(s) - kAtmospherePlanetRadius;
        if (height < 0.f)
            return false;

        opticalDepthLightR_ += exp(-height / kHR) * step;
        opticalDepthLightM_ += exp(-height / kHM) * step;
    }
    return true;
}

float3 AtmoGetIncidentLight(
    const in AtmoRay _rayW,
    const in float3  _sunLightDirection,
    const in float3  _sunRadiance,
    // When true, clamps the ray-march height to ground level instead of letting it go negative.
    // Needed for callers whose eye sits exactly at height 0 (e.g. the sky-ambient SH bake, which
    // samples full-sphere directions including straight down) to stay finite; the interactive sky
    // render keeps this off, since its real, non-zero eye height already keeps things well-behaved
    // and clamping perturbs its (already tuned) near-horizon falloff.
    const in bool    _clampHeightToGround = false)
{
    float t0 = 0.f, t1 = 0.f;
    if (!AtmoRaySphereIntersect(_rayW, kAtmoSphereW, t0, t1))
        return 0.xxx;

    const float step  = t1 / float(kAtmoSampleSteps);
    const float LdotV = dot(_rayW.direction, -_sunLightDirection);

    const float3 accumulationScale = _sunRadiance * step;

    float3 accumulatedRayleigh = 0.xxx;
    float3 accumulatedMie      = 0.xxx;
    float  opticalDepthR       = 0.f;
    float  opticalDepthM       = 0.f;

    const float phaseR = AtmoRayleighPhase(LdotV);
    const float phaseM = AtmoHenyeyGreensteinPhase(LdotV);

    for (uint i = 0; i < kAtmoSampleSteps; i++)
    {
        const float3 s = _rayW.origin + _rayW.direction * (0.5f + float(i)) * step;

        float height = length(s) - kAtmospherePlanetRadius;
        if (_clampHeightToGround)
            height = max(height, 0.f);

        const float hr = exp(-height / kHR);
        const float hm = exp(-height / kHM);
        opticalDepthR += hr * step;
        opticalDepthM += hm * step;

        float opticalDepthLightR, opticalDepthLightM;
        if (AtmoGetSunLight(s, _sunLightDirection, opticalDepthLightR, opticalDepthLightM))
        {
            const float3 tau         = kBetaR * (opticalDepthR + opticalDepthLightR) + kBetaM * (opticalDepthM + opticalDepthLightM);
            const float3 attenuation = exp(-tau);

            accumulatedRayleigh += hr * attenuation;
            accumulatedMie      += hm * attenuation;
        }
    }

    return accumulationScale * (kBetaR * phaseR * accumulatedRayleigh + kBetaM * phaseM * accumulatedMie);
}
