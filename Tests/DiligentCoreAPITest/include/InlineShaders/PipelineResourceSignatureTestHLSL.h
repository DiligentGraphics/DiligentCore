/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <string>

namespace
{

namespace HLSL
{

// clang-format off
const std::string PRSTest1_VS{
R"(
struct VSOutput
{
    float4 f4Position : SV_Position;
    float2 f2Texcoord : TEXCOORD0;
    float3 f3Color    : COLOR;
};

cbuffer Constants
{
    float4 g_Weight1;
    float4 g_Weight2;
};

void main(uint uiVertexId : SV_VertexID,
          out VSOutput Out)
{
    float4 Positions[3];
    Positions[0] = float4(-0.5, -0.5, 0.0, 1.0) * g_Weight1;
    Positions[1] = float4(+0.5, +0.5, 0.0, 1.0) * g_Weight1;
    Positions[2] = float4(-0.5, +0.5, 0.0, 1.0) * g_Weight1;

    float3 Color[3];
    Color[0] = float3(0.5, 0.0, 0.0);
    Color[1] = float3(0.0, 0.0, 0.5);
    Color[2] = float3(0.0, 0.5, 0.0);

    Out.f4Position = Positions[uiVertexId];
    Out.f2Texcoord = Positions[uiVertexId].xy;
    Out.f3Color    = Color[uiVertexId];
}
)"
};

const std::string PRSTest1_PS{
R"(
struct PSInput
{
    float4 f4Position : SV_Position;
    float2 f2Texcoord : TEXCOORD0;
    float3 f3Color    : COLOR;
};
 
cbuffer Constants
{
    float4 g_Weight1;
    float4 g_Weight2;
};

Texture2D    g_Texture;
SamplerState g_Texture_sampler;

void main(PSInput In,
          out float4 Color : SV_Target)
{
    Color = float4(In.f3Color, 1.0) * g_Weight2;
    Color *= g_Texture.Sample(g_Texture_sampler, In.f2Texcoord);
}
)"
};


const std::string PRSTest2_PS{
R"(
struct PSInput
{
    float4 f4Position : SV_Position;
    float2 f2Texcoord : TEXCOORD0;
    float3 f3Color    : COLOR;
};
 
cbuffer Constants
{
    float4 g_Weight1;
    float4 g_Weight2;
};

Texture2D    g_Texture;
SamplerState g_Texture_sampler;

Texture2D    g_Texture2;
SamplerState g_Texture2_sampler;

void main(PSInput In,
          out float4 Color : SV_Target)
{
    Color = float4(In.f3Color, 1.0) * g_Weight2;
    Color *= g_Texture.Sample(g_Texture_sampler, In.f2Texcoord);
    Color += g_Texture2.Sample(g_Texture2_sampler, In.f2Texcoord * 2.0);
}
)"
};

// clang-format on
} // namespace HLSL

} // namespace
