/*     Copyright 2015-2017 Egor Yusov
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
#include <atlbase.h>

#include "BufferD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "BufferViewD3D11Impl.h"
#include "GraphicsUtilities.h"
#include "EngineMemory.h"

namespace Diligent
{

BufferD3D11Impl :: BufferD3D11Impl(FixedBlockMemoryAllocator &BufferObjMemAllocator, 
                                   FixedBlockMemoryAllocator &BuffViewObjMemAllocator,
                                   RenderDeviceD3D11Impl *pRenderDeviceD3D11, 
                                   const BufferDesc& BuffDesc, 
                                   const BufferData &BuffData /*= BufferData()*/) : 
    TBufferBase(BufferObjMemAllocator, BuffViewObjMemAllocator, pRenderDeviceD3D11, BuffDesc, false)
{
#define LOG_BUFFER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\": ", ##__VA_ARGS__);

    if( BuffDesc.Usage == USAGE_STATIC && BuffData.pData == nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Static buffer must be initialized with data at creation time");
    D3D11_BUFFER_DESC D3D11BuffDesc;
    D3D11BuffDesc.BindFlags = BindFlagsToD3D11BindFlags(BuffDesc.BindFlags);
    D3D11BuffDesc.ByteWidth = BuffDesc.uiSizeInBytes;
    D3D11BuffDesc.MiscFlags = 0;
    if( BuffDesc.BindFlags & BIND_INDIRECT_DRAW_ARGS )
    {
        D3D11BuffDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    D3D11BuffDesc.Usage = UsageToD3D11Usage(BuffDesc.Usage);
    
    D3D11BuffDesc.StructureByteStride = 0;
    if( (BuffDesc.BindFlags & BIND_UNORDERED_ACCESS) )
    {
        if( BuffDesc.Mode == BUFFER_MODE_STRUCTURED )
        {
            D3D11BuffDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            D3D11BuffDesc.StructureByteStride = BuffDesc.ElementByteStride;
        }
        else if( BuffDesc.Mode == BUFFER_MODE_FORMATED )
        {
            auto ElementStride = GetValueSize( BuffDesc.Format.ValueType ) * BuffDesc.Format.NumComponents;
            VERIFY( m_Desc.ElementByteStride == 0 || m_Desc.ElementByteStride == ElementStride, "Element byte stride does not match buffer format" );
            m_Desc.ElementByteStride = ElementStride;
            if( BuffDesc.Format.ValueType == VT_FLOAT32 || BuffDesc.Format.ValueType == VT_FLOAT16 )
                m_Desc.Format.IsNormalized = false;
        }
        else
        {
            UNEXPECTED( "Buffer UAV type is not correct" );
        }
    }

    D3D11BuffDesc.CPUAccessFlags = CPUAccessFlagsToD3D11CPUAccessFlags( BuffDesc.CPUAccessFlags );

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = BuffData.pData;
    InitData.SysMemPitch = BuffData.DataSize;
    InitData.SysMemSlicePitch = 0;

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateBuffer(&D3D11BuffDesc, InitData.pSysMem ? &InitData : nullptr, &m_pd3d11Buffer),
                            "Failed to create the Direct3D11 buffer" );
}

BufferD3D11Impl :: ~BufferD3D11Impl()
{
}

IMPLEMENT_QUERY_INTERFACE( BufferD3D11Impl, IID_BufferD3D11, TBufferBase )

void BufferD3D11Impl::UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )
{
    TBufferBase::UpdateData( pContext, Offset, Size, pData );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();

    D3D11_BOX DstBox;
    DstBox.left = Offset;
    DstBox.right = Offset + Size;
    DstBox.top = 0;
    DstBox.bottom = 1;
    DstBox.front = 0;
    DstBox.back = 1;
    auto *pDstBox = (Offset == 0 && Size == m_Desc.uiSizeInBytes) ? nullptr : &DstBox;
    pd3d11DeviceContext->UpdateSubresource(m_pd3d11Buffer, 0, pDstBox, pData, 0, 0);
}

