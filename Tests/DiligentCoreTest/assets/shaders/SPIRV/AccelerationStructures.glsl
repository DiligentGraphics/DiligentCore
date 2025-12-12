// Test for acceleration structures
// GLSL ray gen shader for testing acceleration structure resource reflection
#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT g_AccelStruct;

layout(location = 0) rayPayloadEXT vec4 payload;

void main()
{
    // Use traceRayEXT to ensure g_AccelStruct is actually used and not optimized away
    // This is the proper way to use acceleration structures in ray gen shaders
    const vec2 uv        = vec2(gl_LaunchIDEXT.xy + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    const vec3 origin    = vec3(uv.x, 1.0 - uv.y, -1.0);
    const vec3 direction = vec3(0.0, 0.0, 1.0);

    payload = vec4(0.0);
    traceRayEXT(g_AccelStruct,                  // acceleration structure (first parameter)
                gl_RayFlagsNoneEXT,             // ray flags
                0xFF,                           // cullMask
                0,                              // sbtRecordOffset
                1,                              // sbtRecordStride
                0,                              // missIndex
                origin,                         // ray origin
                0.01,                           // ray min range
                direction,                      // ray direction
                10.0,                           // ray max range
                0);                             // payload location
}
