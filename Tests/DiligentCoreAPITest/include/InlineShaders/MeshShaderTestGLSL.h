/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

namespace GLSL
{

// clang-format off
const std::string MeshShaderTest_MS{
R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x=4) in;
layout(max_vertices=4, max_primitives=2) out;
layout(triangles) out;

//out uvec3 gl_PrimitiveTriangleIndicesEXT[max_primitives]

//out gl_MeshPerVertexEXT {
//  vec4 gl_Position;
//} gl_MeshVerticesEXT[max_vertices]

layout(location = 0) out vec3 out_Color[];

const vec3 colors[4] = {vec3(1.0,0.0,0.0), vec3(0.0,1.0,0.0), vec3(0.0,0.0,1.0), vec3(1.0,1.0,1.0)};

void main ()
{
    const uint I = gl_LocalInvocationID.x;

    // first triangle
    if (I == 0)
    {
        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
    }

    // second triangle
    if (I == 3)
    {
        gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2, 1, 3);
    }

    gl_MeshVerticesEXT[I].gl_Position = vec4(float(I >> 1) * 2.0 - 1.0, float(I & 1) * 2.0 - 1.0, 0.0, 1.0);

    out_Color[I] = colors[I];

    // only one thread writes output primitive count
    if (I == 0)
    {
        uint vertexCount    = 4;
        uint primitiveCount = 2;
        SetMeshOutputsEXT(vertexCount, primitiveCount);
    }
}
)"
};

const std::string MeshShaderTest_FS{
R"(
#version 460

layout(location = 0) in  vec3 in_Color;
layout(location = 0) out vec4 out_Color;

void main()
{
    out_Color = vec4(in_Color, 1.0);
}
)"
};


const std::string AmplificationShaderTest_TS{
R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 8) in;

struct Payload
{    
    uint baseID;
    uint subIDs[8];
};
taskPayloadSharedEXT Payload Output;

void main()
{
    const uint I = gl_LocalInvocationID.x;

    Output.subIDs[I] = I;

    if (I == 0)
    {
        Output.baseID = gl_WorkGroupID.x * 8;
        EmitMeshTasksEXT(8, 1, 1);
    }
}
)"
};

const std::string AmplificationShaderTest_MS{
R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(max_vertices = 3, max_primitives = 1) out;
layout(triangles) out;

struct Payload
{    
    uint baseID;
    uint subIDs[8];
};
taskPayloadSharedEXT Payload Input;

layout(location = 0) out vec3 out_Color[];

const vec3 colors[4] = {vec3(1.0,0.0,0.0), vec3(0.0,1.0,0.0), vec3(0.0,0.0,1.0), vec3(1.0,0.0,1.0)};

void main ()
{
    uint meshletID = Input.baseID + Input.subIDs[gl_WorkGroupID.x];

    vec2 center;
    center.x = (float((meshletID % 9) + 1) / 10.0) * 2.0 - 1.0;
    center.y = (float((meshletID / 9) + 1) / 10.0) * 2.0 - 1.0;

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(2, 1, 0);

    gl_MeshVerticesEXT[0].gl_Position = vec4(center.x, center.y + 0.09, 0.0, 1.0);
    gl_MeshVerticesEXT[1].gl_Position = vec4(center.x - 0.09, center.y - 0.09, 0.0, 1.0);
    gl_MeshVerticesEXT[2].gl_Position = vec4(center.x + 0.09, center.y - 0.09, 0.0, 1.0);

    out_Color[0] = colors[meshletID & 3];
    out_Color[1] = colors[meshletID & 3];
    out_Color[2] = colors[meshletID & 3];

    uint vertexCount    = 3;
    uint primitiveCount = 1;
    SetMeshOutputsEXT(vertexCount, primitiveCount);
}
)"
};

const std::string AmplificationShaderTest_FS{
R"(
#version 450

layout(location = 0) in  vec3 in_Color;
layout(location = 0) out vec4 out_Color;

void main()
{
    out_Color = vec4(in_Color, 1.0);
}
)"
};

// clang-format on

} // namespace GLSL

} // namespace
