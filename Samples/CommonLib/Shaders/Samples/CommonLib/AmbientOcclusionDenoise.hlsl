/**
 * @file
 * @author Max Godefroy
 * @date 25/09/2026.
 */

#include "Platform.hlsl"
#include "AmbientOcclusionConstants.h"

// Edge-aware spatial blur over AmbientOcclusion.hlsl's raw (noisy - only a handful of
// slices/steps are affordable at real-time rates) visibility term, weighting each of the 8
// neighbours by the depth edges detected between it and the center pixel so the blur doesn't
// smear occlusion across depth discontinuities. Ported from XeGTAO_Denoise (see
// https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli),
// simplified to one pixel per thread instead of XeGTAO's two-pixels-per-thread batching, and
// without its visibility/bent-normal packing (this engine stores the raw term directly in an
// R8_UNorm texture). AmbientOcclusionPass dispatches this same shader twice, ping-ponging between
// two working textures, so it always reads/writes plain "source"/"destination" resources rather
// than baking in which pass (first or final) this invocation is.

vkBinding(0, 0) ConstantBuffer<AmbientOcclusionConstants> AoConstants: register(b0, space0);
vkBinding(1, 0) Texture2D<float> SourceAoTerm: register(t0, space0);
vkBinding(2, 0) Texture2D<float> SourceEdges: register(t1, space0);
vkBinding(3, 0) RWTexture2D<unorm float> AoTermOut: register(u0, space0);

float4 UnpackEdges(const in float _packedVal)
{
    const uint packedVal = uint(_packedVal * 255.5f);
    float4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.f;
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.f;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.f;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.f;
    return saturate(edgesLRTB);
}

[numthreads(8, 8, 1)]
void AmbientOcclusionDenoiseMain(const uint3 id: SV_DispatchThreadID)
{
    uint width, height;
    SourceAoTerm.GetDimensions(width, height);
    const int2 resolution = int2(width, height);

    if (any(int2(id.xy) >= resolution))
    {
        return;
    }

    const int2 pc = int2(id.xy);
    const int2 clampMax = resolution - 1;

    const float4 edgesC = UnpackEdges(SourceEdges.Load(int3(pc, 0)));
    const float4 edgesL = UnpackEdges(SourceEdges.Load(int3(clamp(pc + int2(-1, 0), 0, clampMax), 0)));
    const float4 edgesR = UnpackEdges(SourceEdges.Load(int3(clamp(pc + int2(1, 0), 0, clampMax), 0)));
    const float4 edgesT = UnpackEdges(SourceEdges.Load(int3(clamp(pc + int2(0, -1), 0, clampMax), 0)));
    const float4 edgesB = UnpackEdges(SourceEdges.Load(int3(clamp(pc + int2(0, 1), 0, clampMax), 0)));

    // Enforce symmetry between neighbouring edge estimates: edge detection doesn't guarantee that
    // a left edge on this pixel matches the right edge its left neighbour detected, even though
    // they should agree in the majority of cases. Sharpens the blur a little.
    float4 edges = edgesC * float4(edgesL.y, edgesR.x, edgesT.w, edgesB.z);

    // Let a small amount of AO leak in across corners with 3-4 detected edges: softens both
    // spatial and temporal aliasing there instead of hard-stopping the blur exactly at a single
    // (noisy) detected edge.
    const float leakThreshold = 2.5f;
    const float leakStrength = 0.5f;
    const float edginess = (saturate(4.f - leakThreshold - dot(edges, 1.f.xxxx)) / (4.f - leakThreshold)) * leakStrength;
    edges = saturate(edges + edginess);

    const float diagWeight = 0.85f * 0.5f;
    const float weightTL = diagWeight * (edges.x * edgesL.z + edges.z * edgesT.x);
    const float weightTR = diagWeight * (edges.z * edgesT.y + edges.y * edgesR.z);
    const float weightBL = diagWeight * (edges.w * edgesB.x + edges.x * edgesL.w);
    const float weightBR = diagWeight * (edges.y * edgesR.w + edges.w * edgesB.y);

    const float valueC = SourceAoTerm.Load(int3(pc, 0));
    const float valueL = SourceAoTerm.Load(int3(clamp(pc + int2(-1, 0), 0, clampMax), 0));
    const float valueR = SourceAoTerm.Load(int3(clamp(pc + int2(1, 0), 0, clampMax), 0));
    const float valueT = SourceAoTerm.Load(int3(clamp(pc + int2(0, -1), 0, clampMax), 0));
    const float valueB = SourceAoTerm.Load(int3(clamp(pc + int2(0, 1), 0, clampMax), 0));
    const float valueTL = SourceAoTerm.Load(int3(clamp(pc + int2(-1, -1), 0, clampMax), 0));
    const float valueTR = SourceAoTerm.Load(int3(clamp(pc + int2(1, -1), 0, clampMax), 0));
    const float valueBL = SourceAoTerm.Load(int3(clamp(pc + int2(-1, 1), 0, clampMax), 0));
    const float valueBR = SourceAoTerm.Load(int3(clamp(pc + int2(1, 1), 0, clampMax), 0));

    float weightSum = AoConstants.m_denoiseBlurBeta;
    float sum = valueC * weightSum;

    sum += valueL * edges.x; weightSum += edges.x;
    sum += valueR * edges.y; weightSum += edges.y;
    sum += valueT * edges.z; weightSum += edges.z;
    sum += valueB * edges.w; weightSum += edges.w;

    sum += valueTL * weightTL; weightSum += weightTL;
    sum += valueTR * weightTR; weightSum += weightTR;
    sum += valueBL * weightBL; weightSum += weightBL;
    sum += valueBR * weightBR; weightSum += weightBR;

    AoTermOut[pc] = sum / max(weightSum, 1e-6f);
}
