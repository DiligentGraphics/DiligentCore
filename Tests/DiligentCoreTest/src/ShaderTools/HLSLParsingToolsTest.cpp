/*
 *  Copyright 2024 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "HLSLParsingTools.hpp"

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static constexpr char g_TestHLSL_Formats[] = R"(
RWTexture1D<unorm float4 /*format=rgba8*/> g_rgba8;
RWTexture2D<unorm  /*format=rg8*/ float4>  g_rg8;
RWTexture3D</*format=r8*/ unorm float4>    g_r8;

RWTexture1D<unorm float4 /*format =rgba16*/> g_rgba16[1];
RWTexture2D<unorm  /*format =rg16*/ float4>  g_rg16[2];
RWTexture3D</*format =r16*/ unorm float4>    g_r16[3];

RWTexture1D<unorm float4 /*format= rgba16f*/> g_rgba16f[1];
RWTexture2D<unorm  /*format= rg16f*/ float4>  g_rg16f[2];
RWTexture3D</*format= r16f*/ unorm float4>    g_r16f[3];

RWTexture1DArray<unorm float4 /*format = rgba32f*/> g_rgba32f[1];
RWTexture2DArray<unorm  /*format = rg32f*/ float4>  g_rg32f[2];
RWTexture3D     </*format = r32f*/ unorm float4>    g_r32f[3];

RWTexture1D<unorm float4 /*format=rgba8i*/> g_rgba8i;
RWTexture2D<unorm  /*format=rg8i*/ float4>  g_rg8i;
RWTexture3D</*format=r8i*/ unorm float4>    g_r8i;

RWTexture1D<unorm float4 /* format=rgba16i */> g_rgba16i;
RWTexture2D<unorm  /* format=rg16i */ float4>  g_rg16i;
RWTexture3D</* format=r16i */ unorm float4>    g_r16i;

RWTexture1D<unorm float4 /* format=rgba32i*/> g_rgba32i;
RWTexture2D<unorm  /* format=rg32i*/ float4>  g_rg32i;
RWTexture3D</* format=r32i*/ unorm float4>    g_r32i;

RWTexture1D<unorm float4 /*format=rgba8ui */> g_rgba8ui;
RWTexture2D<unorm  /*format=rg8ui */ float4>  g_rg8ui;
RWTexture3D</*format=r8ui */ unorm float4>    g_r8ui;

RWTexture1D<unorm float4 /*format =rgba16ui*/> g_rgba16ui;
RWTexture2D<unorm  /*format =rg16ui*/ float4>  g_rg16ui;
RWTexture3D</*format =r16ui*/ unorm float4>    g_r16ui;

RWTexture1D<unorm float4 /*format= rgba32ui*/> g_rgba32ui;
RWTexture2D<unorm  /*format= rg32ui*/ float4>  g_rg32ui;
RWTexture3D</*format= r32ui*/ unorm float4>    g_r32ui;

RWTexture1D<unorm float4 /*format = rgba8_snorm*/> g_rgba8_snorm;
RWTexture2D<unorm  /*format = rg8_snorm*/ float4>  g_rg8_snorm;
RWTexture3D</*format = r8_snorm*/ unorm float4>    g_r8_snorm;

RWTexture1D<unorm float4 /*format=rgba16_snorm*/> g_rgba16_snorm[1];
RWTexture2D<unorm  /*format=rg16_snorm*/ float4>  g_rg16_snorm[2];
RWTexture3D</*format=r16_snorm*/ unorm float4>    g_r16_snorm[3];

RWTexture1D<unorm float4 /*format=r11f_g11f_b10f*/> g_r11f_g11f_b10f[1];
RWTexture2D<unorm  /*format=rgb10_a2*/ float4>      g_rgb10_a2[2];
RWTexture3D</*format=rgb10_a2ui*/ unorm float4>     g_rgb10_a2ui[3];

RWTexture2D g_RWTex;

void Function(RWTexture2D</*format=rg8*/ unorm float4> FunctionArg1,
              RWTexture2D<unorm /*format=rg8*/ float4> FunctionArg2,
              RWTexture2D<unorm float4 /*format=rg8*/> FunctionArg3)
{
    RWTexture2D</*format=rg8*/ unorm float4> LocalRWTex0;
    RWTexture2D<unorm /*format=rg8*/ float4> LocalRWTex1;
    RWTexture2D<unorm float4 /*format=rg8*/> LocalRWTex2;
}

)";

