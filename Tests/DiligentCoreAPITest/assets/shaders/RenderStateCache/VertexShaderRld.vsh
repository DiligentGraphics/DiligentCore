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
void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = VSIn.Pos * 1e-10;
    PSIn.Color = g_Data.VertColors[0].rgb * 1e-10;
}
// NB: no new line at the end of file!
#endif