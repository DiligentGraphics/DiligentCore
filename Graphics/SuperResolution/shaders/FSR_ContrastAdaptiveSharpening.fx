#include "FSRStructures.fxh"

cbuffer cbFSRAttribs
{
    FSRAttribs g_FSRAttribs;
}

Texture2D<float4> g_TextureSource;

#define FFX_GPU
#define FFX_HLSL
#define FFX_HALF    0
#define FFX_HLSL_SM 50
#include "ffx_core.h"

#define FSR_RCAS_F       1
#define FSR_RCAS_DENOISE 1

FfxFloat32x4 FsrRcasLoadF(FfxInt32x2 Position)
{
    return g_TextureSource.Load(FfxInt32x3(Position, 0));
}

void FsrRcasInputF(FFX_PARAMETER_INOUT FfxFloat32 R, FFX_PARAMETER_INOUT FfxFloat32 G, FFX_PARAMETER_INOUT FfxFloat32 B)
{

}

#include "ffx_fsr1.h"

FfxFloat32x4 ComputeContrastAdaptiveSharpeningPS(in float4 Position : SV_Position) : SV_Target0
{
    FfxFloat32x3 ResultColor = FfxFloat32x3(0.0, 0.0, 0.0);
    FsrRcasF(ResultColor.r, ResultColor.g, ResultColor.b, FfxUInt32x2(Position.xy), g_FSRAttribs.RCASConstants);
    return FfxFloat32x4(ResultColor, 1.0);
}
