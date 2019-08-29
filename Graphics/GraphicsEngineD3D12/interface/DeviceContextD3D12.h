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
/// Definition of the Diligent::IDeviceContextD3D12 interface

#include "../../GraphicsEngine/interface/DeviceContext.h"

namespace Diligent
{

// {DDE9E3AB-5109-4026-92B7-F5E7EC83E21E}
static constexpr INTERFACE_ID IID_DeviceContextD3D12 =
{ 0xdde9e3ab, 0x5109, 0x4026, { 0x92, 0xb7, 0xf5, 0xe7, 0xec, 0x83, 0xe2, 0x1e } };

/// Interface to the device context object implemented in D3D12
class IDeviceContextD3D12 : public IDeviceContext
{
public:

    /// Transitions internal D3D12 texture object to a specified state

    /// \param [in] pTexture - texture to transition
    /// \param [in] State - D3D12 resource state this texture to transition to
    virtual void TransitionTextureState(ITexture* pTexture, D3D12_RESOURCE_STATES State) = 0;

    /// Transitions internal D3D12 buffer object to a specified state

    /// \param [in] pBuffer - Buffer to transition
    /// \param [in] State - D3D12 resource state this buffer to transition to
    virtual void TransitionBufferState(IBuffer* pBuffer, D3D12_RESOURCE_STATES State) = 0;

    /// Returns a pointer to Direct3D12 graphics command list that is currently being recorded

    /// \return - a pointer to the current command list
    ///
    /// \remarks  Any command on the device context may potentially submit the command list for
    ///           execution into the command queue and make it invalid. An application should 
    ///           never cache the pointer and should instead request the command list every time it 
    ///           needs it.\n
    ///           The engine manages the lifetimes of all command buffers, so an application must
    ///           not call AddRef/Release methods on the returned interface.
    virtual ID3D12GraphicsCommandList* GetD3D12CommandList() = 0;
};

}
