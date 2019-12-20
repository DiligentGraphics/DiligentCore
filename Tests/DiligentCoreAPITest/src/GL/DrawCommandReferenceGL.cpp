/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "GL/TestingEnvironmentGL.h"
#include "GL/TestingSwapChainGL.h"

namespace Diligent
{

namespace Testing
{

static const char* VSSource = R"(
#version 420 core

#ifndef GL_ES
out gl_PerVertex
{
	vec4 gl_Position;
};
#endif

layout(location = 0) out vec3 out_Color;

void main()
{
    vec4 Pos[6];
    Pos[0] = vec4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = vec4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = vec4( 0.0, -0.5, 0.0, 1.0);

    Pos[3] = vec4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = vec4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = vec4(+1.0, -0.5, 0.0, 1.0);

    vec3 Col[6];
    Col[0] = vec3(1.0, 0.0, 0.0);
    Col[1] = vec3(0.0, 1.0, 0.0);
    Col[2] = vec3(0.0, 0.0, 1.0);

    Col[3] = vec3(1.0, 0.0, 0.0);
    Col[4] = vec3(0.0, 1.0, 0.0);
    Col[5] = vec3(0.0, 0.0, 1.0);
    
    gl_Position = Pos[gl_VertexID];
    out_Color = Col[gl_VertexID];
}
)";

static const char* PSSource = R"(
#version 420 core

layout(location = 0) in  vec3 in_Color;
layout(location = 0) out vec4 out_Color;

void main()
{
    out_Color = vec4(in_Color, 1.0);
}
)";

void RenderDrawCommandReferenceGL(ISwapChain* pSwapChain)
{
    auto* pEnv                = TestingEnvironmentGL::GetInstance();
    auto* pContext            = pEnv->GetDeviceContext();
    auto* pTestingSwapChainGL = ValidatedCast<TestingSwapChainGL>(pSwapChain);

    const auto& SCDesc = pTestingSwapChainGL->GetDesc();

    GLuint glShaders[2] = {};

    glShaders[0] = pEnv->CompileGLShader(VSSource, GL_VERTEX_SHADER);
    ASSERT_NE(glShaders[0], 0u);
    glShaders[1] = pEnv->CompileGLShader(PSSource, GL_FRAGMENT_SHADER);
    ASSERT_NE(glShaders[1], 0u);
    auto glProg = pEnv->LinkProgram(glShaders, 2);
    ASSERT_NE(glProg, 0u);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    if (glPolygonMode != nullptr)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    pTestingSwapChainGL->BindFramebuffer();
    glViewport(0, 0, SCDesc.Width, SCDesc.Height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(glProg);
    glBindVertexArray(pEnv->GetDummyVAO());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);

    for (int i = 0; i < _countof(glShaders); ++i)
    {
        if (glShaders[i] != 0)
            glDeleteShader(glShaders[i]);
    }
    if (glProg != 0)
        glDeleteProgram(glProg);

    // Make sure Diligent Engine will reset all GL states
    pContext->InvalidateState();
}

} // namespace Testing

} // namespace Diligent
