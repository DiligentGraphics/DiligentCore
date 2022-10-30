#include "GraphicsCommon.h"

#if INTERNAL_MACROS == 1 && EXTERNAL_MACROS == 2
void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn)
{
    PSIn.Pos   = float4(0.0, 0.0, 0.0, 0.0);
    PSIn.Color = float3(0.0, 0.0, 0.0);
}
// NB: no new line at the end of file!
#endif