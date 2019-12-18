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

#pragma once

#ifndef GLEW_STATIC
#    define GLEW_STATIC // Must be defined to use static version of glew
#endif
#ifndef GLEW_NO_GLU
#    define GLEW_NO_GLU
#endif

#include "GL/glew.h"

#include "TestingEnvironment.h"

namespace Diligent
{

namespace Testing
{

class TestingEnvironmentGL final : public TestingEnvironment
{
public:
    TestingEnvironmentGL(DeviceType deviceType, ADAPTER_TYPE AdapterType, const SwapChainDesc& SCDesc);
    ~TestingEnvironmentGL();

    static TestingEnvironmentGL* GetInstance() { return ValidatedCast<TestingEnvironmentGL>(TestingEnvironment::GetInstance()); }

    GLuint CompileGLShader(const char* Source, GLenum ShaderType);
    GLuint LinkProgram(GLuint Shaders[], GLuint NumShaders);

    GLuint GetDummyVAO() { return m_DummyVAO; }

private:
    GLuint m_DummyVAO = 0;
};

} // namespace Testing

} // namespace Diligent
