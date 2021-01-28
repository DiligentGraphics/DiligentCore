uniform sampler2D g_tex2D_Static;
uniform sampler2D g_tex2D_StaticArr[2];
uniform sampler2D g_tex2D_Mut;
uniform sampler2D g_tex2D_MutArr[2];
uniform sampler2D g_tex2D_Dyn;
uniform sampler2D g_tex2D_DynArr[2];

layout(location = 0) out vec4 f4Color;

#ifndef GL_ES
out gl_PerVertex
{
    vec4 gl_Position;
};
#endif

void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
    f4Color = vec4(0.0, 0.0, 0.0, 0.0);
    f4Color += textureLod(g_tex2D_Static, vec2(0.5,0.5), 0.0);
    f4Color += textureLod(g_tex2D_StaticArr[0], vec2(0.5,0.5), 0.0) + textureLod(g_tex2D_StaticArr[1], vec2(0.5,0.5), 0.0);
    f4Color += textureLod(g_tex2D_Mut, vec2(0.5,0.5), 0.0);
    f4Color += textureLod(g_tex2D_MutArr[0], vec2(0.5,0.5), 0.0) + textureLod(g_tex2D_MutArr[1], vec2(0.5,0.5), 0.0);
    f4Color += textureLod(g_tex2D_Dyn, vec2(0.5,0.5), 0.0);
    f4Color += textureLod(g_tex2D_DynArr[0], vec2(0.5,0.5), 0.0) + textureLod(g_tex2D_DynArr[1], vec2(0.5,0.5), 0.0);
}
