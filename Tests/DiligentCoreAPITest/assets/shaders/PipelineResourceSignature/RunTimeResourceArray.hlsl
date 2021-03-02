Texture2D g_Textures[] : register(t0, space1);
SamplerState g_Sampler;

struct CBData
{
    float4 Data;
};
ConstantBuffer<CBData> g_ConstantBuffers[] : register(b10, space5);

float4 CheckValue(float4 Val, float4 Expected)
{
    return float4(Val.x == Expected.x ? 1.0 : 0.0,
                  Val.y == Expected.y ? 1.0 : 0.0,
                  Val.z == Expected.z ? 1.0 : 0.0,
                  Val.w == Expected.w ? 1.0 : 0.0);
}


float4 VerifyResources(uint index, float2 coord)
{
    float4 TexRefValues[NUM_TEXTURES];
    TexRefValues[0] = Tex2D_Ref0;
    TexRefValues[1] = Tex2D_Ref1;
    TexRefValues[2] = Tex2D_Ref2;
    TexRefValues[3] = Tex2D_Ref3;
    TexRefValues[4] = Tex2D_Ref4;
    TexRefValues[5] = Tex2D_Ref5;
    TexRefValues[6] = Tex2D_Ref6;
    TexRefValues[7] = Tex2D_Ref7;

    float4 ConstBuffRefValues[NUM_CONST_BUFFERS];
    ConstBuffRefValues[0] = ConstBuff_Ref0;
    ConstBuffRefValues[1] = ConstBuff_Ref1;
    ConstBuffRefValues[2] = ConstBuff_Ref2;
    ConstBuffRefValues[3] = ConstBuff_Ref3;
    ConstBuffRefValues[4] = ConstBuff_Ref4;
    ConstBuffRefValues[5] = ConstBuff_Ref5;
    ConstBuffRefValues[6] = ConstBuff_Ref6;

    uint TexIdx = index % NUM_TEXTURES;
    uint BuffIdx = index % NUM_CONST_BUFFERS;

    float4 AllCorrect = float4(1.0, 1.0, 1.0, 1.0);
    AllCorrect *= CheckValue(g_Textures[NonUniformResourceIndex(TexIdx)].SampleLevel(g_Sampler, coord, 0.0), TexRefValues[TexIdx]);
    AllCorrect *= CheckValue(g_ConstantBuffers[NonUniformResourceIndex(BuffIdx)].Data, ConstBuffRefValues[BuffIdx]);

    return AllCorrect;
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
    float2 uv = float2(GlobalInvocationID.xy + float2(0.5,0.5)) / float2(Dim);
    Color *= VerifyResources(LocalInvocationIndex, uv);

    g_OutImage[GlobalInvocationID.xy] = Color;
}
