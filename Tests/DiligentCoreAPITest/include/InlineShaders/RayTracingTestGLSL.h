/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
const std::string RayTracingTest1_RG{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(set=0, binding=0) uniform accelerationStructureEXT  g_TLAS;
layout(set=0, binding=1, rgba8) uniform image2D  g_ColorBuffer;

layout(location=0) rayPayloadEXT vec4  payload;

void main()
{
    const vec2 uv        = vec2(gl_LaunchIDEXT.xy) / vec2(gl_LaunchSizeEXT.xy - 1);
    const vec3 origin    = vec3(uv.x, 1.0 - uv.y, -1.0);
    const vec3 direction = vec3(0.0, 0.0, 1.0);

    payload = vec4(0.0);
    traceRayEXT(g_TLAS,                  // acceleration structure
                gl_RayFlagsNoneEXT,      // rayFlags
                0xFF,                    // cullMask
                0,                       // sbtRecordOffset
                0,                       // sbtRecordStride
                0,                       // missIndex
                origin,                  // ray origin
                0.01,                    // ray min range
                direction,               // ray direction
                10.0,                    // ray max range
                0);                      // payload location

    imageStore(g_ColorBuffer, ivec2(gl_LaunchIDEXT), payload);
}
)glsl"
};

const std::string RayTracingTest1_RM{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;

void main()
{
    payload = vec4(1.0, 0.0, 0.0, 1.0);
}
)glsl"
};

const std::string RayTracingTest1_RCH{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;
hitAttributeEXT vec2  hitAttribs;

void main()
{
    const vec3 barycentrics = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
    payload = vec4(barycentrics, 1.0);
}
)glsl"
};
// clang-format on


// clang-format off
const std::string RayTracingTest2_RG{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(set=0, binding=0) uniform accelerationStructureEXT  g_TLAS;
layout(set=0, binding=1, rgba8) uniform image2D  g_ColorBuffer;

layout(location=0) rayPayloadEXT vec4  payload;

void main()
{
    const vec2 uv        = vec2(gl_LaunchIDEXT.xy) / vec2(gl_LaunchSizeEXT.xy - 1);
    const vec3 origin    = vec3(uv.x, 1.0 - uv.y, -1.0);
    const vec3 direction = vec3(0.0, 0.0, 1.0);

    payload = vec4(0.0);
    traceRayEXT(g_TLAS,                  // acceleration structure
                gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF,                    // cullMask
                0,                       // sbtRecordOffset
                0,                       // sbtRecordStride
                0,                       // missIndex
                origin,                  // ray origin
                0.01,                    // ray min range
                direction,               // ray direction
                10.0,                    // ray max range
                0);                      // payload location

    imageStore(g_ColorBuffer, ivec2(gl_LaunchIDEXT), payload);
}
)glsl"
};

const std::string RayTracingTest2_RM{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;

void main()
{
    payload = vec4(0.0, 0.0, 0.0, 0.0);
}
)glsl"
};

const std::string RayTracingTest2_RCH{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require
hitAttributeEXT vec2  hitAttribs;

layout(location=0) rayPayloadInEXT vec4  payload;

void main()
{
    payload *= 4.0;
}
)glsl"
};

const std::string RayTracingTest2_RAH{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;
hitAttributeEXT vec2  hitAttribs;

void main()
{
    const vec3 barycentrics = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
    if (barycentrics.y > barycentrics.x)
        ignoreIntersectionEXT();
    else
        payload += vec4(barycentrics, 1.0) / 3.0;
}
)glsl"
};
// clang-format on


// clang-format off
const std::string RayTracingTest3_RG{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(set=0, binding=0) uniform accelerationStructureEXT  g_TLAS;
layout(set=0, binding=1, rgba8) uniform image2D  g_ColorBuffer;

layout(location=0) rayPayloadEXT vec4  payload;

void main()
{
    const vec2 uv        = vec2(gl_LaunchIDEXT.xy) / vec2(gl_LaunchSizeEXT.xy - 1);
    const vec3 origin    = vec3(uv.x, 1.0 - uv.y, 0.0);
    const vec3 direction = vec3(0.0, 0.0, 1.0);

    payload = vec4(0.0);
    traceRayEXT(g_TLAS,                  // acceleration structure
                gl_RayFlagsNoneEXT,      // rayFlags
                0xFF,                    // cullMask
                0,                       // sbtRecordOffset
                0,                       // sbtRecordStride
                0,                       // missIndex
                origin,                  // ray origin
                0.01,                    // ray min range
                direction,               // ray direction
                4.0,                     // ray max range
                0);                      // payload location

    imageStore(g_ColorBuffer, ivec2(gl_LaunchIDEXT), payload);
}
)glsl"
};

const std::string RayTracingTest3_RM{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;

void main()
{
    payload = vec4(0.0, 0.15, 0.0, 1.0);
}
)glsl"
};

const std::string RayTracingTest3_RCH{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location=0) rayPayloadInEXT vec4  payload;
hitAttributeEXT vec3  hitAttribs;

void main()
{
    payload = vec4(hitAttribs.x, gl_HitTEXT / 4.0, float(gl_HitKindEXT) * 0.2, 1.0);
}
)glsl"
};

const std::string RayTracingTest3_RI{
R"glsl(
#version 460
#extension GL_EXT_ray_tracing : require

hitAttributeEXT vec3  out_hitAttribs;

void main()
{
    const float radius = 0.5;
    const vec3  center = vec3(0.25, 0.5, 2.0); // must match with AABB center

    // ray sphere intersection
    vec3  oc = gl_WorldRayOriginEXT - center;
    float a  = dot(gl_WorldRayDirectionEXT, gl_WorldRayDirectionEXT);
    float b  = 2.0 * dot(oc, gl_WorldRayDirectionEXT);
    float c  = dot(oc, oc) - radius * radius;
    float d  = b * b - 4 * a * c;

    if (d >= 0)
    {
        float hitT = (-b - sqrt(d)) / (2.0 * a);
        out_hitAttribs = vec3(0.5);
        reportIntersectionEXT(hitT, 3);
    }
}
)glsl"
};
// clang-format on


} // namespace GLSL

} // namespace