void BufferD3D11Impl :: CopyData(IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size)
{
    TBufferBase::CopyData( pContext, pSrcBuffer, SrcOffset, DstOffset, Size );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    auto *pSrBufferD3D11Impl = static_cast<BufferD3D11Impl*>( pSrcBuffer );
    
    D3D11_BOX SrcBox;
    SrcBox.left = SrcOffset;
    SrcBox.right = SrcOffset + Size;
    SrcBox.top = 0;
    SrcBox.bottom = 1;
    SrcBox.front = 0;
    SrcBox.back = 1;
    pd3d11DeviceContext->CopySubresourceRegion(m_pd3d11Buffer, 0, DstOffset, 0, 0, pSrBufferD3D11Impl->m_pd3d11Buffer, 0, &SrcBox);
}

void BufferD3D11Impl :: Map(IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData)
{
    TBufferBase::Map( pContext, MapType, MapFlags, pMappedData );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    auto d3d11MapType = MapTypeToD3D11MapType(MapType);
    auto d3d11MapFlags = MapFlagsToD3D11MapFlags(MapFlags);

    D3D11_MAPPED_SUBRESOURCE MappedBuff;
    HRESULT hr = pd3d11DeviceContext->Map(m_pd3d11Buffer, 0, d3d11MapType, d3d11MapFlags, &MappedBuff);

    pMappedData = SUCCEEDED(hr) ? MappedBuff.pData : nullptr;

    VERIFY( pMappedData || (MapFlags & MAP_FLAG_DO_NOT_WAIT) && (hr == DXGI_ERROR_WAS_STILL_DRAWING), "Map failed" );
}

void BufferD3D11Impl::Unmap( IDeviceContext *pContext, MAP_TYPE MapType )
{
    TBufferBase::Unmap( pContext, MapType );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    pd3d11DeviceContext->Unmap(m_pd3d11Buffer, 0);
}

void BufferD3D11Impl::CreateViewInternal( const BufferViewDesc &OrigViewDesc, IBufferView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;

    try
    {
        auto *pDeviceD3D11Impl = ValidatedCast<RenderDeviceD3D11Impl>(GetDevice());
        auto &BuffViewAllocator = pDeviceD3D11Impl->GetBuffViewObjAllocator();
        VERIFY( &BuffViewAllocator == &m_dbgBuffViewAllocator, "Buff view allocator does not match allocator provided at buffer initialization" );

        BufferViewDesc ViewDesc = OrigViewDesc;
        if( ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS )
        {
            CComPtr<ID3D11UnorderedAccessView> pUAV;
            CreateUAV( ViewDesc, &pUAV );
            *ppView = NEW(BuffViewAllocator, "BufferViewD3D11Impl instance",  BufferViewD3D11Impl, pDeviceD3D11Impl, ViewDesc, this, pUAV, bIsDefaultView );
        }
        else if( ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE )
        {
			CComPtr<ID3D11ShaderResourceView> pSRV;
            CreateSRV( ViewDesc, &pSRV );
            *ppView = NEW(BuffViewAllocator, "BufferViewD3D11Impl instance",  BufferViewD3D11Impl, pDeviceD3D11Impl, ViewDesc, this, pSRV, bIsDefaultView );
        }

        if( !bIsDefaultView && *ppView )
            (*ppView)->AddRef();
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name, "\"" )
    }
}

void BufferD3D11Impl::CreateUAV( BufferViewDesc &UAVDesc, ID3D11UnorderedAccessView **ppD3D11UAV )
{
    CorrectBufferViewDesc( UAVDesc );

    D3D11_UNORDERED_ACCESS_VIEW_DESC D3D11_UAVDesc;
    BufferViewDesc_to_D3D11_UAV_DESC(m_Desc, UAVDesc, D3D11_UAVDesc);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateUnorderedAccessView( m_pd3d11Buffer, &D3D11_UAVDesc, ppD3D11UAV ),
                            "Failed to create D3D11 unordered access view" );
}

void BufferD3D11Impl::CreateSRV( struct BufferViewDesc &SRVDesc, ID3D11ShaderResourceView **ppD3D11SRV )
{
    CorrectBufferViewDesc( SRVDesc );

    D3D11_SHADER_RESOURCE_VIEW_DESC D3D11_SRVDesc;
    BufferViewDesc_to_D3D11_SRV_DESC(m_Desc, SRVDesc, D3D11_SRVDesc);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateShaderResourceView( m_pd3d11Buffer, &D3D11_SRVDesc, ppD3D11SRV ),
                            "Failed to create D3D11 shader resource view" );
}

}
