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
/// Definition of the Diligent::IRenderDeviceD3D11 interface

#include "../../GraphicsEngine/interface/RenderDevice.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {05B1CBB8-FCAD-49EE-BADA-7801223EC3FE}
static const struct INTERFACE_ID IID_RenderDeviceD3D11 =
    {0x5b1cbb8, 0xfcad, 0x49ee, {0xba, 0xda, 0x78, 0x1, 0x22, 0x3e, 0xc3, 0xfe}};

#if DILIGENT_CPP_INTERFACE

/// Exposes Direct3D11-specific functionality of a render device.
class IRenderDeviceD3D11 : public IRenderDevice
{
public:
    /// Returns a pointer to the ID3D11Device interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11Device* GetD3D11Device() = 0;

    /// Creates a buffer object from native d3d11 buffer

    /// \param [in]  pd3d11Buffer - Pointer to the native buffer
    /// \param [in]  BuffDesc     - Buffer description. Most of the fields will be
    ///                             populated automatically if left in default values.
    ///                             The only field that must be populated is BufferDesc::Format,
    ///                             when initializing a formatted buffer.
    /// \param [in]  InitialState - Initial buffer state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppBuffer     - Address of the memory location where the pointer to the
    ///                             buffer interface will be stored.
    ///                             The function calls AddRef(), so that the new object will contain
    ///                             one reference.
    virtual void CreateBufferFromD3DResource(ID3D11Buffer*     pd3d11Buffer,
                                             const BufferDesc& BuffDesc,
                                             RESOURCE_STATE    InitialState,
                                             IBuffer**         ppBuffer) = 0;

    /// Creates a texture object from native d3d11 1D texture

    /// \param [in]  pd3d11Texture - pointer to the native 1D texture
    /// \param [in]  InitialState  - Initial texture state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppTexture     - Address of the memory location where the pointer to the
    ///                              texture interface will be stored.
    ///                              The function calls AddRef(), so that the new object will contain
    ///                              one reference.
    virtual void CreateTextureFromD3DResource(ID3D11Texture1D* pd3d11Texture,
                                              RESOURCE_STATE   InitialState,
                                              ITexture**       ppTexture) = 0;

    /// Creates a texture object from native d3d11 2D texture

    /// \param [in]  pd3d11Texture - pointer to the native 2D texture
    /// \param [in]  InitialState  - Initial texture state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppTexture     - Address of the memory location where the pointer to the
    ///                              texture interface will be stored.
    ///                              The function calls AddRef(), so that the new object will contain
    ///                              one reference.
    virtual void CreateTextureFromD3DResource(ID3D11Texture2D* pd3d11Texture,
                                              RESOURCE_STATE   InitialState,
                                              ITexture**       ppTexture) = 0;

    /// Creates a texture object from native d3d11 3D texture

    /// \param [in]  pd3d11Texture - pointer to the native 3D texture
    /// \param [in]  InitialState  - Initial texture state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppTexture     - Address of the memory location where the pointer to the
    ///                              texture interface will be stored.
    ///                              The function calls AddRef(), so that the new object will contain
    ///                              one reference.
    virtual void CreateTextureFromD3DResource(ID3D11Texture3D* pd3d11Texture,
                                              RESOURCE_STATE   InitialState,
                                              ITexture**       ppTexture) = 0;
};

#else



#endif

DILIGENT_END_NAMESPACE // namespace Diligent
