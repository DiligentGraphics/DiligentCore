struct PSOutput
{
    float4 Color : SV_Target0;
};

struct PSInput
{
    float4 Pos : SV_Position;
};

# define PS_OUTPUT PSOutput
# define PS_INPUT  PSInput

#ifdef MACRO0
#   define PS_OUTPUT abc
#elif defined(MACRO1)
#   define PS_OUTPUT xyz
#endif

PS_OUTPUT main1(in PS_INPUT PSIn)
{
    PS_OUTPUT PSOut;
    PSOut.Color = float4(PSIn.Pos.xy, 0.0, 0.0);
    return PSOut;
}

#define FLOAT4 float4
#define FLOAT3 float3

FLOAT4 main2(in PS_INPUT PSIn) : SV_Target
{
    return FLOAT4(PSIn.Pos.xy, 0.0, 0.0);
}

#define VOID void
#define MACRO1

VOID main3()
{
}

#define MACRO2