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

#pragma once

#if defined(ANDROID)
#   ifndef PLATFORM_ANDROID
#       define PLATFORM_ANDROID 1
#   endif
#endif

#if !PLATFORM_WIN32 && !PLATFORM_UNIVERSAL_WINDOWS && !PLATFORM_ANDROID && !PLATFORM_LINUX && !PLATFORM_MACOS && !PLATFORM_IOS
    #error Platform is not defined
#endif

#if PLATFORM_WIN32

#   if PLATFORM_UNIVERSAL_WINDOWS || PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS
#       error Conflicting platform macros
#   endif

#   include "../Win32/include/Win32PlatformDefinitions.h"

#   define OPENGL_SUPPORTED 1
#   define D3D11_SUPPORTED 1
#   define D3D12_SUPPORTED 1

#elif PLATFORM_UNIVERSAL_WINDOWS

#   if PLATFORM_WIN32 || PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS
#       error Conflicting platform macros
#   endif

#   include "../UWP/include/UWPDefinitions.h"

#   define OPENGL_SUPPORTED 0
#   define D3D11_SUPPORTED 1
#   define D3D12_SUPPORTED 1

#elif PLATFORM_ANDROID

#   if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS
#       error Conflicting platform macros
#   endif

#   include "../Android/include/AndroidPlatformDefinitions.h"

#   define OPENGL_SUPPORTED 1
#   define D3D11_SUPPORTED 0
#   define D3D12_SUPPORTED 0

#elif PLATFORM_LINUX

#   if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS || PLATFORM_ANDROID || PLATFORM_MACOS || PLATFORM_IOS
#       error Conflicting platform macros
#   endif

#   include "../Linux/include/LinuxPlatformDefinitions.h"

#   define OPENGL_SUPPORTED 1
#   define D3D11_SUPPORTED 0
#   define D3D12_SUPPORTED 0

#elif PLATFORM_MACOS

#   if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS || PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_IOS
#       error Conflicting platform macros
#   endif

#   include "../Apple/include/ApplePlatformDefinitions.h"

#   define OPENGL_SUPPORTED 1
#   define D3D11_SUPPORTED 0
#   define D3D12_SUPPORTED 0

#elif PLATFORM_IOS

#   if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS || PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS
#       error Conflicting platform macros
#   endif

#   include "../Apple/include/ApplePlatformDefinitions.h"

#   define OPENGL_SUPPORTED 1
#   define D3D11_SUPPORTED 0
#   define D3D12_SUPPORTED 0

#else

#   error Unsupported platform

#endif
