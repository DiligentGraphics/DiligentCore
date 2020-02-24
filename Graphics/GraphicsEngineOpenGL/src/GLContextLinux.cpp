/*     Copyright 2015-2018 Egor Yusov
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

#include "pch.h"

#include "GLContextLinux.hpp"
#include "DeviceCaps.h"
#include "GLTypeConversions.hpp"

namespace Diligent
{

void openglCallbackFunction(GLenum        source,
                            GLenum        type,
                            GLuint        id,
                            GLenum        severity,
                            GLsizei       length,
                            const GLchar* message,
                            const void*   userParam)
{
    std::stringstream MessageSS;

    MessageSS << "OpenGL debug message (";
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
            MessageSS << "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            MessageSS << "DEPRECATED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            MessageSS << "UNDEFINED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            MessageSS << "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            MessageSS << "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            MessageSS << "OTHER";
            break;
    }

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_LOW:
            MessageSS << ", low severity";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            MessageSS << ", medium severity";
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            MessageSS << ", HIGH severity";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            MessageSS << ", notification";
            break;
    }

    MessageSS << ")" << std::endl
              << message << std::endl;

    LOG_INFO_MESSAGE(MessageSS.str().c_str());
}

GLContext::GLContext(const EngineGLCreateInfo& InitAttribs, DeviceCaps& deviceCaps, const struct SwapChainDesc* /*pSCDesc*/) :
    m_Context(0),
    m_WindowId(InitAttribs.Window.WindowId),
    m_pDisplay(InitAttribs.Window.pDisplay)
{
    auto CurrentCtx = glXGetCurrentContext();
    if (CurrentCtx == 0)
    {
        LOG_ERROR_AND_THROW("No current GL context found!");
    }

    // Initialize GLEW
    GLenum err = glewInit();
    if (GLEW_OK != err)
        LOG_ERROR_AND_THROW("Failed to initialize GLEW");

    if (InitAttribs.Window.WindowId != 0 && InitAttribs.Window.pDisplay != nullptr)
    {
        //glXSwapIntervalEXT(0);

        if (glDebugMessageCallback)
        {
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(openglCallbackFunction, nullptr);
            GLuint unusedIds = 0;
            glDebugMessageControl(GL_DONT_CARE,
                                  GL_DONT_CARE,
                                  GL_DONT_CARE,
                                  0,
                                  &unusedIds,
                                  true);
        }
    }

    //Checking GL version
    const GLubyte* GLVersionString = glGetString(GL_VERSION);
    const GLubyte* GLRenderer      = glGetString(GL_RENDERER);

    Int32 MajorVersion = 0, MinorVersion = 0;
    //Or better yet, use the GL3 way to get the version number
    glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);
    LOG_INFO_MESSAGE(InitAttribs.Window.WindowId != 0 ? "Initialized OpenGL " : "Attached to OpenGL ", MajorVersion, '.', MinorVersion, " context (", GLVersionString, ", ", GLRenderer, ')');

    // Under the standard filtering rules for cubemaps, filtering does not work across faces of the cubemap.
    // This results in a seam across the faces of a cubemap. This was a hardware limitation in the past, but
    // modern hardware is capable of interpolating across a cube face boundary.
    // GL_TEXTURE_CUBE_MAP_SEAMLESS is not defined in OpenGLES
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    if (glGetError() != GL_NO_ERROR)
        LOG_ERROR_MESSAGE("Failed to enable seamless cubemap filtering");

    // When GL_FRAMEBUFFER_SRGB is enabled, and if the destination image is in the sRGB colorspace
    // then OpenGL will assume the shader's output is in the linear RGB colorspace. It will therefore
    // convert the output from linear RGB to sRGB.
    // Any writes to images that are not in the sRGB format should not be affected.
    // Thus this setting should be just set once and left that way
    glEnable(GL_FRAMEBUFFER_SRGB);
    if (glGetError() != GL_NO_ERROR)
        LOG_ERROR_MESSAGE("Failed to enable SRGB framebuffers");

    deviceCaps.DevType      = RENDER_DEVICE_TYPE_GL;
    deviceCaps.MajorVersion = MajorVersion;
    deviceCaps.MinorVersion = MinorVersion;
}

GLContext::~GLContext()
{
}

void GLContext::SwapBuffers(int SwapInterval)
{
    if (m_WindowId != 0 && m_pDisplay != nullptr)
    {
        auto wnd     = static_cast<Window>(m_WindowId);
        auto display = reinterpret_cast<Display*>(m_pDisplay);
#if GLX_EXT_swap_control
        if (glXSwapIntervalEXT != nullptr)
        {
            glXSwapIntervalEXT(display, wnd, SwapInterval);
        }
#endif
        glXSwapBuffers(display, wnd);
    }
    else
    {
        LOG_ERROR("Swap buffer failed because window and/or display handle is not initialized");
    }
}

GLContext::NativeGLContextType GLContext::GetCurrentNativeGLContext()
{
    return glXGetCurrentContext();
}

} // namespace Diligent
