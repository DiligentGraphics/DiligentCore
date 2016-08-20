/*     Copyright 2015-2016 Egor Yusov
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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <vector>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

// Must be defined to use static version of glew
#ifdef PLATFORM_WIN32
    #define GLEW_STATIC
    #include "glew.h"
    // Glew includes <windows.h>
    #define NOMINMAX
    #include "wglew.h"
    #include <GL/GL.h>
#endif


#ifdef ANDROID
#ifndef USE_GL3_STUB
    #define USE_GL3_STUB 0
#endif
    #if USE_GL3_STUB
        #include "gl3stub.h"
        #include <GLES2/gl2platform.h>
    #else
        #include <GLES3/gl3.h>
        #include <GLES3/gl3platform.h>
    #endif
#endif

#include "Errors.h"

#ifdef ANDROID
    // GLStubs must be included after GLFeatures!
    #include "GLStubs.h"
#endif

#include "PlatformDefinitions.h"
#include "RefCntAutoPtr.h"
#include "DebugUtilities.h"
#include "GLObjectWrapper.h"
#include "DebugUtilities.h"
#include "ValidatedCast.h"
#include "RenderDevice.h"
#include "BaseInterfacesGL.h"

using namespace Diligent;
using namespace std;

#define CHECK_GL_ERROR(...)\
{                                       \
    auto err = glGetError();            \
    if( err != GL_NO_ERROR )            \
    {                                   \
        LogError<false>(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__, "\nGL Error Code: ", err); \
        UNEXPECTED("Error");            \
    }                                   \
}

#define CHECK_GL_ERROR_AND_THROW(...)\
{                                       \
    auto err = glGetError();            \
    if( err != GL_NO_ERROR )            \
        LogError<true>(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__, "\nGL Error Code: ", err); \
}
