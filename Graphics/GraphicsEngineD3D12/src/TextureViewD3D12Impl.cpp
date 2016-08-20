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

#include "pch.h"
#include "TextureViewD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"

namespace Diligent
{

TextureViewD3D12Impl::TextureViewD3D12Impl( FixedBlockMemoryAllocator &TexViewObjAllocator,
                                            IRenderDevice *pDevice, 
                                            const TextureViewDesc& ViewDesc, 
                                            ITexture *pTexture,
                                            DescriptorHeapAllocation &&HandleAlloc,
                                            bool bIsDefaultView ) :
    TTextureViewBase( TexViewObjAllocator, pDevice, ViewDesc, pTexture, bIsDefaultView ),
    m_Descriptor(std::move(HandleAlloc))
{
}
//
//ID3D12View* TextureViewD3D12Impl::GetD3D12View()
//{
//    return m_pD3D12View;
//}

IMPLEMENT_QUERY_INTERFACE( TextureViewD3D12Impl, IID_TextureViewD3D12, TTextureViewBase )

void TextureViewD3D12Impl::GenerateMips( IDeviceContext *pContext )
{
    VERIFY( m_Desc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "GenerateMips() is allowed for shader resource views only, ", GetTexViewTypeLiteralName(m_Desc.ViewType), " is not allowed." );
    if( m_Desc.ViewType != TEXTURE_VIEW_SHADER_RESOURCE )
    {
        LOG_ERROR("GenerateMips() is allowed for shader resource views only, ", GetTexViewTypeLiteralName(m_Desc.ViewType), " is not allowed.");
        return;
    }
    auto *pDeviceCtxD3D12 = ValidatedCast<DeviceContextD3D12Impl>( pContext );
    pDeviceCtxD3D12->GenerateMips(this);
}

}
