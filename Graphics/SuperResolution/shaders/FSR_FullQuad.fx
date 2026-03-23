float4 FSR_FullQuadVS(in uint VertexId : SV_VertexID) : SV_Position
{
    float2 PosXY[3];
    PosXY[0] = float2(-1.0, -1.0);
    PosXY[1] = float2(-1.0, +3.0);
    PosXY[2] = float2(+3.0, -1.0);

    return float4(PosXY[VertexId % 3u], 0.0, 1.0);
}
