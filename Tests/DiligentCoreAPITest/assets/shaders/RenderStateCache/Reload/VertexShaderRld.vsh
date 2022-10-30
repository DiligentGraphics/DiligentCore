#include "GraphicsCommon.h"

struct VSInput
{
    float4 Pos   : ATTRIB0;
    float3 Color : ATTRIB1;
};

#if INTERNAL_MACROS == 1 && EXTERNAL_MACROS == 2
void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = VSIn.Pos;
    PSIn.Color = VSIn.Color;
}
// NB: no new line at the end of file!
#endif