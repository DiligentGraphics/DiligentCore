#version 460 core
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_nonuniform_qualifier : require

uniform texture2D g_Textures[];
uniform sampler g_Sampler;

vec4 CheckValue(vec4 Val, vec4 Expected)
{
    return vec4(Val.x == Expected.x ? 1.0 : 0.0,
                Val.y == Expected.y ? 1.0 : 0.0,
                Val.z == Expected.z ? 1.0 : 0.0,
                Val.w == Expected.w ? 1.0 : 0.0);
}


vec4 VerifyResources(uint index, vec2 coord)
{
    vec4 RefValues[NUM_TEXTURES];
    RefValues[0] = Tex2D_Ref0;
    RefValues[1] = Tex2D_Ref1;
    RefValues[2] = Tex2D_Ref2;
    RefValues[3] = Tex2D_Ref3;
    RefValues[4] = Tex2D_Ref4;
    RefValues[5] = Tex2D_Ref5;
    RefValues[6] = Tex2D_Ref6;
    RefValues[7] = Tex2D_Ref7;

    return CheckValue(textureLod(sampler2D(g_Textures[nonuniformEXT(index)], g_Sampler), coord, 0.0), RefValues[index]);
}

layout(rgba8) writeonly uniform image2D  g_OutImage;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    ivec2 Dim = imageSize(g_OutImage);
    if (gl_GlobalInvocationID.x >= uint(Dim.x) || gl_GlobalInvocationID.y >= uint(Dim.y))
        return;

    vec4 Color = vec4(vec2(gl_GlobalInvocationID.xy % 256u) / 256.0, 0.0, 1.0);
    vec2 uv = vec2(gl_GlobalInvocationID.xy + vec2(0.5,0.5)) / vec2(gl_WorkGroupSize.xy * gl_NumWorkGroups.xy);
    Color *= VerifyResources(gl_LocalInvocationIndex % NUM_TEXTURES, uv);

    imageStore(g_OutImage, ivec2(gl_GlobalInvocationID.xy),  Color);
}
