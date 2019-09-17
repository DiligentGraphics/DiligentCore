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

#include "PlatformDefinitions.h"

#if PLATFORM_WIN32
    #include "../Win32/interface/Win32Debug.h"
    typedef WindowsDebug PlatformDebug;

#elif PLATFORM_UNIVERSAL_WINDOWS
    #include "../UWP/interface/UWPDebug.h"
    typedef WindowsStoreDebug PlatformDebug;

#elif PLATFORM_ANDROID
    #include "../Android/interface/AndroidDebug.h"
    typedef AndroidDebug PlatformDebug;

#elif PLATFORM_LINUX
    #include "../Linux/interface/LinuxDebug.h"
    typedef LinuxDebug PlatformDebug;

#elif PLATFORM_MACOS || PLATFORM_IOS
    #include "../Apple/interface/AppleDebug.h"
    typedef AppleDebug PlatformDebug;

#else
    #error Unknown platform. Please define one of the following macros as 1:  PLATFORM_WIN32, PLATFORM_UNIVERSAL_WINDOWS, PLATFORM_ANDROID, PLATFORM_LINUX, PLATFORM_MACOS, PLATFORM_IOS.
#endif
