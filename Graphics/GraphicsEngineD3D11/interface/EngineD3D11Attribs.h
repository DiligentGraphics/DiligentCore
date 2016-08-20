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

#pragma once

/// \file
/// Definition of the Engine D3D11 attribs

#include "BasicTypes.h"

namespace Diligent
{
    /// Debug flags that can be specified when creating Direct3D11-based engine implementation.
    ///
    /// \sa CreateDeviceAndContextsD3D11Type, CreateSwapChainD3D11Type, LoadGraphicsEngineD3D11
    enum class EngineD3D11DebugFlags : Uint32
    {
        /// Before executing draw/dispatch command, verify that
        /// all required shader resources are bound to the device context
        VerifyCommittedShaderResources = 0x01,

        /// Verify that all committed cotext resources are relevant,
        /// i.e. they are consistent with the committed resource cache.
        /// This is very expensive operation and should generally not be 
        /// necessary.
        VerifyCommittedResourceRelevance = 0x02
    };

    /// Attributes of the Direct3D11-based engine implementation
    struct EngineD3D11Attribs : public EngineCreationAttribs
    {
        /// Debug flags. See Diligent::EngineD3D11DebugFlags for a list of allowed values.
        ///
        /// \sa CreateDeviceAndContextsD3D11Type, CreateSwapChainD3D11Type, LoadGraphicsEngineD3D11
        Uint32 DebugFlags;

        EngineD3D11Attribs() :
            DebugFlags(0)
        {
    #ifdef _DEBUG
            DebugFlags = static_cast<Uint32>(EngineD3D11DebugFlags::VerifyCommittedShaderResources);
    #endif
        }
    };
}
