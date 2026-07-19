#version 450
#extension GL_GOOGLE_include_directive : enable

#include "IncludeNestedParentRelative/Headers/Types.glsl"

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = GetNestedIncludeColor();
}