TEST(HLSLParsingTools, ExtractGLSLImageFormatsFromHLSL)
{
    std::unordered_map<HashMapStringKey, ImageFormatAndAccess> RefFormats;
    RefFormats.emplace("g_rgba8", TEX_FORMAT_RGBA8_UNORM);
    RefFormats.emplace("g_rg8", TEX_FORMAT_RG8_UNORM);
    RefFormats.emplace("g_r8", TEX_FORMAT_R8_UNORM);

    RefFormats.emplace("g_rgba16", TEX_FORMAT_RGBA16_UNORM);
    RefFormats.emplace("g_rg16", TEX_FORMAT_RG16_UNORM);
    RefFormats.emplace("g_r16", TEX_FORMAT_R16_UNORM);

    RefFormats.emplace("g_rgba16f", TEX_FORMAT_RGBA16_FLOAT);
    RefFormats.emplace("g_rg16f", TEX_FORMAT_RG16_FLOAT);
    RefFormats.emplace("g_r16f", TEX_FORMAT_R16_FLOAT);

    RefFormats.emplace("g_rgba32f", TEX_FORMAT_RGBA32_FLOAT);
    RefFormats.emplace("g_rg32f", TEX_FORMAT_RG32_FLOAT);
    RefFormats.emplace("g_r32f", TEX_FORMAT_R32_FLOAT);

    RefFormats.emplace("g_rgba8i", TEX_FORMAT_RGBA8_SINT);
    RefFormats.emplace("g_rg8i", TEX_FORMAT_RG8_SINT);
    RefFormats.emplace("g_r8i", TEX_FORMAT_R8_SINT);

    RefFormats.emplace("g_rgba16i", TEX_FORMAT_RGBA16_SINT);
    RefFormats.emplace("g_rg16i", TEX_FORMAT_RG16_SINT);
    RefFormats.emplace("g_r16i", TEX_FORMAT_R16_SINT);

    RefFormats.emplace("g_rgba32i", TEX_FORMAT_RGBA32_SINT);
    RefFormats.emplace("g_rg32i", TEX_FORMAT_RG32_SINT);
    RefFormats.emplace("g_r32i", TEX_FORMAT_R32_SINT);

    RefFormats.emplace("g_rgba8ui", TEX_FORMAT_RGBA8_UINT);
    RefFormats.emplace("g_rg8ui", TEX_FORMAT_RG8_UINT);
    RefFormats.emplace("g_r8ui", TEX_FORMAT_R8_UINT);

    RefFormats.emplace("g_rgba16ui", TEX_FORMAT_RGBA16_UINT);
    RefFormats.emplace("g_rg16ui", TEX_FORMAT_RG16_UINT);
    RefFormats.emplace("g_r16ui", TEX_FORMAT_R16_UINT);

    RefFormats.emplace("g_rgba32ui", TEX_FORMAT_RGBA32_UINT);
    RefFormats.emplace("g_rg32ui", TEX_FORMAT_RG32_UINT);
    RefFormats.emplace("g_r32ui", TEX_FORMAT_R32_UINT);

    RefFormats.emplace("g_rgba8_snorm", TEX_FORMAT_RGBA8_SNORM);
    RefFormats.emplace("g_rg8_snorm", TEX_FORMAT_RG8_SNORM);
    RefFormats.emplace("g_r8_snorm", TEX_FORMAT_R8_SNORM);

    RefFormats.emplace("g_rgba16_snorm", TEX_FORMAT_RGBA16_SNORM);
    RefFormats.emplace("g_rg16_snorm", TEX_FORMAT_RG16_SNORM);
    RefFormats.emplace("g_r16_snorm", TEX_FORMAT_R16_SNORM);

    RefFormats.emplace("g_r11f_g11f_b10f", TEX_FORMAT_R11G11B10_FLOAT);
    RefFormats.emplace("g_rgb10_a2", TEX_FORMAT_RGB10A2_UNORM);
    RefFormats.emplace("g_rgb10_a2ui", TEX_FORMAT_RGB10A2_UINT);

    std::unordered_map<HashMapStringKey, ImageFormatAndAccess> Formats = Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL(g_TestHLSL_Formats);
    EXPECT_EQ(Formats.size(), RefFormats.size());
    for (const auto& RefFmtIt : RefFormats)
    {
        const auto& FmtIt = Formats.find(RefFmtIt.first);
        if (FmtIt == Formats.end())
            ADD_FAILURE() << "Unable to find image format for resource " << RefFmtIt.first.GetStr();
        if (FmtIt != Formats.end())
        {
            EXPECT_EQ(FmtIt->second, RefFmtIt.second) << "Incorrect format for resource " << RefFmtIt.first.GetStr();
        }
    }

    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D<").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D<>").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D</*format=*/>").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D</*format=xyz*/>").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D</*format=rgba8*/>").empty());
    EXPECT_TRUE(Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL("RWTexture2D</*format=rgba8*/> 123").empty());
}


