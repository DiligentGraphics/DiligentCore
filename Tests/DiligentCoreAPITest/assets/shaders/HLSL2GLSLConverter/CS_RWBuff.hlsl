RWBuffer</* format = r32f */ float >      TexBuff_F/*comment*/ : /*comment*/register(u0)/*comment*/;
RWBuffer</* format = rg16i */ int2 >      TexBuff_I;
RWBuffer</* format = rgba16ui */ uint4 >  TexBuff_U;

struct StorageBufferStruct
{
    float4 Data;
};
struct StorageBufferStruct1
{
    float4 Data;
};
struct StorageBufferStruct2
{
    float4 Data;
};

RWStructuredBuffer<StorageBufferStruct>  RWStructBuff0 /*comment*/:/*comment*/ register(u1)/*comment*/;
RWStructuredBuffer<StorageBufferStruct1> RWStructBuff1;
RWStructuredBuffer<StorageBufferStruct2> RWStructBuff2 : register(u2);

RWStructuredBuffer</*comment*/ int /*comment*/> RWStructBuff3;

void TestGetDimensions()
{
    //RWBuffer
    {
        uint uWidth;
        int iWidth;
        float fWidth;
        TexBuff_F.GetDimensions(uWidth);
        //TexBuff_I.GetDimensions(iWidth);
        //TexBuff_U.GetDimensions(fWidth);
    }
}


void TestLoad()
{
    int4 Location = int4(2, 5, 1, 10);

    //Buffer
    {
        TexBuff_F.Load(Location.x);
        TexBuff_I.Load(Location.x);
        TexBuff_U.Load(Location.x);
    }
    StorageBufferStruct  Data0 = RWStructBuff0[Location.x];
    StorageBufferStruct1 Data1 = RWStructBuff1[Location.y];
    StorageBufferStruct2 Data2 = RWStructBuff2[Location.w];
    
    int Data4 = RWStructBuff3[Location.z];
}



void TestStore(uint3 Location)
{
    //Buffer
    {
        TexBuff_F[Location.x] = 1.0;
        TexBuff_I[Location.x] = int2(1,2);
        TexBuff_U[Location.x] = uint4(1,2,3,4);
    }
    StorageBufferStruct  Data0;
    StorageBufferStruct1 Data1;
    StorageBufferStruct2 Data2;
    Data0.Data = float4(0.0, 1.0, 2.0, 3.0);
    Data1.Data = float4(0.0, 1.0, 2.0, 3.0);
    Data2.Data = float4(0.0, 1.0, 2.0, 3.0);
    RWStructBuff0[Location.x] = Data0;
    RWStructBuff1[Location.z] = Data1;
    RWStructBuff2[Location.y] = Data2;
    RWStructBuff3[Location.x] = 16;
}

struct CSInputSubstr
{
    uint3 DTid : SV_DispatchThreadID;
};
struct CSInput
{
    uint GroupInd : SV_GroupIndex;
    CSInputSubstr substr;
};

[numthreads(2,4,8)]
void TestCS(CSInput In,
            uint3 Gid : SV_GroupID,
            uint3 GTid : SV_GroupThreadID)
{
    TestGetDimensions();
    TestLoad();
    TestStore(GTid);
}
