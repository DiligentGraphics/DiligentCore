#include "GraphicsCommon.h"

cbuffer Colors
{
    float4 g_VertColors[3];
};

struct VSInput
{
    float4 Pos : ATTRIB0;
};

#if INTERNAL_MACROS == 1 && EXTERNAL_MACROS == 2
void main(in  uint    VertId : SV_VertexID,
          in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = VSIn.Pos;
    PSIn.Color = g_VertColors[VertId % 3].rgb;
}
#endif