static constexpr char g_TestHLSL_AccessMode[] = R"(
RWTexture1D<unorm float4 /*access=read*/>       g_ReadOnly;
RWTexture1D</*access=write*/ unorm float4>      g_WriteOnly;
RWTexture1D<unorm /*access=read_write*/ float4> g_ReadWrite;

RWTexture1D</*access=read_write*/ float4> g_ReadWrite_1;
RWTexture1D<float4 /*access=read_write*/> g_ReadWrite_2;

RWTexture1D<unorm float4 /*access=read_write*/ /*format=rgba32f*/>     g_ReadWriteFmt_01;
RWTexture1D<unorm float4 /*format=rgba32f*/     /*access=read_write*/> g_ReadWriteFmt_02;
RWTexture1D<unorm /*access=read_write*/ /*format=rgba32f*/     float4> g_ReadWriteFmt_03;
RWTexture1D<unorm /*format=rgba32f*/     /*access=read_write*/ float4> g_ReadWriteFmt_04;
RWTexture1D<unorm /*format=rgba32f*/ float4 /*access=read_write*/>       g_ReadWriteFmt_05;
RWTexture1D<unorm /*access=read_write*/ float4 /*format=rgba32f*/>  g_ReadWriteFmt_06;

RWTexture2D g_ReadWriteNoneVisible;

void Function(RWTexture2D</*format=rg8*/ unorm /*access=write*/ float4> FunctionArg1,
              RWTexture2D<unorm /*format=rg8*/ float4> FunctionArg2,
              RWTexture2D<unorm float4 /*format=rg8*/> FunctionArg3)
{
    RWTexture2D</*format=rg8*/ unorm float4> LocalRWTex0;
    RWTexture2D<unorm /*format=rg8*/ float4> LocalRWTex1;
    RWTexture2D<unorm float4 /*format=rg8*/> LocalRWTex2;
}

)";

TEST(HLSLParsingTools, ExtractGLSLAccessModeFromHLSL)
{
    std::unordered_map<HashMapStringKey, ImageFormatAndAccess> RefFormats;
    RefFormats.emplace("g_ReadOnly", ImageFormatAndAccess{TEX_FORMAT_UNKNOWN, IMAGE_ACCESS_MODE_READ});
    RefFormats.emplace("g_WriteOnly", ImageFormatAndAccess{TEX_FORMAT_UNKNOWN, IMAGE_ACCESS_MODE_WRITE});
    RefFormats.emplace("g_ReadWrite", ImageFormatAndAccess{TEX_FORMAT_UNKNOWN, IMAGE_ACCESS_MODE_READ_WRITE});

    RefFormats.emplace("g_ReadWrite_1", ImageFormatAndAccess{TEX_FORMAT_UNKNOWN, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWrite_2", ImageFormatAndAccess{TEX_FORMAT_UNKNOWN, IMAGE_ACCESS_MODE_READ_WRITE});

    RefFormats.emplace("g_ReadWriteFmt_01", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWriteFmt_02", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWriteFmt_03", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWriteFmt_04", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWriteFmt_05", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});
    RefFormats.emplace("g_ReadWriteFmt_06", ImageFormatAndAccess{TEX_FORMAT_RGBA32_FLOAT, IMAGE_ACCESS_MODE_READ_WRITE});


    std::unordered_map<HashMapStringKey, ImageFormatAndAccess> Formats = Parsing::ExtractGLSLImageFormatsAndAccessModeFromHLSL(g_TestHLSL_AccessMode);
    EXPECT_EQ(Formats.size(), RefFormats.size());
    for (const auto& RefFmtIt : RefFormats)
    {
        const auto& FmtIt = Formats.find(RefFmtIt.first);
        if (FmtIt == Formats.end())
            ADD_FAILURE() << "Unable to find image format for resource " << RefFmtIt.first.GetStr();
        if (FmtIt != Formats.end())
        {
            EXPECT_EQ(FmtIt->second, RefFmtIt.second) << "Incorrect format for resource " << RefFmtIt.first.GetStr();
        }
    }
}

} // namespace
