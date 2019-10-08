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

#include "pch.h"
#include "TextureViewD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"

namespace Diligent
{

TextureViewD3D11Impl::TextureViewD3D11Impl( IReferenceCounters*    pRefCounters,
                                            RenderDeviceD3D11Impl* pDevice, 
                                            const TextureViewDesc& ViewDesc, 
                                            ITexture*              pTexture,
                                            ID3D11View*            pD3D11View,
                                            bool                   bIsDefaultView ) :
    TTextureViewBase
    {
        pRefCounters,
        pDevice,
        ViewDesc,
        pTexture,
        bIsDefaultView
    },
    m_pD3D11View{pD3D11View}
{
    if (*m_Desc.Name != 0)
    {
        auto hr = m_pD3D11View->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(m_Desc.Name)), m_Desc.Name);
        DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to set texture view name");
    }
}

IMPLEMENT_QUERY_INTERFACE( TextureViewD3D11Impl, IID_TextureViewD3D11, TTextureViewBase )

}
