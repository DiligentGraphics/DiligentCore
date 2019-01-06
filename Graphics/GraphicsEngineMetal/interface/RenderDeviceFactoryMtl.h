/*     Copyright 2015-2019 Egor Yusov
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
/// Declaration of functions that initialize Direct3D11-based engine implementation

#include <sstream>

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"
#include "EngineMtlAttribs.h"

// https://gcc.gnu.org/wiki/Visibility
#define API_QUALIFIER //__attribute__((visibility("default")))

namespace Diligent
{

class IEngineFactoryMtl
{
public:
    virtual void CreateDeviceAndContextsMtl( const EngineMtlAttribs& Attribs, 
                                             IRenderDevice**         ppDevice, 
                                             IDeviceContext**        ppContexts,
                                             Uint32                  NumDeferredContexts ) = 0;

   virtual void CreateSwapChainMtl( IRenderDevice*            pDevice, 
                                    IDeviceContext*           pImmediateContext, 
                                    const SwapChainDesc&      SCDesc, 
                                    void*                     pView, 
                                    ISwapChain**              ppSwapChain ) = 0;

   virtual void AttachToMtlDevice(void*                   pMtlNativeDevice, 
                                  const EngineMtlAttribs& EngineAttribs, 
                                  IRenderDevice**         ppDevice, 
                                  IDeviceContext**        ppContexts,
                                  Uint32                  NumDeferredContexts) = 0;
};


API_QUALIFIER IEngineFactoryMtl* GetEngineFactoryMtl();

}
