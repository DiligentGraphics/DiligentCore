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
#include "BufferVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
//#include "VkTypeConversions.h"
#include "BufferViewVkImpl.h"
#include "GraphicsAccessories.h"
//#include "DXGITypeConversions.h"
#include "EngineMemory.h"
#include "StringTools.h"

namespace Diligent
{

BufferVkImpl :: BufferVkImpl(IReferenceCounters *pRefCounters, 
                             FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                             RenderDeviceVkImpl *pRenderDeviceVk, 
                             const BufferDesc& BuffDesc, 
                             const BufferData &BuffData /*= BufferData()*/) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceVk, BuffDesc, false)/*,
#ifdef _DEBUG
    m_DbgMapType(1 + pRenderDeviceVk->GetNumDeferredContexts(), std::make_pair(static_cast<MAP_TYPE>(-1), static_cast<Uint32>(-1)), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<pair<MAP_TYPE,Uint32>>")),
#endif
    m_DynamicData(BuffDesc.Usage == USAGE_DYNAMIC ? (1 + pRenderDeviceVk->GetNumDeferredContexts()) : 0, DynamicAllocation(), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
    */
{
#if 0
#define LOG_BUFFER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\": ", ##__VA_ARGS__);

    if( m_Desc.Usage == USAGE_STATIC && BuffData.pData == nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Static buffer must be initialized with data at creation time")

    if( m_Desc.Usage == USAGE_DYNAMIC && BuffData.pData != nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Dynamic buffer must be initialized via Map()")

    Uint32 AlignmentMask = 1;
    if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
        AlignmentMask = 255;
    
    if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
    {
        if (m_Desc.CPUAccessFlags != CPU_ACCESS_WRITE && m_Desc.CPUAccessFlags != CPU_ACCESS_READ)
            LOG_BUFFER_ERROR_AND_THROW("Exactly one of the CPU_ACCESS_WRITE or CPU_ACCESS_READ flags must be specified for a cpu-accessible buffer")

        if (m_Desc.CPUAccessFlags == CPU_ACCESS_WRITE)
        {
            if(BuffData.pData != nullptr )
                LOG_BUFFER_ERROR_AND_THROW("CPU-writable staging buffers must be updated via map")

            AlignmentMask = Vk_TEXTURE_DATA_PITCH_ALIGNMENT - 1;
        }
    }
    
    if(AlignmentMask != 1)
        m_Desc.uiSizeInBytes = (m_Desc.uiSizeInBytes + AlignmentMask) & (~AlignmentMask);

    if(m_Desc.Usage == USAGE_DYNAMIC && (m_Desc.BindFlags & (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) == 0)
    {
        // Dynamic constant/vertex/index buffers are suballocated in the upload heap when Map() is called.
        // Dynamic buffers with SRV or UAV flags need to be allocated in GPU-only memory
        // Dynamic upload heap buffer is always in Vk_RESOURCE_STATE_GENERIC_READ state

        m_UsageState = Vk_RESOURCE_STATE_GENERIC_READ;
        VERIFY_EXPR(m_DynamicData.size() == 1 + pRenderDeviceVk->GetNumDeferredContexts());
    }
    else
    {
        Vk_RESOURCE_DESC VkBuffDesc = {};
        VkBuffDesc.Dimension = Vk_RESOURCE_DIMENSION_BUFFER;
        VkBuffDesc.Alignment = 0;
        VkBuffDesc.Width = m_Desc.uiSizeInBytes;
        VkBuffDesc.Height = 1;
        VkBuffDesc.DepthOrArraySize = 1;
        VkBuffDesc.MipLevels = 1;
        VkBuffDesc.Format = DXGI_FORMAT_UNKNOWN;
        VkBuffDesc.SampleDesc.Count = 1;
        VkBuffDesc.SampleDesc.Quality = 0;
        // Layout must be Vk_TEXTURE_LAYOUT_ROW_MAJOR, as buffer memory layouts are 
        // understood by applications and row-major texture data is commonly marshaled through buffers.
        VkBuffDesc.Layout = Vk_TEXTURE_LAYOUT_ROW_MAJOR;
        VkBuffDesc.Flags = Vk_RESOURCE_FLAG_NONE;
        if( m_Desc.BindFlags & BIND_UNORDERED_ACCESS )
            VkBuffDesc.Flags |= Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if( !(m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
            VkBuffDesc.Flags |= Vk_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

        auto *pVkDevice = pRenderDeviceVk->GetVkDevice();

        Vk_HEAP_PROPERTIES HeapProps;
        if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
            HeapProps.Type = m_Desc.CPUAccessFlags == CPU_ACCESS_READ ? Vk_HEAP_TYPE_READBACK : Vk_HEAP_TYPE_UPLOAD;
        else
            HeapProps.Type = Vk_HEAP_TYPE_DEFAULT;

        if(HeapProps.Type == Vk_HEAP_TYPE_READBACK)
            m_UsageState = Vk_RESOURCE_STATE_COPY_DEST;
        else if(HeapProps.Type == Vk_HEAP_TYPE_UPLOAD)
            m_UsageState = Vk_RESOURCE_STATE_GENERIC_READ;
	    HeapProps.CPUPageProperty = Vk_CPU_PAGE_PROPERTY_UNKNOWN;
	    HeapProps.MemoryPoolPreference = Vk_MEMORY_POOL_UNKNOWN;
	    HeapProps.CreationNodeMask = 1;
	    HeapProps.VisibleNodeMask = 1;

        bool bInitializeBuffer = (BuffData.pData != nullptr && BuffData.DataSize > 0);
        if(bInitializeBuffer)
            m_UsageState = Vk_RESOURCE_STATE_COPY_DEST;

        auto hr = pVkDevice->CreateCommittedResource( &HeapProps, Vk_HEAP_FLAG_NONE,
		    &VkBuffDesc, m_UsageState, nullptr, __uuidof(m_pVkResource), reinterpret_cast<void**>(static_cast<IVkResource**>(&m_pVkResource)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create Vk buffer");

        if( *m_Desc.Name != 0)
            m_pVkResource->SetName(WidenString(m_Desc.Name).c_str());

	    if( bInitializeBuffer )
        {
            Vk_HEAP_PROPERTIES UploadHeapProps;
	        UploadHeapProps.Type = Vk_HEAP_TYPE_UPLOAD;
	        UploadHeapProps.CPUPageProperty = Vk_CPU_PAGE_PROPERTY_UNKNOWN;
	        UploadHeapProps.MemoryPoolPreference = Vk_MEMORY_POOL_UNKNOWN;
	        UploadHeapProps.CreationNodeMask = 1;
	        UploadHeapProps.VisibleNodeMask = 1;

            VkBuffDesc.Flags = Vk_RESOURCE_FLAG_NONE;
            CComPtr<IVkResource> UploadBuffer;
            hr = pVkDevice->CreateCommittedResource( &UploadHeapProps, Vk_HEAP_FLAG_NONE,
		                &VkBuffDesc, Vk_RESOURCE_STATE_GENERIC_READ, nullptr,  __uuidof(UploadBuffer), 
                        reinterpret_cast<void**>(static_cast<IVkResource**>(&UploadBuffer)) );
            if(FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to create uload buffer");

	        void* DestAddress = nullptr;
	        hr = UploadBuffer->Map(0, nullptr, &DestAddress);
            if(FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to map uload buffer");
	        memcpy(DestAddress, BuffData.pData, BuffData.DataSize);
	        UploadBuffer->Unmap(0, nullptr);

            auto  *pInitContext = pRenderDeviceVk->AllocateCommandContext();
	        // copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default buffer
            VERIFY_EXPR(m_UsageState == Vk_RESOURCE_STATE_COPY_DEST);
            // We MUST NOT call TransitionResource() from here, because
            // it will call AddRef() and potentially Release(), while 
            // the object is not constructed yet
	        pInitContext->CopyResource(m_pVkResource, UploadBuffer);

            // Command list fence should only be signaled when submitting cmd list
            // from the immediate context, otherwise the basic requirement will be violated
            // as in the scenario below
            // See http://diligentgraphics.com/diligent-engine/architecture/Vk/managing-resource-lifetimes/
            //                                                           
            //  Signaled Fence  |        Immediate Context               |            InitContext            |
            //                  |                                        |                                   |
            //    N             |  Draw(ResourceX)                       |                                   |
            //                  |  Release(ResourceX)                    |                                   |
            //                  |   - (ResourceX, N) -> Release Queue    |                                   |
            //                  |                                        | CopyResource()                    |
            //   N+1            |                                        | CloseAndExecuteCommandContext()   |
            //                  |                                        |                                   |
            //   N+2            |  CloseAndExecuteCommandContext()       |                                   |
            //                  |   - Cmd list is submitted with number  |                                   |
            //                  |     N+1, but resource it references    |                                   |
            //                  |     was added to the delete queue      |                                   |
            //                  |     with value N                       |                                   |
	        pRenderDeviceVk->CloseAndExecuteCommandContext(pInitContext, false);

            // Add reference to the object to the release queue to keep it alive
            // until copy operation is complete. This must be done after
            // submitting command list for execution!
            pRenderDeviceVk->SafeReleaseVkObject(UploadBuffer);
        }

        if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
        {
            m_CBVDescriptorAllocation = pRenderDeviceVk->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateCBV(m_CBVDescriptorAllocation.GetCpuHandle());
        }
    }
#endif
}


static BufferDesc BufferDescFromVkResource(BufferDesc BuffDesc, void *pVkBuffer)
{
#if 0
    VERIFY(BuffDesc.Usage != USAGE_DYNAMIC, "Dynamic buffers cannot be attached to native Vk resource");

    auto VkBuffDesc = pVkBuffer->GetDesc();
    VERIFY(VkBuffDesc.Dimension == Vk_RESOURCE_DIMENSION_BUFFER, "Vk resource is not a buffer");

    VERIFY(BuffDesc.uiSizeInBytes == 0 || BuffDesc.uiSizeInBytes == VkBuffDesc.Width, "Buffer size specified by the BufferDesc (", BuffDesc.uiSizeInBytes,") does not match Vk resource size (", VkBuffDesc.Width, ")" );
    BuffDesc.uiSizeInBytes = static_cast<Uint32>( VkBuffDesc.Width );

    if (VkBuffDesc.Flags & Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        VERIFY(BuffDesc.BindFlags == 0 || (BuffDesc.BindFlags & BIND_UNORDERED_ACCESS), "BIND_UNORDERED_ACCESS flag is not specified by the BufferDesc, while Vk resource was created with Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS flag");
        BuffDesc.BindFlags |= BIND_UNORDERED_ACCESS;
    }
    if (VkBuffDesc.Flags & Vk_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
    {
        VERIFY( !(BuffDesc.BindFlags & BIND_SHADER_RESOURCE), "BIND_SHADER_RESOURCE flag is specified by the BufferDesc, while Vk resource was created with Vk_RESOURCE_FLAG_DENY_SHADER_RESOURCE flag");
        BuffDesc.BindFlags &= ~BIND_SHADER_RESOURCE;
    }

    if( (BuffDesc.BindFlags & BIND_UNORDERED_ACCESS) || (BuffDesc.BindFlags & BIND_SHADER_RESOURCE) )
    {
        if(BuffDesc.Mode == BUFFER_MODE_STRUCTURED)
        {
            VERIFY(BuffDesc.ElementByteStride != 0, "Element byte stride cannot be 0 for a structured buffer");
        }
        else if(BuffDesc.Mode == BUFFER_MODE_FORMATTED)
        {
            VERIFY( BuffDesc.Format.ValueType != VT_UNDEFINED, "Value type is not specified for a formatted buffer" );
            VERIFY( BuffDesc.Format.NumComponents != 0, "Num components cannot be zero in a formated buffer" );
        }
        else
        {
            UNEXPECTED("Buffer mode must be structured or formatted");
        }
    }
#endif
    return BuffDesc;
}

BufferVkImpl :: BufferVkImpl(IReferenceCounters *pRefCounters, 
                                   FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                                   RenderDeviceVkImpl *pRenderDeviceVk, 
                                   const BufferDesc& BuffDesc,
                                   void *pVkBuffer) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceVk, BufferDescFromVkResource(BuffDesc, pVkBuffer), false)/*,
#ifdef _DEBUG
    m_DbgMapType(1 + pRenderDeviceVk->GetNumDeferredContexts(), std::make_pair(static_cast<MAP_TYPE>(-1), static_cast<Uint32>(-1)), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<pair<MAP_TYPE,Uint32>>")),
#endif
    m_DynamicData(BuffDesc.Usage == USAGE_DYNAMIC ? (1 + pRenderDeviceVk->GetNumDeferredContexts()) : 0, DynamicAllocation(), STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
    */
{
#if 0
    m_pVkResource = pVkBuffer;

    if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
    {
        m_CBVDescriptorAllocation = pRenderDeviceVk->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        CreateCBV(m_CBVDescriptorAllocation.GetCpuHandle());
    }
#endif
}
BufferVkImpl :: ~BufferVkImpl()
{
#if 0
    // Vk object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
    pDeviceVkImpl->SafeReleaseVkObject(m_pVkResource);
#endif
}

IMPLEMENT_QUERY_INTERFACE( BufferVkImpl, IID_BufferVk, TBufferBase )

void BufferVkImpl::UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )
{
    TBufferBase::UpdateData( pContext, Offset, Size, pData );

#if 0
    // We must use cmd context from the device context provided, otherwise there will
    // be resource barrier issues in the cmd list in the device context
    auto *pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pContext);
    pDeviceContextVk->UpdateBufferRegion(this, pData, Offset, Size);
#endif
}

void BufferVkImpl :: CopyData(IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size)
{
#if 0
    TBufferBase::CopyData( pContext, pSrcBuffer, SrcOffset, DstOffset, Size );
    auto *pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pContext);
    pDeviceContextVk->CopyBufferRegion(ValidatedCast<BufferVkImpl>(pSrcBuffer), this, SrcOffset, DstOffset, Size);
#endif
}

void BufferVkImpl :: Map(IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData)
{
    TBufferBase::Map( pContext, MapType, MapFlags, pMappedData );
#if 0
    auto *pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pContext);
#ifdef _DEBUG
    if(pDeviceContextVk != nullptr)
        m_DbgMapType[pDeviceContextVk->GetContextId()] = std::make_pair(MapType, MapFlags);
#endif
    if (MapType == MAP_READ )
    {
        LOG_WARNING_MESSAGE_ONCE("Mapping CPU buffer for reading on Vk currently requires flushing context and idling GPU");
        pDeviceContextVk->Flush();
        auto *pDeviceVk = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
        pDeviceVk->IdleGPU(false);

        VERIFY(m_Desc.Usage == USAGE_CPU_ACCESSIBLE, "Buffer must be created as USAGE_CPU_ACCESSIBLE to be mapped for reading");
        Vk_RANGE MapRange;
        MapRange.Begin = 0;
        MapRange.End = m_Desc.uiSizeInBytes;
        m_pVkResource->Map(0, &MapRange, &pMappedData);
    }
    else if(MapType == MAP_WRITE)
    {
        if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
        {
            VERIFY(m_pVkResource != nullptr, "USAGE_CPU_ACCESSIBLE buffer mapped for writing must intialize Vk resource");
            if (MapFlags & MAP_FLAG_DISCARD)
            {
                
            }
            m_pVkResource->Map(0, nullptr, &pMappedData);
        }
        else if (m_Desc.Usage == USAGE_DYNAMIC)
        {
            VERIFY(MapFlags & MAP_FLAG_DISCARD, "Vk buffer must be mapped for writing with MAP_FLAG_DISCARD flag");
            auto *pCtxVk = ValidatedCast<DeviceContextVkImpl>(pContext);
            auto ContextId = pDeviceContextVk->GetContextId();
            m_DynamicData[ContextId] = pCtxVk->AllocateDynamicSpace(m_Desc.uiSizeInBytes);
            pMappedData = m_DynamicData[ContextId].CPUAddress;
        }
        else
        {
            LOG_ERROR("Only USAGE_DYNAMIC and USAGE_CPU_ACCESSIBLE Vk buffers can be mapped for writing");
        }
    }
    else if(MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported on Vk");
    }
    else
    {
        LOG_ERROR("Only MAP_WRITE_DISCARD and MAP_READ are currently implemented in Vk");
    }
#endif
}

void BufferVkImpl::Unmap( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags )
{
    TBufferBase::Unmap( pContext, MapType, MapFlags );

#if 0
    auto *pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pContext);
    Uint32 CtxId = pDeviceContextVk != nullptr ? pDeviceContextVk->GetContextId() : static_cast<Uint32>(-1);
#ifdef _DEBUG
    if (pDeviceContextVk != nullptr)
    {
        VERIFY(m_DbgMapType[CtxId].first == MapType, "Map type does not match the type provided to Map()");
        VERIFY(m_DbgMapType[CtxId].second == MapFlags, "Map flags do not match the flags provided to Map()");
    }
#endif

    if (MapType == MAP_READ )
    {
        Vk_RANGE MapRange;
        // It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin.
        MapRange.Begin = 1;
        MapRange.End = 0;
        m_pVkResource->Unmap(0, &MapRange);
    }
    else if(MapType == MAP_WRITE)
    {
        if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
        {
            VERIFY(m_pVkResource != nullptr, "USAGE_CPU_ACCESSIBLE buffer mapped for writing must intialize Vk resource");
            m_pVkResource->Unmap(0, nullptr);
        }
        else if (m_Desc.Usage == USAGE_DYNAMIC)
        {
            VERIFY(MapFlags & MAP_FLAG_DISCARD, "Vk buffer must be mapped for writing with MAP_FLAG_DISCARD flag");
            // Copy data into the resource
            if (m_pVkResource)
            {
                pDeviceContextVk->UpdateBufferRegion(this, m_DynamicData[CtxId], 0, m_Desc.uiSizeInBytes);
            }
        }
    }

#ifdef _DEBUG
    if(pDeviceContextVk != nullptr)
        m_DbgMapType[CtxId] = std::make_pair(static_cast<MAP_TYPE>(-1), static_cast<Uint32>(-1));
#endif
#endif
}

void BufferVkImpl::CreateViewInternal( const BufferViewDesc &OrigViewDesc, IBufferView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;
#if 0
    try
    {
        auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
        auto &BuffViewAllocator = pDeviceVkImpl->GetBuffViewObjAllocator();
        VERIFY( &BuffViewAllocator == &m_dbgBuffViewAllocator, "Buff view allocator does not match allocator provided at buffer initialization" );

        BufferViewDesc ViewDesc = OrigViewDesc;
        if( ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS )
        {
            auto UAVHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateUAV( ViewDesc, UAVHandleAlloc.GetCpuHandle() );
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewVkImpl instance", BufferViewVkImpl, bIsDefaultView ? this : nullptr)
                                (GetDevice(), ViewDesc, this, std::move(UAVHandleAlloc), bIsDefaultView );
        }
        else if( ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE )
        {
			auto SRVHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CreateSRV( ViewDesc, SRVHandleAlloc.GetCpuHandle() );
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewVkImpl instance", BufferViewVkImpl, bIsDefaultView ? this : nullptr)
                                (GetDevice(), ViewDesc, this, std::move(SRVHandleAlloc), bIsDefaultView );
        }

        if( !bIsDefaultView && *ppView )
            (*ppView)->AddRef();
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name, "\"" );
    }
#endif
}

#if 0
void BufferVkImpl::CreateUAV( BufferViewDesc &UAVDesc, Vk_CPU_DESCRIPTOR_HANDLE UAVDescriptor )
{
    CorrectBufferViewDesc( UAVDesc );

    Vk_UNORDERED_ACCESS_VIEW_DESC Vk_UAVDesc;
    BufferViewDesc_to_Vk_UAV_DESC(m_Desc, UAVDesc, Vk_UAVDesc);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateUnorderedAccessView( m_pVkResource, nullptr, &Vk_UAVDesc, UAVDescriptor );
}

void BufferVkImpl::CreateSRV( struct BufferViewDesc &SRVDesc, Vk_CPU_DESCRIPTOR_HANDLE SRVDescriptor )
{
    CorrectBufferViewDesc( SRVDesc );

    Vk_SHADER_RESOURCE_VIEW_DESC Vk_SRVDesc;
    BufferViewDesc_to_Vk_SRV_DESC(m_Desc, SRVDesc, Vk_SRVDesc);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateShaderResourceView( m_pVkResource, &Vk_SRVDesc, SRVDescriptor );
}

void BufferVkImpl::CreateCBV(Vk_CPU_DESCRIPTOR_HANDLE CBVDescriptor)
{
    Vk_CONSTANT_BUFFER_VIEW_DESC Vk_CBVDesc;
    Vk_CBVDesc.BufferLocation = m_pVkResource->GetGPUVirtualAddress();
    Vk_CBVDesc.SizeInBytes = m_Desc.uiSizeInBytes;

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateConstantBufferView( &Vk_CBVDesc, CBVDescriptor );
}

#ifdef _DEBUG
void BufferVkImpl::DbgVerifyDynamicAllocation(Uint32 ContextId)
{
    VERIFY(m_DynamicData[ContextId].GPUAddress != 0, "Dynamic buffer must be mapped before the first use");
	auto CurrentFrame = ValidatedCast<RenderDeviceVkImpl>(GetDevice())->GetCurrentFrameNumber();
    VERIFY(m_DynamicData[ContextId].FrameNum == CurrentFrame, "Dynamic allocation is out-of-date. Dynamic buffer \"", m_Desc.Name, "\" must be mapped in the same frame it is used.");
    VERIFY(GetState() == Vk_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers are expected to always be in Vk_RESOURCE_STATE_GENERIC_READ state");
}
#endif

#endif
}
