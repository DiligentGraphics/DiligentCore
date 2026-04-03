/*
 *  Copyright 2026 Diligent Graphics LLC
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

namespace MSL
{

// clang-format off
const std::string MeshShaderTest{
R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut
{
    float4 position [[position]];
    float3 color;
};

using TriMesh = metal::mesh<VertexOut, void, 4, 2, metal::topology::triangle>;

[[mesh]]
void MSmain(uint tid [[thread_index_in_threadgroup]],
            TriMesh output)
{
    if (tid == 0)
        output.set_primitive_count(2);

    const float3 colors[4] = {float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                              float3(0.0, 0.0, 1.0), float3(1.0, 1.0, 1.0)};

    VertexOut v;
    v.position = float4(float(tid >> 1) * 2.0 - 1.0, float(tid & 1) * 2.0 - 1.0, 0.0, 1.0);
    v.color = colors[tid];
    output.set_vertex(tid, v);

    // Triangle 0: (0, 1, 2)
    if (tid == 0)
    {
        output.set_index(0, 0);
        output.set_index(1, 1);
        output.set_index(2, 2);
    }
    // Triangle 1: (2, 1, 3)
    if (tid == 3)
    {
        output.set_index(3, 2);
        output.set_index(4, 1);
        output.set_index(5, 3);
    }
}

struct FSOut
{
    float4 color [[color(0)]];
};

fragment FSOut PSmain(VertexOut in [[stage_in]])
{
    FSOut out;
    out.color = float4(in.color, 1.0);
    return out;
}
)"
};

const std::string AmplificationShaderTest{
R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut
{
    float4 position [[position]];
    float3 color;
};

struct Payload
{
    uint baseID;
    uint subIDs[8];
};

// Object (amplification) shader
[[object]]
void OBJmain(uint tid [[thread_index_in_threadgroup]],
             uint gid [[threadgroup_position_in_grid]],
             object_data Payload& payload [[payload]],
             mesh_grid_properties mgp)
{
    if (tid == 0)
        payload.baseID = gid * 8;
    payload.subIDs[tid] = tid;

    if (tid == 0)
        mgp.set_threadgroups_per_grid(uint3(8, 1, 1));
}

using SmallTriMesh = metal::mesh<VertexOut, void, 3, 1, metal::topology::triangle>;

// Mesh shader for amplification test
[[mesh]]
void AmpMSmain(uint gid [[threadgroup_position_in_grid]],
               const object_data Payload& payload [[payload]],
               SmallTriMesh output)
{
    output.set_primitive_count(1);

    uint meshletID = payload.baseID + payload.subIDs[gid];

    const float3 colors[4] = {float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                              float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 1.0)};

    float2 center;
    center.x = (float((meshletID % 9) + 1) / 10.0) * 2.0 - 1.0;
    center.y = (float((meshletID / 9) + 1) / 10.0) * 2.0 - 1.0;

    VertexOut v;
    v.color = colors[meshletID & 3];

    v.position = float4(center.x, center.y + 0.09, 0.0, 1.0);
    output.set_vertex(0, v);

    v.position = float4(center.x - 0.09, center.y - 0.09, 0.0, 1.0);
    output.set_vertex(1, v);

    v.position = float4(center.x + 0.09, center.y - 0.09, 0.0, 1.0);
    output.set_vertex(2, v);

    output.set_index(0, 2);
    output.set_index(1, 1);
    output.set_index(2, 0);
}

struct FSOut
{
    float4 color [[color(0)]];
};

fragment FSOut AmpPSmain(VertexOut in [[stage_in]])
{
    FSOut out;
    out.color = float4(in.color, 1.0);
    return out;
}
)"
};
// clang-format on

} // namespace MSL

} // namespace
