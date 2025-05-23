/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "pch.h"

#include "GLContextWindows.hpp"
#include "GraphicsTypes.h"
#include "GLTypeConversions.hpp"
#include "GraphicsAccessories.hpp"

#if DILIGENT_USE_OPENXR
#    include "OpenXR_GLHelpers.hpp"
#endif

namespace Diligent
{

GLContext::GLContext(const EngineGLCreateInfo& InitAttribs,
                     RENDER_DEVICE_TYPE&       DevType,
                     Version&                  APIVersion,
                     const SwapChainDesc*      pSCDesc) :
    m_Context{0},
    m_WindowHandleToDeviceContext{0}
{
#if DILIGENT_USE_OPENXR
    Version OpenXRRequiredGLVersion;
    if (InitAttribs.Window.hWnd != nullptr)
    {
        // Check OpenXR requirements when not attaching to an existing context
        OpenXRRequiredGLVersion = GetOpenXRRequiredGLVersion(InitAttribs.pXRAttribs);
    }
#endif

    int MajorVersion = 0, MinorVersion = 0;
    if (InitAttribs.Window.hWnd != nullptr)
    {
        HWND hWnd = reinterpret_cast<HWND>(InitAttribs.Window.hWnd);

        // See http://www.opengl.org/wiki/Tutorial:_OpenGL_3.1_The_First_Triangle_(C%2B%2B/Win)
        //     http://www.opengl.org/wiki/Creating_an_OpenGL_Context_(WGL)
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion   = 1;
        pfd.dwFlags    = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
        pfd.iPixelType = PFD_TYPE_RGBA;
        if (pSCDesc != nullptr)
        {
            TEXTURE_FORMAT ColorFmt = pSCDesc->ColorBufferFormat;
            if (ColorFmt == TEX_FORMAT_RGBA8_UNORM || ColorFmt == TEX_FORMAT_RGBA8_UNORM_SRGB ||
                ColorFmt == TEX_FORMAT_BGRA8_UNORM || ColorFmt == TEX_FORMAT_BGRA8_UNORM_SRGB)
            {
                pfd.cColorBits = 32;
            }
            else
            {
                LOG_WARNING_MESSAGE("Unsupported color buffer format ", GetTextureFormatAttribs(ColorFmt).Name,
                                    ". OpenGL only supports 32-bit UNORM color buffer formats.");
                pfd.cColorBits = 32;
            }

            TEXTURE_FORMAT DepthFmt = pSCDesc->DepthBufferFormat;
            switch (DepthFmt)
            {
                case TEX_FORMAT_UNKNOWN:
                    pfd.cDepthBits   = 0;
                    pfd.cStencilBits = 0;
                    break;

                case TEX_FORMAT_D32_FLOAT_S8X24_UINT:
                    pfd.cDepthBits   = 32;
                    pfd.cStencilBits = 8;
                    break;

                case TEX_FORMAT_D32_FLOAT:
                    pfd.cDepthBits = 32;
                    break;

                case TEX_FORMAT_D24_UNORM_S8_UINT:
                    pfd.cDepthBits   = 24;
                    pfd.cStencilBits = 8;
                    break;

                case TEX_FORMAT_D16_UNORM:
                    pfd.cDepthBits = 16;
                    break;

                default:
                    LOG_ERROR_MESSAGE("Unsupported depth buffer format ", GetTextureFormatAttribs(DepthFmt).Name);
                    pfd.cDepthBits = 32;
            }
        }
        else
        {
            pfd.cColorBits = 32;
            pfd.cDepthBits = 32;
        }
        pfd.iLayerType = PFD_MAIN_PLANE;

        m_WindowHandleToDeviceContext = GetDC(hWnd);
        int nPixelFormat              = ChoosePixelFormat(m_WindowHandleToDeviceContext, &pfd);

        if (nPixelFormat == 0)
            LOG_ERROR_AND_THROW("Invalid Pixel Format");

        // NB: An application can only set the pixel format of a window one time.
        //     Once a window's pixel format is set, it cannot be changed.
        //     https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat
        BOOL bResult = SetPixelFormat(m_WindowHandleToDeviceContext, nPixelFormat, &pfd);
        if (!bResult)
            LOG_ERROR_AND_THROW("Failed to set Pixel Format");

        // Create standard OpenGL (2.1) rendering context which will be used only temporarily,
        HGLRC tempContext = wglCreateContext(m_WindowHandleToDeviceContext);
        // and make it current
        wglMakeCurrent(m_WindowHandleToDeviceContext, tempContext);

        // Initialize GLEW
        GLenum err = glewInit();
        if (GLEW_OK != err)
            LOG_ERROR_AND_THROW("Failed to initialize GLEW");

        if (wglewIsSupported("WGL_ARB_create_context") == 1)
        {
            std::pair<int, int> gl_versions[] = {{4, 4}, {4, 3}, {4, 2}};
            for (size_t i = 0; i < _countof(gl_versions) && m_Context == NULL; ++i)
            {
                // Setup attributes for a new OpenGL rendering context
                const auto& version = gl_versions[i];
                MajorVersion        = version.first;
                MinorVersion        = version.second;

                int ContextFlags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
                if (InitAttribs.EnableValidation)
                {
                    ContextFlags |= WGL_CONTEXT_DEBUG_BIT_ARB;
                }

                int attribs[] =
                    {
                        WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
                        WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
                        WGL_CONTEXT_FLAGS_ARB, ContextFlags,
                        GL_CONTEXT_PROFILE_MASK, GL_CONTEXT_CORE_PROFILE_BIT,
                        0, 0 //
                    };

                // Create new rendering context
                // In order to create new OpenGL rendering context we have to call function wglCreateContextAttribsARB(),
                // which is an OpenGL function and requires OpenGL to be active when it is called.
                // The only way is to create an old context, activate it, and while it is active create a new one.
                // Very inconsistent, but we have to live with it!
                m_Context = wglCreateContextAttribsARB(m_WindowHandleToDeviceContext, 0, attribs);
            }

            if (m_Context == NULL)
            {
                LOG_ERROR_AND_THROW("Failed to initialize OpenGL context.");
            }

            // Delete tempContext
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(tempContext);
            // Make new context current
            wglMakeCurrent(m_WindowHandleToDeviceContext, m_Context);
            wglSwapIntervalEXT(0);
        }
        else
        { //It's not possible to make a GL 4.x context. Use the old style context (GL 2.1 and before)
            m_Context = tempContext;
        }
    }
    else
    {
        HGLRC CurrentCtx = wglGetCurrentContext();
        if (CurrentCtx == 0)
        {
            LOG_ERROR_AND_THROW("No current GL context found! Provide non-null handle to a native Window to create a GL context");
        }

        // Initialize GLEW
        GLenum err = glewInit();
        if (GLEW_OK != err)
            LOG_ERROR_AND_THROW("Failed to initialize GLEW");
    }

    //Checking GL version
    const GLubyte* GLVersionString = glGetString(GL_VERSION);

    //Or better yet, use the GL3 way to get the version number
    glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);
    LOG_INFO_MESSAGE(InitAttribs.Window.hWnd != nullptr ? "Initialized OpenGL " : "Attached to OpenGL ", MajorVersion, '.', MinorVersion, " context (", GLVersionString, ')');

