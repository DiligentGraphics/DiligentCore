Texture2D g_Textures[] : register(t0, space1);
SamplerState g_Sampler;

float4 CheckValue(float4 Val, float4 Expected)
{
    return float4(Val.x == Expected.x ? 1.0 : 0.0,
                  Val.y == Expected.y ? 1.0 : 0.0,
                  Val.z == Expected.z ? 1.0 : 0.0,
                  Val.w == Expected.w ? 1.0 : 0.0);
}


float4 VerifyResources(uint index, float2 coord)
{
    float4 RefValues[NUM_TEXTURES];
    RefValues[0] = Tex2D_Ref0;
    RefValues[1] = Tex2D_Ref1;
    RefValues[2] = Tex2D_Ref2;
    RefValues[3] = Tex2D_Ref3;
    RefValues[4] = Tex2D_Ref4;
    RefValues[5] = Tex2D_Ref5;
    RefValues[6] = Tex2D_Ref6;
    RefValues[7] = Tex2D_Ref7;

    return CheckValue(g_Textures[index].Sample(g_Sampler, coord, 0.0), RefValues[index]);
}

RWTexture2D<float4>  g_OutImage;

[numthreads(16, 16, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID,
          uint  LocalInvocationIndex : SV_GroupIndex)
{
    uint2 Dim;
    g_OutImage.GetDimensions(Dim.x, Dim.y);
    if (GlobalInvocationID.x >= Dim.x || GlobalInvocationID.y >= Dim.y)
        return;

    float4 Color = float4(float2(GlobalInvocationID.xy % 256u) / 256.0, 0.0, 1.0);
    float2 uv = float2(GlobalInvocationID.xy + float2(0.5,0.5)) / float(Dim);
    Color *= VerifyResources(LocalInvocationIndex % NUM_TEXTURES, uv);

    g_OutImage[GlobalInvocationID.xy] = Color;
}
