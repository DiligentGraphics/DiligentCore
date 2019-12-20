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

static const char* CSSource = R"(
#version 430 core

layout(rgba8, binding = 0) uniform writeonly image2D g_tex2DUAV;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
 
void main()
{
	ivec2 Dim = imageSize(g_tex2DUAV);
	if (gl_GlobalInvocationID.x >= uint(Dim.x) || gl_GlobalInvocationID.y >= uint(Dim.y))
		return;

	imageStore(g_tex2DUAV, ivec2(gl_GlobalInvocationID.xy), vec4( vec2(gl_GlobalInvocationID.xy % 256u) / 256.0, 0.0, 1.0) );
}
)";

void ComputeShaderReferenceGL(ISwapChain* pSwapChain)
{
    auto* pEnv                = TestingEnvironmentGL::GetInstance();
    auto* pContext            = pEnv->GetDeviceContext();
    auto* pTestingSwapChainGL = ValidatedCast<TestingSwapChainGL>(pSwapChain);

    const auto& SCDesc = pTestingSwapChainGL->GetDesc();

    GLuint glCS;

    glCS = pEnv->CompileGLShader(CSSource, GL_COMPUTE_SHADER);
    ASSERT_NE(glCS, 0u);

    auto glProg = pEnv->LinkProgram(&glCS, 1);
    ASSERT_NE(glProg, 0u);

    glUseProgram(glProg);

    GLenum glFormat = 0;
    switch (SCDesc.ColorBufferFormat)
    {
        case TEX_FORMAT_RGBA8_UNORM:
            glFormat = GL_RGBA8;
            break;

        default:
            UNEXPECTED("Unexpected texture format");
    }
    glBindImageTexture(0, pTestingSwapChainGL->GetRenderTargetGLHandle(), 0, 0, 0, GL_WRITE_ONLY, glFormat);
    glDispatchCompute((SCDesc.Width + 15) / 16, (SCDesc.Height + 15) / 16, 1);
    VERIFY_EXPR(glGetError() == GL_NO_ERROR);

    glUseProgram(0);

    if (glCS != 0)
        glDeleteShader(glCS);

    if (glProg != 0)
        glDeleteProgram(glProg);


    // Make sure Diligent Engine will reset all GL states
    pContext->InvalidateState();
}

} // namespace Testing

} // namespace Diligent
