/**
 * @file
 * @author Max Godefroy
 * @date 10/04/2025.
 */

#include "Platform.hlsl"
#include "Math/CoordinateTransforms.hlsl"
#include "Math/Quaternion.hlsl"
#include "../FrameData.hlsl"

struct Input {
    float4 screenPosition: SV_Position;
};

struct Output {
    float4 color: SV_Target0;
};

vkBinding(0, 0) ConstantBuffer<FrameData> SceneConstants;

struct Ray
{
    float3 origin;
    float3 direction;
};

struct Sphere
{
    float3 origin;
    float radius;
};

static const float pi = 3.1415f;

static const float planetRadius = 6.360e6f;
static const float atmosphereRadius = 6.420e6f;

static const float3 betaR = float3(3.8e-6f, 13.5e-6f, 33.1e-6f);
static const float3 betaM = float3(21.0e-6f.xxx);

static const float hR = 7994.f;
static const float hM = 1200.f;

static const uint sampleSteps = 16;
static const uint lightSampleSteps = 8;

bool RaySphereIntersection(
    const in Ray _rayW,
    const in Sphere _sphereW,
    inout float t0_,
    inout float t1_)
{
    const float3 rc = _sphereW.origin - _rayW.origin;
    const float radiusSq = _sphereW.radius * _sphereW.radius;
    const float tca = dot(rc, _rayW.direction);
    const float d2 = dot(rc, rc) - tca * tca;
    if (d2 > radiusSq)
        return false;
    const float thc = sqrt(radiusSq - d2);
    t0_ = tca - thc;
    t1_ = tca + thc;
    return true;
}

float RayleighPhase(const in float _LdotV)
{
    return 3 * (1 + _LdotV * _LdotV) / (16.f * pi);
}

float HenyeyGreensteinPhase(const in float _LdotV, const in float _g = .76)
{
    return (1 - _g * _g) / (4 * pi * pow(1. + _g * _g - 2 * _g * _LdotV, 1.5));
}

static const Sphere atmoSphere = { 0.xxx, atmosphereRadius };

bool GetSunLight(const in float3 _positionW, out float opticalDepthLightR_, out float opticalDepthLightM_)
{
    const Ray rayW = { _positionW, -SceneConstants.m_sunLightDirection };
    float t0, t1;
    RaySphereIntersection(rayW, atmoSphere, t0, t1);

    opticalDepthLightR_ = 0;
    opticalDepthLightM_ = 0;

    float step = t1 / float(lightSampleSteps);

    for (uint i = 0; i < lightSampleSteps; i++)
    {
        const float3 s = rayW.origin + rayW.direction * (0.5f + float(i)) * step;
        const float height = length(s) - planetRadius;

        if (height < 0)
            return false;

        opticalDepthLightR_ += exp(-height / hR) * step;
        opticalDepthLightM_ += exp(-height / hM) * step;
    }

    return true;
}

float3 GetIncidentLight(const in Ray _rayW)
{
    float t0 = 0, t1 = 0;
    if (!RaySphereIntersection(_rayW, atmoSphere, t0, t1))
    {
        return 0.xxx;
    }

    const float step = t1 / sampleSteps;

    const float LdotV = dot(_rayW.direction, -SceneConstants.m_sunLightDirection);
    const float phaseR = RayleighPhase(LdotV);
    const float phaseM = HenyeyGreensteinPhase(LdotV);

    float3 accumulatedRayleigh = 0.xxx;
    float3 accumulatedMie = 0.xxx;

    float opticalDepthR = 0;
    float opticalDepthM = 0;

    for (uint i = 0; i < sampleSteps; i++)
    {
        const float3 s = _rayW.origin + _rayW.direction * (0.5f + float(i)) * step;
        const float height = length(s) - planetRadius;

        const float hr = exp(-height / hR) * step;
        const float hm = exp(-height / hM) * step;
        opticalDepthR += hr;
        opticalDepthM += hm;

        float opticalDepthLightR, opticalDepthLightM;
        const bool overGround = GetSunLight(s, opticalDepthLightR, opticalDepthLightM);

        if (overGround)
        {
            const float3 tau = betaR * (opticalDepthR + opticalDepthLightR) + betaM * (opticalDepthM + opticalDepthLightM);
            const float3 attenuation = exp(-tau);

            accumulatedRayleigh += hr * attenuation;
            accumulatedMie += hm * attenuation;
        }
    }

    return SceneConstants.m_sunDiffuse * (betaR * phaseR * accumulatedRayleigh + betaM * phaseM * accumulatedMie);
}

Output SkyMain(Input _input)
{
    Output output;

    const float2 resolution = SceneConstants.m_screenResolution;
    const float2 ndc = ScreenSpaceToNdc(_input.screenPosition.xy, resolution);

    const float aspect = resolution.x / resolution.y;
    const float3 cameraV = float3(
        ndc.x * aspect * SceneConstants.m_tanHalfFov,
        1.0f,
        ndc.y * SceneConstants.m_tanHalfFov
    );

    const float4 vsToWsQuaternion = Quaternion::Conjugate(SceneConstants.m_cameraQuaternion);
    const float3 cameraW = Quaternion::Apply(vsToWsQuaternion, normalize(cameraV));
    const float3 eyePosW = SceneConstants.m_cameraTranslation + float3(0, 0, planetRadius);

    const Ray rayW = { eyePosW, cameraW };

    output.color = float4(GetIncidentLight(rayW), 1);

    return output;
}