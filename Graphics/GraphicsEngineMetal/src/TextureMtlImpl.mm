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

#include "TextureMtlImpl.h"
#include "RenderDeviceMtlImpl.h"
#include "DeviceContextMtlImpl.h"
#include "MtlTypeConversions.h"
#include "TextureViewMtlImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

TextureMtlImpl :: TextureMtlImpl(IReferenceCounters*        pRefCounters,
                                 FixedBlockMemoryAllocator& TexViewObjAllocator,
                                 RenderDeviceMtlImpl*       pRenderDeviceMtl,
                                 const TextureDesc&         TexDesc,
                                 const TextureData*         pInitData /*= nullptr*/) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceMtl, TexDesc)
{
    LOG_ERROR_AND_THROW("Textures are not implemented in Metal backend");

    if( (TexDesc.Usage == USAGE_STATIC && pInitData == nullptr) || pInitData->pSubResources == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");
    SetState(RESOURCE_STATE_UNDEFINED);
}

IMPLEMENT_QUERY_INTERFACE( TextureMtlImpl, IID_TextureMtl, TTextureBase )

TextureMtlImpl :: ~TextureMtlImpl()
{
}

void TextureMtlImpl::CreateViewInternal( const TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "View pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;

    try
    {
        LOG_ERROR_MESSAGE("TextureMtlImpl::CreateViewInternal() is not implemented");
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetTexViewTypeLiteralName(ViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", ViewDesc.Name ? ViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"" );
    }
}

}
