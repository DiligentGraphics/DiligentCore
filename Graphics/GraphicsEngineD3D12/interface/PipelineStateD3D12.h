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
/// Definition of the Diligent::IPipeplineStateD3D12 interface

#include "../../GraphicsEngine/interface/PipelineState.h"

namespace Diligent
{

// {33C9BE4B-6F23-4F83-A665-5AC1836DF35A}
static constexpr INTERFACE_ID IID_PipelineStateD3D12 = 
{ 0x33c9be4b, 0x6f23, 0x4f83, { 0xa6, 0x65, 0x5a, 0xc1, 0x83, 0x6d, 0xf3, 0x5a } };


/// Interface to the blend state object implemented in D3D12
class IPipelineStateD3D12 : public IPipelineState
{
public:

    /// Returns ID3D12PipelineState interface of the internal D3D12 pipeline state object object.
    
    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D12PipelineState* GetD3D12PipelineState()const = 0;

    /// Returns a pointer to the root signature object associated with this pipeline state.
    
    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D12RootSignature* GetD3D12RootSignature()const = 0;
};

}
