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
#include "BufferD3D12Impl.h"
#include "RenderDeviceD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"
#include "D3D12TypeConversions.h"
#include "BufferViewD3D12Impl.h"
#include "GraphicsUtilities.h"
#include "DXGITypeConversions.h"
#include "EngineMemory.h"
#include "StringTools.h"

namespace Diligent
{

BufferD3D12Impl :: BufferD3D12Impl(FixedBlockMemoryAllocator &BufferObjMemAllocator, 
                                   FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                                   RenderDeviceD3D12Impl *pRenderDeviceD3D12, 
                                   const BufferDesc& BuffDesc, 
                                   const BufferData &BuffData /*= BufferData()*/) : 
    TBufferBase(BufferObjMemAllocator, BuffViewObjMemAllocator, pRenderDeviceD3D12, BuffDesc),
#ifdef _DEBUG
    m_DbgMapType(1 + pRenderDeviceD3D12->GetNumDeferredContexts(), static_cast<MAP_TYPE>(-1), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<MAP_TYPE>")),
#endif
    m_DynamicData(BuffDesc.Usage == USAGE_DYNAMIC ? (1 + pRenderDeviceD3D12->GetNumDeferredContexts()) : 0, DynamicAllocation(), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
{
#define LOG_BUFFER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\": ", ##__VA_ARGS__);

    if( m_Desc.Usage == USAGE_STATIC && BuffData.pData == nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Static buffer must be initialized with data at creation time")

    if( m_Desc.Usage == USAGE_DYNAMIC && BuffData.pData != nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Dynamic buffer must be initialized via Map()")

    if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
    {
        Uint32 AlignmentMask = 255;
        m_Desc.uiSizeInBytes = (m_Desc.uiSizeInBytes + AlignmentMask) & (~AlignmentMask);
    }

    if(m_Desc.Usage == USAGE_DYNAMIC)
    {
        // Dynamic buffers are suballocated in the upload heap when Map() is called.
        // Dynamic upload heap buffer is always in D3D12_RESOURCE_STATE_GENERIC_READ state

        m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
        VERIFY_EXPR(m_DynamicData.size() == 1 + pRenderDeviceD3D12->GetNumDeferredContexts());
    }
    else
    {
        D3D12_RESOURCE_DESC D3D12BuffDesc = {};
        D3D12BuffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        D3D12BuffDesc.Alignment = 0;
        D3D12BuffDesc.Width = m_Desc.uiSizeInBytes;
        D3D12BuffDesc.Height = 1;
        D3D12BuffDesc.DepthOrArraySize = 1;
        D3D12BuffDesc.MipLevels = 1;
        D3D12BuffDesc.Format = DXGI_FORMAT_UNKNOWN;
        D3D12BuffDesc.SampleDesc.Count = 1;
        D3D12BuffDesc.SampleDesc.Quality = 0;
        // Layout must be D3D12_TEXTURE_LAYOUT_ROW_MAJOR, as buffer memory layouts are 
        // understood by applications and row-major texture data is commonly marshaled through buffers.
        D3D12BuffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12BuffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        if( m_Desc.BindFlags & BIND_UNORDERED_ACCESS )
            D3D12BuffDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
        auto *pd3d12Device = pRenderDeviceD3D12->GetD3D12Device();


	    D3D12_HEAP_PROPERTIES HeapProps;
	    HeapProps.Type = m_Desc.Usage == USAGE_CPU_ACCESSIBLE ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_DEFAULT;
        if(HeapProps.Type == D3D12_HEAP_TYPE_READBACK)
            m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;
        else if(HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
            m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
	    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	    HeapProps.CreationNodeMask = 1;
	    HeapProps.VisibleNodeMask = 1;

        bool bInitializeBuffer = (BuffData.pData != nullptr && BuffData.DataSize > 0);
        if(bInitializeBuffer)
            m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;

        auto hr = pd3d12Device->CreateCommittedResource( &HeapProps, D3D12_HEAP_FLAG_NONE,
		    &D3D12BuffDesc, m_UsageState, nullptr, __uuidof(m_pd3d12Resource), reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&m_pd3d12Resource)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create D3D12 buffer")

        if( *m_Desc.Name != 0)
            m_pd3d12Resource->SetName(WidenString(m_Desc.Name).c_str());

	    if( bInitializeBuffer )
        {
            D3D12_HEAP_PROPERTIES UploadHeapProps;
	        UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	        UploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	        UploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	        UploadHeapProps.CreationNodeMask = 1;
	        UploadHeapProps.VisibleNodeMask = 1;

            D3D12BuffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            CComPtr<ID3D12Resource> UploadBuffer;
            hr = pd3d12Device->CreateCommittedResource( &UploadHeapProps, D3D12_HEAP_FLAG_NONE,
		                &D3D12BuffDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,  __uuidof(UploadBuffer), 
                        reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&UploadBuffer)) );
            if(FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to create uload buffer")

	        void* DestAddress = nullptr;
	        hr = UploadBuffer->Map(0, nullptr, &DestAddress);
            if(FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to map uload buffer")
	        memcpy(DestAddress, BuffData.pData, BuffData.DataSize);
	        UploadBuffer->Unmap(0, nullptr);

            auto  *pInitContext = pRenderDeviceD3D12->AllocateCommandContext();
	        // copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default buffer
            VERIFY_EXPR(m_UsageState == D3D12_RESOURCE_STATE_COPY_DEST);
            // We MUST NOT call TransitionResource() from here, because
            // it will call AddRef() and potentially Release(), while 
            // the object is not constructed yet
	        pInitContext->CopyResource(m_pd3d12Resource, UploadBuffer);

	        pRenderDeviceD3D12->CloseAndExecuteCommandContext(pInitContext);

            // Add reference to the object to the release queue to keep it alive
            // until copy operation is complete. This must be done after
            // submitting command list for execution!
            pRenderDeviceD3D12->SafeReleaseD3D12Object(UploadBuffer);
        }

        if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
        {
            m_CBVDescriptorAllocation = pRenderDeviceD3D12->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateCBV(m_CBVDescriptorAllocation.GetCpuHandle());
        }
    }
#if 0
    D3D12BuffDesc.BindFlags = BindFlagsToD3D12BindFlags(BuffDesc.BindFlags);
    D3D12BuffDesc.ByteWidth = 
    D3D12BuffDesc.MiscFlags = 0;
    if( BuffDesc.BindFlags & BIND_INDIRECT_DRAW_ARGS )
    {
        D3D12BuffDesc.MiscFlags |= D3D12_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    D3D12BuffDesc.Usage = UsageToD3D12Usage(BuffDesc.Usage);
    
    D3D12BuffDesc.StructureByteStride = 0;
    if( (BuffDesc.BindFlags & BIND_UNORDERED_ACCESS) )
    {
        if( BuffDesc.Mode == BUFFER_MODE_STRUCTURED )
        {
            D3D12BuffDesc.MiscFlags |= D3D12_RESOURCE_MISC_BUFFER_STRUCTURED;
            D3D12BuffDesc.StructureByteStride = BuffDesc.ElementByteStride;
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

    D3D12BuffDesc.CPUAccessFlags = CPUAccessFlagsToD3D12CPUAccessFlags( BuffDesc.CPUAccessFlags );

    D3D12_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = BuffData.pData;
    InitData.SysMemPitch = BuffData.DataSize;
    InitData.SysMemSlicePitch = 0;

    auto *pDeviceD3D12 = pRenderDeviceD3D12->GetD3D12Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D12->CreateBuffer(&D3D12BuffDesc, InitData.pSysMem ? &InitData : nullptr, &m_pD3D12Buffer),
                            "Failed to create the Direct3D11 buffer" );
#endif
}

BufferD3D12Impl :: ~BufferD3D12Impl()
{
    // D3D12 object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
    pDeviceD3D12Impl->SafeReleaseD3D12Object(m_pd3d12Resource);
}

IMPLEMENT_QUERY_INTERFACE( BufferD3D12Impl, IID_BufferD3D12, TBufferBase )

void BufferD3D12Impl::UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )
{
    TBufferBase::UpdateData( pContext, Offset, Size, pData );

    // We must use cmd context from the device context provided, otherwise there will
    // be resource barrier issues in the cmd list in the device context
    auto *pDeviceContextD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pContext);
    pDeviceContextD3D12->UpdateBufferRegion(this, pData, Offset, Size);
}

void BufferD3D12Impl :: CopyData(IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size)
{
    TBufferBase::CopyData( pContext, pSrcBuffer, SrcOffset, DstOffset, Size );
    auto *pDeviceContextD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pContext);
    pDeviceContextD3D12->CopyBufferRegion(ValidatedCast<BufferD3D12Impl>(pSrcBuffer), this, SrcOffset, DstOffset, Size);
}

void BufferD3D12Impl :: Map(IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData)
{
    TBufferBase::Map( pContext, MapType, MapFlags, pMappedData );
    auto *pDeviceContextD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pContext);
#ifdef _DEBUG
    m_DbgMapType[pDeviceContextD3D12->GetContextId()] = MapType;
#endif
    if (MapType == MAP_READ )
    {
        LOG_WARNING_MESSAGE_ONCE("Mapping CPU buffer for reading on D3D12 currently requires flushing context and idling GPU");
        pDeviceContextD3D12->Flush();
        auto *pDeviceD3D12 = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
        pDeviceD3D12->IdleGPU();

        VERIFY(m_Desc.Usage == USAGE_CPU_ACCESSIBLE, "Buffer must be created as USAGE_CPU_ACCESSIBLE to be mapped for reading")
        D3D12_RANGE MapRange;
        MapRange.Begin = 0;
        MapRange.End = m_Desc.uiSizeInBytes;
        m_pd3d12Resource->Map(0, &MapRange, &pMappedData);
    }
    else if(MapType == MAP_WRITE_DISCARD)
    {
        auto *pCtxD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pContext);
        auto ContextId = pDeviceContextD3D12->GetContextId();
        m_DynamicData[ContextId] = pCtxD3D12->AllocateDynamicSpace(m_Desc.uiSizeInBytes);
        pMappedData = m_DynamicData[ContextId].CPUAddress;
    }
    else if(MapType == MAP_READ_WRITE || MapType == MAP_WRITE)
    {
        LOG_ERROR("D3D12 allows writing to CPU-readable buffer, but it is not accessable by GPU")
    }
    else
    {
        LOG_ERROR("Only MAP_WRITE_DISCARD and MAP_READ are currently implemented in D3D12")
    }
}

void BufferD3D12Impl::Unmap( IDeviceContext *pContext, MAP_TYPE MapType )
{
    TBufferBase::Unmap( pContext, MapType );
    auto *pDeviceContextD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pContext);
    auto CtxId = pDeviceContextD3D12->GetContextId();
#ifdef _DEBUG
    VERIFY(m_DbgMapType[CtxId] == MapType, "Map type provided does not match the type provided to Map()" );
#endif

    if (MapType == MAP_READ )
    {
        D3D12_RANGE MapRange;
        // It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin.
        MapRange.Begin = 1;
        MapRange.End = 0;
        m_pd3d12Resource->Unmap(0, &MapRange);
    }
    else if(MapType == MAP_WRITE_DISCARD)
    {
        // Nothing needs to be done.
    }
#ifdef _DEBUG
    m_DbgMapType[CtxId] = static_cast<MAP_TYPE>(-1);
#endif
}

void BufferD3D12Impl::CreateViewInternal( const BufferViewDesc &OrigViewDesc, IBufferView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;

    try
    {
        auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
        auto &BuffViewAllocator = pDeviceD3D12Impl->GetBuffViewObjAllocator();
        VERIFY( &BuffViewAllocator == &m_dbgBuffViewAllocator, "Buff view allocator does not match allocator provided at buffer initialization" );

        BufferViewDesc ViewDesc = OrigViewDesc;
        if( ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS )
        {
            auto UAVHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateUAV( ViewDesc, UAVHandleAlloc.GetCpuHandle() );
            *ppView = NEW(BuffViewAllocator, "BufferViewD3D12Impl instance", BufferViewD3D12Impl, GetDevice(), ViewDesc, this, std::move(UAVHandleAlloc), bIsDefaultView );
        }
        else if( ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE )
        {
			auto SRVHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateSRV( ViewDesc, SRVHandleAlloc.GetCpuHandle() );
            *ppView = NEW(BuffViewAllocator, "BufferViewD3D12Impl instance", BufferViewD3D12Impl, GetDevice(), ViewDesc, this, std::move(SRVHandleAlloc), bIsDefaultView );
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

void BufferD3D12Impl::CreateUAV( BufferViewDesc &UAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE UAVDescriptor )
{
    CorrectBufferViewDesc( UAVDesc );

    D3D12_UNORDERED_ACCESS_VIEW_DESC D3D12_UAVDesc;
    BufferViewDesc_to_D3D12_UAV_DESC(m_Desc, UAVDesc, D3D12_UAVDesc);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateUnorderedAccessView( m_pd3d12Resource, nullptr, &D3D12_UAVDesc, UAVDescriptor );
}

void BufferD3D12Impl::CreateSRV( struct BufferViewDesc &SRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE SRVDescriptor )
{
    CorrectBufferViewDesc( SRVDesc );

    D3D12_SHADER_RESOURCE_VIEW_DESC D3D12_SRVDesc;
    BufferViewDesc_to_D3D12_SRV_DESC(m_Desc, SRVDesc, D3D12_SRVDesc);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateShaderResourceView( m_pd3d12Resource, &D3D12_SRVDesc, SRVDescriptor );
}

void BufferD3D12Impl::CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE CBVDescriptor)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC D3D12_CBVDesc;
    D3D12_CBVDesc.BufferLocation = m_pd3d12Resource->GetGPUVirtualAddress();
    D3D12_CBVDesc.SizeInBytes = m_Desc.uiSizeInBytes;

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateConstantBufferView( &D3D12_CBVDesc, CBVDescriptor );
}

#ifdef _DEBUG
void BufferD3D12Impl::DbgVerifyDynamicAllocation(Uint32 ContextId)
{
    VERIFY(m_DynamicData[ContextId].GPUAddress != 0, "Dynamic buffer must be mapped before the first use");
	auto CurrentFrame = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice())->GetCurrentFrameNumber();
    VERIFY(m_DynamicData[ContextId].FrameNum == CurrentFrame, "Dynamic allocation is out-of-date. Dynamic buffer must be mapped in the same frame it is used.");
    VERIFY(GetState() == D3D12_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers are expected to always be in D3D12_RESOURCE_STATE_GENERIC_READ state")
}
#endif


}
