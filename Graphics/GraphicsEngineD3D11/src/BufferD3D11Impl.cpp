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

#include "pch.h"
#include <atlbase.h>

#include "BufferD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "BufferViewD3D11Impl.h"
#include "GraphicsAccessories.h"
#include "EngineMemory.h"

namespace Diligent
{

BufferD3D11Impl :: BufferD3D11Impl(IReferenceCounters*        pRefCounters, 
                                   FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                                   RenderDeviceD3D11Impl*     pRenderDeviceD3D11, 
                                   const BufferDesc&          BuffDesc, 
                                   const BufferData&          BuffData /*= BufferData()*/) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceD3D11, BuffDesc, false)
{
#define LOG_BUFFER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Buffer \"", m_Desc.Name ? m_Desc.Name : "", "\": ", ##__VA_ARGS__);

    if( m_Desc.Usage == USAGE_STATIC && BuffData.pData == nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Static buffer must be initialized with data at creation time");

    if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
    {
        Uint32 AlignmentMask = 15;
        m_Desc.uiSizeInBytes = (m_Desc.uiSizeInBytes + AlignmentMask) & (~AlignmentMask);
    }

    D3D11_BUFFER_DESC D3D11BuffDesc;
    D3D11BuffDesc.BindFlags = BindFlagsToD3D11BindFlags(m_Desc.BindFlags);
    D3D11BuffDesc.ByteWidth = m_Desc.uiSizeInBytes;
    D3D11BuffDesc.MiscFlags = 0;
    if( m_Desc.BindFlags & BIND_INDIRECT_DRAW_ARGS )
    {
        D3D11BuffDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    D3D11BuffDesc.Usage = UsageToD3D11Usage(m_Desc.Usage);
    
    D3D11BuffDesc.StructureByteStride = 0;
    if( (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) || (m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
    {
        if( m_Desc.Mode == BUFFER_MODE_STRUCTURED )
        {
            D3D11BuffDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            D3D11BuffDesc.StructureByteStride = m_Desc.ElementByteStride;
        }
        else if( m_Desc.Mode == BUFFER_MODE_FORMATTED )
        {
            auto ElementStride = GetValueSize( m_Desc.Format.ValueType ) * m_Desc.Format.NumComponents;
            VERIFY( m_Desc.ElementByteStride == 0 || m_Desc.ElementByteStride == ElementStride, "Element byte stride does not match buffer format" );
            m_Desc.ElementByteStride = ElementStride;
            if( m_Desc.Format.ValueType == VT_FLOAT32 || m_Desc.Format.ValueType == VT_FLOAT16 )
                m_Desc.Format.IsNormalized = false;
        }
        else
        {
            UNEXPECTED( "Buffer UAV type is not correct" );
        }
    }

    D3D11BuffDesc.CPUAccessFlags = CPUAccessFlagsToD3D11CPUAccessFlags( m_Desc.CPUAccessFlags );

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = BuffData.pData;
    InitData.SysMemPitch = BuffData.DataSize;
    InitData.SysMemSlicePitch = 0;

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateBuffer(&D3D11BuffDesc, InitData.pSysMem ? &InitData : nullptr, &m_pd3d11Buffer),
                            "Failed to create the Direct3D11 buffer" );
}

static BufferDesc BuffDescFromD3D11Buffer(ID3D11Buffer *pd3d11Buffer, BufferDesc BuffDesc)
{
    D3D11_BUFFER_DESC D3D11BuffDesc;
    pd3d11Buffer->GetDesc(&D3D11BuffDesc);

    VERIFY(BuffDesc.uiSizeInBytes == 0 || BuffDesc.uiSizeInBytes == D3D11BuffDesc.ByteWidth, "Buffer size specified by the BufferDesc (",BuffDesc.uiSizeInBytes,") does not match d3d11 buffer size (", D3D11BuffDesc.ByteWidth, ")" );
    BuffDesc.uiSizeInBytes = Uint32{ D3D11BuffDesc.ByteWidth };

    auto BindFlags = D3D11BindFlagsToBindFlags(D3D11BuffDesc.BindFlags);
    if (D3D11BuffDesc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
        BindFlags |= BIND_INDIRECT_DRAW_ARGS;
    VERIFY(BuffDesc.BindFlags == 0 || BuffDesc.BindFlags == BindFlags, "Bind flags specified by the BufferDesc (", BuffDesc.BindFlags,") do not match bind flags recovered from d3d11 buffer desc (", BindFlags, ")" );
    BuffDesc.BindFlags = BindFlags;

    auto Usage = D3D11UsageToUsage(D3D11BuffDesc.Usage);
    VERIFY(BuffDesc.Usage == 0 || BuffDesc.Usage == Usage, "Usage specified by the BufferDesc (", BuffDesc.Usage,") do not match buffer usage recovered from d3d11 buffer desc (", Usage, ")" );
    BuffDesc.Usage = Usage;

    auto CPUAccessFlags = D3D11CPUAccessFlagsToCPUAccessFlags(D3D11BuffDesc.CPUAccessFlags);
    VERIFY(BuffDesc.CPUAccessFlags == 0 || BuffDesc.CPUAccessFlags == CPUAccessFlags, "CPU access flags specified by the BufferDesc (", BuffDesc.CPUAccessFlags, ") do not match CPU access flags recovered from d3d11 buffer desc (", CPUAccessFlags, ")");
    BuffDesc.CPUAccessFlags = CPUAccessFlags;

    if( (BuffDesc.BindFlags & BIND_UNORDERED_ACCESS) || (BuffDesc.BindFlags & BIND_SHADER_RESOURCE) )
    {
        if(D3D11BuffDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
        {
            VERIFY(BuffDesc.Mode == BUFFER_MODE_UNDEFINED || BuffDesc.Mode == BUFFER_MODE_STRUCTURED, "Unexpected buffer mode");
            BuffDesc.Mode = BUFFER_MODE_STRUCTURED;
            VERIFY(BuffDesc.ElementByteStride == 0 || BuffDesc.ElementByteStride == D3D11BuffDesc.StructureByteStride, "Element byte stride specified by the BufferDesc (", BuffDesc.ElementByteStride, ") does not match structured byte stride recovered from d3d11 buffer desc (", D3D11BuffDesc.StructureByteStride, ")");
            BuffDesc.ElementByteStride = Uint32{ D3D11BuffDesc.StructureByteStride };
        }
        else
        {
            VERIFY(BuffDesc.Mode == BUFFER_MODE_UNDEFINED || BuffDesc.Mode == BUFFER_MODE_FORMATTED, "Unexpected buffer mode");
            BuffDesc.Mode = BUFFER_MODE_FORMATTED;
            VERIFY( BuffDesc.Format.ValueType != VT_UNDEFINED, "Value type is not specified for a formatted buffer" );
            VERIFY( BuffDesc.Format.NumComponents != 0, "Num components cannot be zero in a formated buffer" );
        }
    }

    return BuffDesc;
}
BufferD3D11Impl :: BufferD3D11Impl(IReferenceCounters*          pRefCounters,
                                   FixedBlockMemoryAllocator&   BuffViewObjMemAllocator,
                                   class RenderDeviceD3D11Impl* pDeviceD3D11, 
                                   const BufferDesc&            BuffDesc, 
                                   ID3D11Buffer*                pd3d11Buffer) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pDeviceD3D11, BuffDescFromD3D11Buffer(pd3d11Buffer, BuffDesc), false)
{
    m_pd3d11Buffer = pd3d11Buffer;
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

void BufferD3D11Impl :: Map(IDeviceContext* pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid& pMappedData)
{
    TBufferBase::Map( pContext, MapType, MapFlags, pMappedData );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    D3D11_MAP d3d11MapType = static_cast<D3D11_MAP>(0);
    UINT d3d11MapFlags = 0;
    MapParamsToD3D11MapParams(MapType, MapFlags, d3d11MapType, d3d11MapFlags);

    D3D11_MAPPED_SUBRESOURCE MappedBuff;
    HRESULT hr = pd3d11DeviceContext->Map(m_pd3d11Buffer, 0, d3d11MapType, d3d11MapFlags, &MappedBuff);

    pMappedData = SUCCEEDED(hr) ? MappedBuff.pData : nullptr;

    VERIFY( pMappedData || (MapFlags & MAP_FLAG_DO_NOT_WAIT) && (hr == DXGI_ERROR_WAS_STILL_DRAWING), "Map failed" );
}

void BufferD3D11Impl::Unmap( IDeviceContext* pContext, MAP_TYPE MapType, Uint32 MapFlags )
{
    TBufferBase::Unmap( pContext, MapType, MapFlags );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    pd3d11DeviceContext->Unmap(m_pd3d11Buffer, 0);
}

void BufferD3D11Impl::CreateViewInternal( const BufferViewDesc& OrigViewDesc, IBufferView** ppView, bool bIsDefaultView )
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
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewD3D11Impl instance",  BufferViewD3D11Impl, bIsDefaultView ? this : nullptr)
                                ( pDeviceD3D11Impl, ViewDesc, this, pUAV, bIsDefaultView );
        }
        else if( ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE )
        {
			CComPtr<ID3D11ShaderResourceView> pSRV;
            CreateSRV( ViewDesc, &pSRV );
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewD3D11Impl instance",  BufferViewD3D11Impl, bIsDefaultView ? this : nullptr)
                                (pDeviceD3D11Impl, ViewDesc, this, pSRV, bIsDefaultView );
        }

        if( !bIsDefaultView && *ppView )
            (*ppView)->AddRef();
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name, "\"" );
    }
}

void BufferD3D11Impl::CreateUAV( BufferViewDesc& UAVDesc, ID3D11UnorderedAccessView** ppD3D11UAV )
{
    CorrectBufferViewDesc( UAVDesc );

    D3D11_UNORDERED_ACCESS_VIEW_DESC D3D11_UAVDesc;
    BufferViewDesc_to_D3D11_UAV_DESC(m_Desc, UAVDesc, D3D11_UAVDesc);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateUnorderedAccessView( m_pd3d11Buffer, &D3D11_UAVDesc, ppD3D11UAV ),
                            "Failed to create D3D11 unordered access view" );
}

void BufferD3D11Impl::CreateSRV( struct BufferViewDesc& SRVDesc, ID3D11ShaderResourceView** ppD3D11SRV )
{
    CorrectBufferViewDesc( SRVDesc );

    D3D11_SHADER_RESOURCE_VIEW_DESC D3D11_SRVDesc;
    BufferViewDesc_to_D3D11_SRV_DESC(m_Desc, SRVDesc, D3D11_SRVDesc);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateShaderResourceView( m_pd3d11Buffer, &D3D11_SRVDesc, ppD3D11SRV ),
                            "Failed to create D3D11 shader resource view" );
}

}
