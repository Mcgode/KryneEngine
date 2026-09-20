/**
 * @file
 * @author Max Godefroy
 * @date 20/09/2026.
 */

#include "Platform.hlsl"

vkBinding(0, 0) Texture2D<float> GBufferDepth: register(t0, space0);
vkBinding(1, 0) RWTexture2D<float> DeferredShadows: register(u0, space0);

[numthreads(8, 8, 1)]
void DeferredShadowsMain(const uint3 id: SV_DispatchThreadID)
{
    uint2 resolution;
    GBufferDepth.GetDimensions(resolution.x, resolution.y);

    const uint2 pixelCoordinates = id.xy;
    if (any(pixelCoordinates >= resolution))
    {
        return;
    }

    // Reversed-Z: the GBuffer depth is cleared to 0, which is the far plane.
    const float depthSs = GBufferDepth.Load(int3(pixelCoordinates, 0));
    if (depthSs == 0.f)
    {
        return;
    }

    // Placeholder: unshadowed everywhere the depth buffer was written to.
    DeferredShadows[pixelCoordinates] = 1.f;
}
