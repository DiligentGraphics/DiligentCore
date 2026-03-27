#include "FSRStructures.fxh"

cbuffer cbFSRAttribs
{
    FSRAttribs g_FSRAttribs;
}

Texture2D<float4> g_TextureSource;
SamplerState      g_TextureSource_sampler;

#define FFX_GPU
#define FFX_HLSL
#define FFX_HALF    0
#define FFX_HLSL_SM 50
#include "ffx_core.h"

#define FFX_FSR_EASU_FLOAT 1

FfxFloat32x4 FsrEasuRF(FfxFloat32x2 Texcoord)
{
#ifdef FSR_FEATURE_TEXTURE_GATHER
    return g_TextureSource.GatherRed(g_TextureSource_sampler, Texcoord);
#else
    float2 Position = g_FSRAttribs.SourceSize.xy * Texcoord - float2(0.5, 0.5);
    FfxFloat32x4 Gather;
    Gather.x = g_TextureSource.Load(int3(int2(Position) + int2(0, 1), 0)).r;
    Gather.y = g_TextureSource.Load(int3(int2(Position) + int2(1, 1), 0)).r;
    Gather.z = g_TextureSource.Load(int3(int2(Position) + int2(1, 0), 0)).r;
    Gather.w = g_TextureSource.Load(int3(int2(Position) + int2(0, 0), 0)).r;
    return Gather;
#endif
}

FfxFloat32x4 FsrEasuGF(FfxFloat32x2 Texcoord)
{
#ifdef FSR_FEATURE_TEXTURE_GATHER
    return g_TextureSource.GatherGreen(g_TextureSource_sampler, Texcoord);
#else
    float2 Position = g_FSRAttribs.SourceSize.xy * Texcoord - float2(0.5, 0.5);
    FfxFloat32x4 Gather;
    Gather.x = g_TextureSource.Load(int3(int2(Position) + int2(0, 1), 0)).g;
    Gather.y = g_TextureSource.Load(int3(int2(Position) + int2(1, 1), 0)).g;
    Gather.z = g_TextureSource.Load(int3(int2(Position) + int2(1, 0), 0)).g;
    Gather.w = g_TextureSource.Load(int3(int2(Position) + int2(0, 0), 0)).g;
    return Gather;
#endif
}

FfxFloat32x4 FsrEasuBF(FfxFloat32x2 Texcoord)
{
#ifdef FSR_FEATURE_TEXTURE_GATHER
    return g_TextureSource.GatherBlue(g_TextureSource_sampler, Texcoord);
#else
    float2 Position = g_FSRAttribs.SourceSize.xy * Texcoord - float2(0.5, 0.5);
    FfxFloat32x4 Gather;
    Gather.x = g_TextureSource.Load(int3(int2(Position) + int2(0, 1), 0)).b;
    Gather.y = g_TextureSource.Load(int3(int2(Position) + int2(1, 1), 0)).b;
    Gather.z = g_TextureSource.Load(int3(int2(Position) + int2(1, 0), 0)).b;
    Gather.w = g_TextureSource.Load(int3(int2(Position) + int2(0, 0), 0)).b;
    return Gather;
#endif
}

#include "ffx_fsr1.h"

FfxFloat32x4 ComputeEdgeAdaptiveUpsamplingPS(in float4 Position : SV_Position) : SV_Target0
{
    FfxFloat32x3 ResultColor = FfxFloat32x3(0.0, 0.0, 0.0);
    ffxFsrEasuFloat(ResultColor, FfxUInt32x2(Position.xy),
                    g_FSRAttribs.EASUConstants0, g_FSRAttribs.EASUConstants1,
                    g_FSRAttribs.EASUConstants2, g_FSRAttribs.EASUConstants3);
    return FfxFloat32x4(ResultColor, 1.0);
}
