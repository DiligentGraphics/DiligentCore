#include "GraphicsCommon.h"

cbuffer Colors
{
    ReloadTestData g_Data;
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
    PSIn.Color = g_Data.VertColors[VertId % 3].rgb;
}
// NB: no new line at the end of file!
#endif