    DevType    = RENDER_DEVICE_TYPE_GL;
    APIVersion = Version{static_cast<Uint32>(MajorVersion), static_cast<Uint32>(MinorVersion)};
    VERIFY(static_cast<int>(APIVersion.Major) == MajorVersion && static_cast<int>(APIVersion.Minor) == MinorVersion,
           "Not enough bits to store version number");

#if DILIGENT_USE_OPENXR
    if (InitAttribs.Window.hWnd != nullptr && OpenXRRequiredGLVersion > APIVersion)
    {
        LOG_ERROR("OpenGL version ", APIVersion.Major, '.', APIVersion.Minor, " does not meet minimum required version for OpenXR: ",
                  OpenXRRequiredGLVersion.Major, '.', OpenXRRequiredGLVersion.Minor);
    }
#endif
}

GLContext::~GLContext()
{
    // Do not destroy context if it was created by the app.
    if (m_Context)
    {
        wglMakeCurrent(m_WindowHandleToDeviceContext, 0);
        wglDeleteContext(m_Context);
    }
}

void GLContext::SwapBuffers(int SwapInterval)
{
    if (m_WindowHandleToDeviceContext)
    {
#if WGL_EXT_swap_control
        if (wglSwapIntervalEXT != nullptr)
        {
            wglSwapIntervalEXT(SwapInterval);
        }
#endif

        ::SwapBuffers(m_WindowHandleToDeviceContext);
    }
    else
    {
        LOG_ERROR("Swap buffer failed because window handle to device context is not initialized");
    }
}

GLContext::NativeGLContextType GLContext::GetCurrentNativeGLContext()
{
    return wglGetCurrentContext();
}

} // namespace Diligent
