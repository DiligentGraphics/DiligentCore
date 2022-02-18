#include "Common.h"

struct VSInput
{
    float4 Pos   : ATTRIB0;
    float3 Color : ATTRIB1;
    float2 UV    : ATTRIB2;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = VSIn.Pos;
    PSIn.Color = VSIn.Color;
    PSIn.UV    = VSIn.UV * UVScale.xy;
}
