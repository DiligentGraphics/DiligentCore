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
/// Definition of the Diligent::ITextureViewD3D11 interface

#include "../../GraphicsEngine/interface/TextureView.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

#if DILIGENT_CPP_INTERFACE

// {0767EBE4-AD47-4E70-9B65-38C6B9CAC37D}
static constexpr INTERFACE_ID IID_TextureViewD3D11 =
    {0x767ebe4, 0xad47, 0x4e70, {0x9b, 0x65, 0x38, 0xc6, 0xb9, 0xca, 0xc3, 0x7d}};

/// Exposes Direct3D11-specific functionality of a texture view object.
class ITextureViewD3D11 : public ITextureView
{
public:
    /// Returns a pointer to the ID3D11View interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11View* GetD3D11View() = 0;
};


#else

struct ITextureViewD3D11Methods
{
    ID3D11View* (*GetD3D11View)();
};


struct ITextureViewD3D11Vtbl
{
    struct IObjectMethods           Object;
    struct IDeviceObjectMethods     DeviceObject;
    struct ITextureViewMethods      TextureView;
    struct ITextureViewD3D11Methods TextureViewD3D11;
};

struct ITextureViewD3D11
{
    struct ITextureViewD3D11Vtbl* pVtbl;
};

#    define ITextureViewD3D11_GetD3D11View(This) (This)->pVtbl->TextureViewD3D11.GetD3D11View((struct ITextureViewD3D11*)(This))

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
