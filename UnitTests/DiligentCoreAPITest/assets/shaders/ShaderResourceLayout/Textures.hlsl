Texture2D g_Tex2D_Static;
Texture2D g_Tex2D_Mut;
Texture2D g_Tex2D_Dyn;

Texture2D g_Tex2DArr_Static[STATIC_TEX_ARRAY_SIZE];
Texture2D g_Tex2DArr_Mut   [MUTABLE_TEX_ARRAY_SIZE];
Texture2D g_Tex2DArr_Dyn   [DYNAMIC_TEX_ARRAY_SIZE];

SamplerState g_Sampler;

float4 UseResources()
{
    float2 UV = float2(0.0, 0.0);
    float4 f4Color = float4(0.0, 0.0, 0.0, 0.0);
    f4Color += g_Tex2D_Static.SampleLevel(g_Sampler, UV.xy, 0.0);
    f4Color += g_Tex2D_Mut.   SampleLevel(g_Sampler, UV.xy, 0.0);
    f4Color += g_Tex2D_Dyn.   SampleLevel(g_Sampler, UV.xy, 0.0);

    int i = 0;
    for (i=0; i < STATIC_TEX_ARRAY_SIZE; ++i)
        f4Color += g_Tex2DArr_Static[i].SampleLevel(g_Sampler, UV.xy, 0.0);
    for (i=0; i < MUTABLE_TEX_ARRAY_SIZE; ++i)
        f4Color += g_Tex2DArr_Mut[i].SampleLevel(g_Sampler,  UV.xy, 0.0);
    for (i=0; i < DYNAMIC_TEX_ARRAY_SIZE; ++i)
        f4Color += g_Tex2DArr_Dyn[i].SampleLevel(g_Sampler,  UV.xy, 0.0);

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
