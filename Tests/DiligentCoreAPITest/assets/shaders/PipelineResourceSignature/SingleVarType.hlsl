Texture2D g_Tex2D_1;
Texture2D g_Tex2D_2;

SamplerState g_Sampler;

cbuffer ConstBuff_1
{
    float4 g_Data_1;
}

cbuffer ConstBuff_2
{
    float4 g_Data_2;
}

float4 UseResources()
{
    float2 UV = float2(0.0, 0.0);
    float4 f4Color = float4(0.0, 0.0, 0.0, 0.0);
    f4Color += g_Tex2D_1.SampleLevel(g_Sampler, UV.xy, 0.0);
    f4Color += g_Tex2D_2.SampleLevel(g_Sampler, UV.xy, 0.0);
    f4Color += g_Data_1 + g_Data_2;

	return f4Color;
}

void VSMain(out float4 f4Color    : COLOR,
            out float4 f4Position : SV_Position)
{
    f4Color = UseResources();
    f4Position = float4(0.0, 0.0, 0.0, 1.0);
}

float4 PSMain(in float4 in_f4Color : COLOR,
              in float4 f4Position : SV_Position) : SV_Target
{
    return in_f4Color + UseResources();
}
