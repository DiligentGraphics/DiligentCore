uniform sampler2D g_tex2D_Static;
uniform sampler2D g_tex2D_StaticArr[2];
uniform sampler2D g_tex2D_Mut;
uniform sampler2D g_tex2D_MutArr[2];
uniform sampler2D g_tex2D_Dyn;
uniform sampler2D g_tex2D_DynArr[2];


vec4 CheckValue(vec4 Val, vec4 Expected)
{
    return vec4(Val.x == Expected.x ? 1.0 : 0.0,
                Val.y == Expected.y ? 1.0 : 0.0,
                Val.z == Expected.z ? 1.0 : 0.0,
                Val.w == Expected.w ? 1.0 : 0.0);
}

vec4 VerifyResources()
{
    vec4 AllCorrect = vec4(1.0, 1.0, 1.0, 1.0);

    AllCorrect *= CheckValue(textureLod(g_tex2D_Static, vec2(0.5, 0.5), 0.0), Tex2D_Static_Ref);
    AllCorrect *= CheckValue(textureLod(g_tex2D_Mut, vec2(0.5, 0.5), 0.0), Tex2D_Mut_Ref);
    AllCorrect *= CheckValue(textureLod(g_tex2D_Dyn, vec2(0.5, 0.5), 0.0), Tex2D_Dyn_Ref);

    AllCorrect *= CheckValue(textureLod(g_tex2D_StaticArr[0], vec2(0.5,0.5), 0.0), Tex2DArr_Static_Ref0);
    AllCorrect *= CheckValue(textureLod(g_tex2D_StaticArr[1], vec2(0.5,0.5), 0.0), Tex2DArr_Static_Ref1);

    AllCorrect *= CheckValue(textureLod(g_tex2D_MutArr[0], vec2(0.5, 0.5), 0.0), Tex2DArr_Mut_Ref0);
    AllCorrect *= CheckValue(textureLod(g_tex2D_MutArr[1], vec2(0.5, 0.5), 0.0), Tex2DArr_Mut_Ref1);

    AllCorrect *= CheckValue(textureLod(g_tex2D_DynArr[0], vec2(0.5, 0.5), 0.0), Tex2DArr_Dyn_Ref0);
    AllCorrect *= CheckValue(textureLod(g_tex2D_DynArr[1], vec2(0.5, 0.5), 0.0), Tex2DArr_Dyn_Ref1);

    return AllCorrect;
}

//To use any built-in input or output in the gl_PerVertex and
//gl_PerFragment blocks in separable program objects, shader code must
//redeclare those blocks prior to use.
//
// Declaring this block causes compilation error on NVidia GLES
#ifndef GL_ES
out gl_PerVertex
{
    vec4 gl_Position;
};
#endif

layout(location = 0) out vec4 out_Color;

void main()
{
    vec4 Pos[6];
    Pos[0] = vec4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = vec4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = vec4(0.0, -0.5, 0.0, 1.0);

    Pos[3] = vec4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = vec4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = vec4(+1.0, -0.5, 0.0, 1.0);

    vec4 Col[3];
    Col[0] = vec4(1.0, 0.0, 0.0, 1.0);
    Col[1] = vec4(0.0, 1.0, 0.0, 1.0);
    Col[2] = vec4(0.0, 0.0, 1.0, 1.0);

#ifdef VULKAN
    gl_Position = Pos[gl_VertexIndex];
    out_Color   = Col[gl_VertexIndex % 3] * VerifyResources();
#else
    gl_Position = Pos[gl_VertexID];
    out_Color   = Col[gl_VertexID % 3] * VerifyResources();
#endif
}
