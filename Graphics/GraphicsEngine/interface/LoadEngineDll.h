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

/// \file
/// Helper function to load engine DLL on Windows

#include <sstream>

#if PLATFORM_UNIVERSAL_WINDOWS
#    include "../../../Common/interface/StringTools.h"
#endif

#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <Windows.h>

namespace Diligent
{

inline FARPROC LoadEngineDll(const char* EngineName, const char* GetFactoryFuncName)
{
    std::string LibName = EngineName;

#if _WIN64
    LibName += "_64";
#else
    LibName += "_32";
#endif

#ifdef _DEBUG
    LibName += "d";
#else
    LibName += "r";
#endif

    LibName += ".dll";

#if PLATFORM_WIN32
    auto hModule = LoadLibraryA(LibName.c_str());
#elif PLATFORM_UNIVERSAL_WINDOWS
    auto hModule = LoadPackagedLibrary(WidenString(LibName).c_str(), 0);
#else
#    error Unexpected platform
#endif

    if (hModule == NULL)
    {
        std::stringstream ss;
        ss << "Failed to load " << LibName << " library.\n";
        OutputDebugStringA(ss.str().c_str());
        return NULL;
    }

    auto GetFactoryFunc = GetProcAddress(hModule, GetFactoryFuncName);
    if (GetFactoryFunc == NULL)
    {
        std::stringstream ss;
        ss << "Failed to load " << GetFactoryFuncName << " function from " << LibName << " library.\n";
        OutputDebugStringA(ss.str().c_str());
        FreeLibrary(hModule);
    }

    return GetFactoryFunc;
}

} // namespace Diligent
