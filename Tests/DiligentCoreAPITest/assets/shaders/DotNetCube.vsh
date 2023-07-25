cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
};

void main(in VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.Color = VSIn.Color;
}
