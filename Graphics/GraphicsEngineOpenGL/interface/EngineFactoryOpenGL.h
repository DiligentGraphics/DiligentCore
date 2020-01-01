/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#pragma once

/// \file
/// Declaration of functions that create OpenGL-based engine implementation

#include <sstream>

#include "../../GraphicsEngine/interface/EngineFactory.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#include "../../HLSL2GLSLConverterLib/interface/HLSL2GLSLConverter.h"


#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS || (PLATFORM_WIN32 && !defined(_MSC_VER))
// https://gcc.gnu.org/wiki/Visibility
#    define API_QUALIFIER __attribute__((visibility("default")))
#elif PLATFORM_WIN32
#    define API_QUALIFIER
#else
#    error Unsupported platform
#endif

#if ENGINE_DLL && PLATFORM_WIN32 && defined(_MSC_VER)
#    include "../../GraphicsEngine/interface/LoadEngineDll.h"
#    define EXPLICITLY_LOAD_ENGINE_GL_DLL 1
#endif

namespace Diligent
{

// {9BAAC767-02CC-4FFA-9E4B-E1340F572C49}
static const INTERFACE_ID IID_EngineFactoryOpenGL =
    {0x9baac767, 0x2cc, 0x4ffa, {0x9e, 0x4b, 0xe1, 0x34, 0xf, 0x57, 0x2c, 0x49}};

class IEngineFactoryOpenGL : public IEngineFactory
{
public:
    virtual void CreateDeviceAndSwapChainGL(const EngineGLCreateInfo& EngineCI,
                                            IRenderDevice**           ppDevice,
                                            IDeviceContext**          ppImmediateContext,
                                            const SwapChainDesc&      SCDesc,
                                            ISwapChain**              ppSwapChain)        = 0;
    virtual void CreateHLSL2GLSLConverter(IHLSL2GLSLConverter** ppConverter) = 0;

    virtual void AttachToActiveGLContext(const EngineGLCreateInfo& EngineCI,
                                         IRenderDevice**           ppDevice,
                                         IDeviceContext**          ppImmediateContext) = 0;
};


#if EXPLICITLY_LOAD_ENGINE_GL_DLL

using GetEngineFactoryOpenGLType = IEngineFactoryOpenGL* (*)();

static bool LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGLType& GetFactoryFunc)
{
    auto ProcAddress = LoadEngineDll("GraphicsEngineOpenGL", "GetEngineFactoryOpenGL");
    GetFactoryFunc   = reinterpret_cast<GetEngineFactoryOpenGLType>(ProcAddress);
    return GetFactoryFunc != nullptr;
}

#else

// Do not forget to call System.loadLibrary("GraphicsEngineOpenGL") in Java on Android!
API_QUALIFIER
IEngineFactoryOpenGL* GetEngineFactoryOpenGL();

#endif

} // namespace Diligent
