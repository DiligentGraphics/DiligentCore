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

#include "BufferMtlImpl.h"
#include "RenderDeviceMtlImpl.h"
#include "DeviceContextMtlImpl.h"
#include "MtlTypeConversions.h"
#include "BufferViewMtlImpl.h"
#include "GraphicsAccessories.hpp"
#include "EngineMemory.h"

namespace Diligent
{

BufferMtlImpl :: BufferMtlImpl(IReferenceCounters*        pRefCounters, 
                               FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                               RenderDeviceMtlImpl*       pRenderDeviceMtl, 
                               const BufferDesc&          BuffDesc, 
                               const BufferData*          pBuffData /*= nullptr*/) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceMtl, BuffDesc, false)
{
    LOG_ERROR_AND_THROW("Buffers are not implemented in Metal backend");
}

BufferDesc BuffDescFromMtlBuffer(void* pMtlBuffer, const BufferDesc& Desc)
{
    UNSUPPORTED("Not implemented");
    return BufferDesc{};
}

BufferMtlImpl :: BufferMtlImpl(IReferenceCounters*        pRefCounters,
                               FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                               class RenderDeviceMtlImpl* pDeviceMtl, 
                               const BufferDesc&          BuffDesc, 
                               RESOURCE_STATE             InitialState,
                               void*                      pMtlBuffer) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pDeviceMtl, BuffDescFromMtlBuffer(pMtlBuffer, BuffDesc), false)
{
    SetState(InitialState);
}

BufferMtlImpl :: ~BufferMtlImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( BufferMtlImpl, IID_BufferMtl, TBufferBase )

void BufferMtlImpl::CreateViewInternal( const BufferViewDesc& OrigViewDesc, IBufferView** ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;

    try
    {
        LOG_ERROR_MESSAGE("BufferD3D11Impl::CreateViewInternal() is not implemented");
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view '", OrigViewDesc.Name ? OrigViewDesc.Name : "", "' (", ViewTypeName, ") for buffer '", m_Desc.Name, "'" );
    }
}

}
