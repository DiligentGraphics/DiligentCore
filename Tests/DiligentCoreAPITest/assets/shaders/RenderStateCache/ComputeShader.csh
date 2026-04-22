#include "Defines.h"

VK_IMAGE_FORMAT("rgba8") RWTexture2D</*format=rgba8*/ float4> g_tex2DUAV;

cbuffer cbConstants
{
    uint2 g_TexDim;
    uint2 g_TileDim;
}

StructuredBuffer<float> g_CoordinateScaleBuffer;

// Use output buffer to test https://github.com/KhronosGroup/SPIRV-Cross/issues/2613
RWStructuredBuffer<float> g_OutputBuffer;

#if INTERNAL_MACROS == 1 && EXTERNAL_MACROS == 2
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_TexDim.x || DTid.y >= g_TexDim.y)
        return;

    g_tex2DUAV[DTid.xy] = float4(float2(DTid.xy % g_TileDim.xy) / float2(g_CoordinateScaleBuffer[DTid.x], g_CoordinateScaleBuffer[DTid.y]), 0.0, 1.0);
    g_OutputBuffer[DTid.x] = g_CoordinateScaleBuffer[DTid.x];
}
#endif
