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

/// \file
/// Definition of the Diligent::IRenderDeviceD3D12 interface

#include "RenderDevice.h"

namespace Diligent
{
// {C7987C98-87FE-4309-AE88-E98F044B00F6}
static constexpr INTERFACE_ID IID_RenderDeviceD3D12 =
{ 0xc7987c98, 0x87fe, 0x4309, { 0xae, 0x88, 0xe9, 0x8f, 0x4, 0x4b, 0x0, 0xf6 } };

/// Interface to the render device object implemented in D3D12
class IRenderDeviceD3D12 : public IRenderDevice
{
public:

    /// Returns ID3D12Device interface of the internal Direct3D12 device object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D12Device* GetD3D12Device() = 0;

    /// Returns the fence value that will be signaled by the GPU command queue next
    virtual Uint64 GetNextFenceValue() = 0;

    /// Checks if the fence value has been signaled by the GPU. True means
    /// that all associated work has been finished
    virtual Bool IsFenceSignaled(Uint64 FenceValue) = 0;

    /// Should be called at the end of the frame when attached to existing D3D12 device
    /// Otherwise the method is automatically called before present
    virtual void FinishFrame() = 0;

    /// Creates a texture object from native d3d12 resource

    /// \param [in] pd3d12Texture - pointer to the native D3D12 texture
    /// \param [out] ppTexture - Address of the memory location where the pointer to the
    ///                          texture interface will be stored. 
    ///                          The function calls AddRef(), so that the new object will contain 
    ///                          one refernce.
    virtual void CreateTextureFromD3DResource(ID3D12Resource *pd3d12Texture, ITexture **ppTexture) = 0;

    /// Creates a buffer object from native d3d12 resoruce

    /// \param [in] pd3d12Buffer - Pointer to the native d3d12 buffer resource
    /// \param [in] BuffDesc - Buffer description. The system can recover buffer size, but
    ///                        the rest of the fields need to be populated by the client 
    ///                        as they cannot be recovered from d3d12 resource description
    /// \param [out] ppBuffer - Address of the memory location where the pointer to the
    ///                         buffer interface will be stored. 
    ///                         The function calls AddRef(), so that the new object will contain 
    ///                         one reference.
    virtual void CreateBufferFromD3DResource(ID3D12Resource *pd3d12Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer) = 0;
};

}
