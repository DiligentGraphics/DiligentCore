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
#include "RenderDeviceD3D12Impl.h"
#include "PipelineStateD3D12Impl.h"
#include "ShaderD3D12Impl.h"
#include "TextureD3D12Impl.h"
#include "DXGITypeConversions.h"
#include "SamplerD3D12Impl.h"
#include "BufferD3D12Impl.h"
#include "ShaderResourceBindingD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"

#include "EngineMemory.h"
namespace Diligent
{

RenderDeviceD3D12Impl :: RenderDeviceD3D12Impl(IReferenceCounters *pRefCounters, IMemoryAllocator &RawMemAllocator, const EngineD3D12Attribs &CreationAttribs, ID3D12Device *pd3d12Device, ICommandQueueD3D12 *pCmdQueue, Uint32 NumDeferredContexts) : 
    TRenderDeviceBase(pRefCounters, RawMemAllocator, NumDeferredContexts, sizeof(TextureD3D12Impl), sizeof(TextureViewD3D12Impl), sizeof(BufferD3D12Impl), sizeof(BufferViewD3D12Impl), sizeof(ShaderD3D12Impl), sizeof(SamplerD3D12Impl), sizeof(PipelineStateD3D12Impl), sizeof(ShaderResourceBindingD3D12Impl)),
    m_pd3d12Device(pd3d12Device),
    m_pCommandQueue(pCmdQueue),
    m_EngineAttribs(CreationAttribs),
	m_FrameNumber(0),
    m_NextCmdListNumber(0),
    m_CmdListManager(this),
    m_CPUDescriptorHeaps
    {
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[2], D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[3], D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE}
    },
    m_GPUDescriptorHeaps
    {
        {RawMemAllocator, this, CreationAttribs.GPUDescriptorHeapSize[0], CreationAttribs.GPUDescriptorHeapDynamicSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
        {RawMemAllocator, this, CreationAttribs.GPUDescriptorHeapSize[1], CreationAttribs.GPUDescriptorHeapDynamicSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE}
    },
	m_DynamicDescriptorAllocationChunkSize
	{
		CreationAttribs.DynamicDescriptorAllocationChunkSize[0], // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		CreationAttribs.DynamicDescriptorAllocationChunkSize[1]  // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
	},
    m_ContextPool(STD_ALLOCATOR_RAW_MEM(ContextPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<CommandContext>>")),
    m_AvailableContexts(STD_ALLOCATOR_RAW_MEM(CommandContext*, GetRawAllocator(), "Allocator for vector<CommandContext*>")),
    m_D3D12ObjReleaseQueue(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, GetRawAllocator(), "Allocator for queue<ReleaseQueueElemType>")),
    m_StaleD3D12Objects(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, GetRawAllocator(), "Allocator for queue<ReleaseQueueElemType>")),
    m_UploadHeaps(STD_ALLOCATOR_RAW_MEM(UploadHeapPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<DynamicUploadHeap>>"))
{
    m_DeviceCaps.DevType = DeviceType::D3D12;
    m_DeviceCaps.MajorVersion = 12;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;
}

RenderDeviceD3D12Impl::~RenderDeviceD3D12Impl()
{
	// Finish current frame. This will release resources taken by previous frames, and
    // will move all stale resources to the release queues. The resources will not be
    // release until next call to FinishFrame()
    FinishFrame();
    // Wait for the GPU to complete all its operations
    IdleGPU(true);
    // Call FinishFrame() again to destroy resources in
    // release queues
    FinishFrame(true);
    
	m_ContextPool.clear();
}

void RenderDeviceD3D12Impl::DisposeCommandContext(CommandContext* pCtx)
{
	std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
    m_AvailableContexts.push_back(pCtx);
}

void RenderDeviceD3D12Impl::CloseAndExecuteCommandContext(CommandContext *pCtx, bool DiscardStaleObjects)
{
    CComPtr<ID3D12CommandAllocator> pAllocator;
	auto *pCmdList = pCtx->Close(&pAllocator);

    Uint64 FenceValue = 0;
    Uint64 CmdListNumber = 0;
    {
	    std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        auto NextFenceValue = m_pCommandQueue->GetNextFenceValue();
	    // Submit the command list to the queue
        FenceValue = m_pCommandQueue->ExecuteCommandList(pCmdList);
        VERIFY(FenceValue >= NextFenceValue, "Fence value of the executed command list is less than the next fence value previously queried through GetNextFenceValue()");
        FenceValue = std::max(FenceValue, NextFenceValue);
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }

    if (DiscardStaleObjects)
    {
        // The following basic requirement guarantees correctness of resource deallocation:
        //
        //        A resource is never released before the last draw command referencing it is invoked on the immediate context
        //
        // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/

        // Stale objects should only be discarded when submitting cmd list from 
        // the immediate context, otherwise the basic requirement may be violated
        // as in the following scenario
        //                                                           
        //  Signaled        |                                        |
        //  Fence Value     |        Immediate Context               |            InitContext            |
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
        //                  |     with number N                      |                                   |

        // Move stale objects into a release queue.
        // Note that objects are moved from stale list to release queue based on the
        // cmd list number, not the fence value. This makes sure that basic requirement
        // is met even when the fence value is not incremented while executing 
        // the command list (as is the case with Unity command queue).
        DiscardStaleD3D12Objects(CmdListNumber, FenceValue);
    }

    // DiscardAllocator() is thread-safe
	m_CmdListManager.DiscardAllocator(FenceValue, pAllocator);
    
    pCtx->DiscardDynamicDescriptors(FenceValue);

    {
	    std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
    	m_AvailableContexts.push_back(pCtx);
    }
}


void RenderDeviceD3D12Impl::IdleGPU(bool ReleaseStaleObjects) 
{ 
    Uint64 FenceValue = 0;
    Uint64 CmdListNumber = 0;
    {
        // Lock the command queue to avoid other threads interfering with the GPU
        std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        FenceValue = m_pCommandQueue->GetNextFenceValue();
        m_pCommandQueue->IdleGPU();
        // Increment cmd list number while keeping queue locked. 
        // This guarantees that any D3D12 object released after the lock
        // is released, will be associated with the incremented cmd list number
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }
    
    if (ReleaseStaleObjects)
    {
        // Do not wait until the end of the frame and force deletion. 
        // This is necessary to release outstanding references to the
        // swap chain buffers when it is resized in the middle of the frame.
        // Since GPU has been idled, it it is safe to do so
        DiscardStaleD3D12Objects(CmdListNumber, FenceValue);
        ProcessReleaseQueue(FenceValue);
    }
}

Bool RenderDeviceD3D12Impl::IsFenceSignaled(Uint64 FenceValue) 
{
    return FenceValue <= GetCompletedFenceValue();
}

Uint64 RenderDeviceD3D12Impl::GetCompletedFenceValue()
{
    return m_pCommandQueue->GetCompletedFenceValue();
}

void RenderDeviceD3D12Impl::FinishFrame(bool ReleaseAllResources)
{
    {
        if (auto pImmediateCtx = m_wpImmediateContext.Lock())
        {
            auto pImmediateCtxD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pImmediateCtx.RawPtr());
            if(pImmediateCtxD3D12->GetNumCommandsInCtx() != 0)
                LOG_ERROR_MESSAGE("There are outstanding commands in the immediate device context when finishing the frame. This is an error and may cause unpredicted behaviour. Call Flush() to submit all commands for execution before finishing the frame")
        }

        for (auto wpDeferredCtx : m_wpDeferredContexts)
        {
            if (auto pDeferredCtx = wpDeferredCtx.Lock())
            {
                auto pDeferredCtxD3D12 = ValidatedCast<DeviceContextD3D12Impl>(pDeferredCtx.RawPtr());
                if(pDeferredCtxD3D12->GetNumCommandsInCtx() != 0)
                    LOG_ERROR_MESSAGE("There are outstanding commands in the deferred device context when finishing the frame. This is an error and may cause unpredicted behaviour. Close all deferred contexts and execute them before finishing the frame")
            }
        }
    }
    
    auto CompletedFenceValue = ReleaseAllResources ? std::numeric_limits<Uint64>::max() : GetCompletedFenceValue();

    // We must use NextFenceValue here, NOT current value, because the 
    // fence value may or may not have been incremented when the last 
    // command list was submitted for execution (Unity only
    // increments fence value once per frame)
    Uint64 NextFenceValue = 0;
    Uint64 CmdListNumber = 0;
    {
        // Lock the command queue to avoid other threads interfering with the GPU
        std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        NextFenceValue = m_pCommandQueue->GetNextFenceValue();
        // Increment cmd list number while keeping queue locked. 
        // This guarantees that any D3D12 object released after the lock
        // is released, will be associated with the incremented cmd list number
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }

    {
        // There is no need to lock as new heaps are only created during initialization
        // time for every context
        //std::lock_guard<std::mutex> LockGuard(m_UploadHeapMutex);
        
        // Upload heaps are used to update resource contents as well as to allocate
        // space for dynamic resources.
        // Initial resource data is uploaded using temporary one-time upload buffers, 
        // so can be performed in parallel across frame boundaries
        for (auto &UploadHeap : m_UploadHeaps)
        {
            // Currently upload heaps are free-threaded, so other threads must not allocate
            // resources at the same time. This means that all dynamic buffers must be unmaped 
            // in the same frame and all resources must be updated within boundaries of a single frame.
            //
            //    worker thread 3    | pDevice->CrateTexture(InitData) |    | pDevice->CrateBuffer(InitData) |    | pDevice->CrateTexture(InitData) |
            //                                                                                               
            //    worker thread 2     | pDfrdCtx2->UpdateResource()  |                                              ||
            //                                                                                                      ||
            //    worker thread 1       |  pDfrdCtx1->Map(WRITE_DISCARD) |    | pDfrdCtx1->UpdateResource()  |      ||
            //                                                                                                      ||
            //    main thread        |  pCtx->Map(WRITE_DISCARD )|  | pCtx->UpdateResource()  |                     ||   | Present() |
            //
            //
            
            UploadHeap->FinishFrame(NextFenceValue, CompletedFenceValue);
        }
    }

    for(Uint32 CPUHeap=0; CPUHeap < _countof(m_CPUDescriptorHeaps); ++CPUHeap)
    {
        // This is OK if other thread disposes descriptor heap allocation at this time
        // The allocation will be registered as part of the current frame
        m_CPUDescriptorHeaps[CPUHeap].ReleaseStaleAllocations(CompletedFenceValue);
    }
        
    for(Uint32 GPUHeap=0; GPUHeap < _countof(m_GPUDescriptorHeaps); ++GPUHeap)
    {
        m_GPUDescriptorHeaps[GPUHeap].ReleaseStaleAllocations(CompletedFenceValue);
    }

    // Discard all remaining objects. This is important to do if there were 
    // no command lists submitted during the frame
    DiscardStaleD3D12Objects(CmdListNumber, NextFenceValue);
    ProcessReleaseQueue(CompletedFenceValue);

    Atomics::AtomicIncrement(m_FrameNumber);
}

DynamicUploadHeap* RenderDeviceD3D12Impl::RequestUploadHeap()
{
    std::lock_guard<std::mutex> LockGuard(m_UploadHeapMutex);

#ifdef _DEBUG
    size_t InitialSize = 1024+64;
#else
    size_t InitialSize = 64<<10;//16<<20;
#endif

    auto &UploadHeapAllocator = GetRawAllocator();
    auto *pRawMem = ALLOCATE(UploadHeapAllocator, "DynamicUploadHeap instance", sizeof(DynamicUploadHeap));
    auto *pNewHeap = new (pRawMem) DynamicUploadHeap(GetRawAllocator(), true, this, InitialSize);
    m_UploadHeaps.emplace_back( pNewHeap, STDDeleterRawMem<DynamicUploadHeap>(UploadHeapAllocator) );
    return pNewHeap;
}

void RenderDeviceD3D12Impl::ReleaseUploadHeap(DynamicUploadHeap* pUploadHeap)
{

}

CommandContext* RenderDeviceD3D12Impl::AllocateCommandContext(const Char *ID)
{
	std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);

	CommandContext* ret = nullptr;
	if (m_AvailableContexts.empty())
	{
        auto &CmdCtxAllocator = GetRawAllocator();
        auto *pRawMem = ALLOCATE(CmdCtxAllocator, "CommandContext instance", sizeof(CommandContext));
		ret = new (pRawMem) CommandContext( GetRawAllocator(), m_CmdListManager, m_GPUDescriptorHeaps, m_DynamicDescriptorAllocationChunkSize);
		m_ContextPool.emplace_back(ret, STDDeleterRawMem<CommandContext>(CmdCtxAllocator) );
	}
	else
	{
		ret = m_AvailableContexts.front();
		m_AvailableContexts.pop_front();
		ret->Reset(m_CmdListManager);
	}
	VERIFY_EXPR(ret != nullptr);
	ret->SetID(ID);
	//if ( ID != nullptr && *ID != 0 )
	//	EngineProfiling::BeginBlock(ID, NewContext);

	return ret;
}

void RenderDeviceD3D12Impl::SafeReleaseD3D12Object(ID3D12Object* pObj)
{
    // When D3D12 object is released, it is first moved into the
    // stale objects list. The list is moved into a release queue
    // when the next command list is executed. 
    std::lock_guard<std::mutex> LockGuard(m_StaleObjectsMutex);
    m_StaleD3D12Objects.emplace_back( m_NextCmdListNumber, CComPtr<ID3D12Object>(pObj) );
}

void RenderDeviceD3D12Impl::DiscardStaleD3D12Objects(Uint64 CmdListNumber, Uint64 FenceValue)
{
    // Only discard these stale objects that were released before CmdListNumber
    // was executed
    std::lock_guard<std::mutex> StaleObjectsLock(m_StaleObjectsMutex);
    std::lock_guard<std::mutex> ReleaseQueueLock(m_ReleaseQueueMutex);
    while (!m_StaleD3D12Objects.empty() )
    {
        auto &FirstStaleObj = m_StaleD3D12Objects.front();
        if (FirstStaleObj.first <= CmdListNumber)
        {
            m_D3D12ObjReleaseQueue.emplace_back(FenceValue, std::move(FirstStaleObj.second));
            m_StaleD3D12Objects.pop_front();
        }
        else 
            break;
    }
}

void RenderDeviceD3D12Impl::ProcessReleaseQueue(Uint64 CompletedFenceValue)
{
    std::lock_guard<std::mutex> LockGuard(m_ReleaseQueueMutex);

    // Release all objects whose associated fence value is at most CompletedFenceValue
    // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
    while (!m_D3D12ObjReleaseQueue.empty())
    {
        auto &FirstObj = m_D3D12ObjReleaseQueue.front();
        if (FirstObj.first <= CompletedFenceValue)
            m_D3D12ObjReleaseQueue.pop_front();
        else
            break;
    }
}

bool CreateTestResource(ID3D12Device *pDevice, const D3D12_RESOURCE_DESC &ResDesc)
{
    // Set the texture pointer address to nullptr to validate input parameters
    // without creating the texture
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dn899178(v=vs.85).aspx

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;
        
    auto hr = pDevice->CreateCommittedResource( &HeapProps, D3D12_HEAP_FLAG_NONE, &ResDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, __uuidof(ID3D12Resource), nullptr );
    return hr == S_FALSE; // S_FALSE means that input parameters passed validation
}

void RenderDeviceD3D12Impl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto DXGIFormat = TexFormatToDXGI_Format(TexFormat);
    D3D12_RESOURCE_FLAGS DefaultResourceFlags = D3D12_RESOURCE_FLAG_NONE;
    if( TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
        TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL )
        DefaultResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    const int TestTextureDim = 32;
    const int TestTextureDepth = 8;
    
    D3D12_RESOURCE_DESC ResDesc = 
    {
        D3D12_RESOURCE_DIMENSION_TEXTURE1D,
        0, // Alignment
        TestTextureDim,
        1, // Height
        1, // DepthOrArraySize
        1, // MipLevels
        DXGIFormat,
        {1, 0},
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        DefaultResourceFlags
    };

    // Create test texture 1D
    TexFormatInfo.Tex1DFmt = false;
    if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED )
    {
        TexFormatInfo.Tex1DFmt = CreateTestResource(m_pd3d12Device, ResDesc );
    }

    // Create test texture 2D
    TexFormatInfo.Tex2DFmt = false;
    TexFormatInfo.TexCubeFmt = false;
    TexFormatInfo.ColorRenderable = false;
    TexFormatInfo.DepthRenderable = false;
    TexFormatInfo.SupportsMS = false;
    {
        ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ResDesc.Height = TestTextureDim;
        TexFormatInfo.Tex2DFmt = CreateTestResource( m_pd3d12Device, ResDesc );

        if( TexFormatInfo.Tex2DFmt )
        {
            {
            //    D3D12_TEXTURE2D_DESC CubeTexDesc = Tex2DDesc;
                  ResDesc.DepthOrArraySize = 6;
            //    CubeTexDesc.MiscFlags = D3D12_RESOURCE_MISC_TEXTURECUBE;
                  TexFormatInfo.TexCubeFmt = CreateTestResource( m_pd3d12Device, ResDesc );
                  ResDesc.DepthOrArraySize = 1;
            }

            if( TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
                TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL )
            {
                ResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                ResDesc.SampleDesc.Count = 1;
                TexFormatInfo.DepthRenderable = CreateTestResource( m_pd3d12Device, ResDesc );

                if( TexFormatInfo.DepthRenderable )
                {
                    ResDesc.SampleDesc.Count = 4;
                    TexFormatInfo.SupportsMS = CreateTestResource( m_pd3d12Device, ResDesc );
                }
            }
            else if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED && 
                     TexFormatInfo.Format != DXGI_FORMAT_R9G9B9E5_SHAREDEXP )
            {
                ResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                ResDesc.SampleDesc.Count = 1;
                TexFormatInfo.ColorRenderable = CreateTestResource( m_pd3d12Device, ResDesc );
                if( TexFormatInfo.ColorRenderable )
                {
                    ResDesc.SampleDesc.Count = 4;
                    TexFormatInfo.SupportsMS = CreateTestResource( m_pd3d12Device, ResDesc );
                }
            }
        }
    }

    // Create test texture 3D
    TexFormatInfo.Tex3DFmt = false;
    // 3D textures do not support depth formats
    if( !(TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
          TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL) )
    {
        ResDesc.SampleDesc.Count = 1;
        ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        ResDesc.Flags = DefaultResourceFlags;
        ResDesc.DepthOrArraySize = TestTextureDepth;
        TexFormatInfo.Tex3DFmt = CreateTestResource( m_pd3d12Device, ResDesc );
    }
}


IMPLEMENT_QUERY_INTERFACE( RenderDeviceD3D12Impl, IID_RenderDeviceD3D12, TRenderDeviceBase )

void RenderDeviceD3D12Impl::CreatePipelineState(const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState)
{
    CreateDeviceObject("Pipeline State", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateD3D12Impl *pPipelineStateD3D12( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateD3D12Impl instance", PipelineStateD3D12Impl)(this, PipelineDesc ) );
            pPipelineStateD3D12->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl :: CreateBufferFromD3DResource(ID3D12Resource *pd3d12Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D12Impl *pBufferD3D12( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D12Impl instance", BufferD3D12Impl)(m_BuffViewObjAllocator, this, BuffDesc, pd3d12Buffer ) );
            pBufferD3D12->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D12Impl *pBufferD3D12( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D12Impl instance", BufferD3D12Impl)(m_BuffViewObjAllocator, this, BuffDesc, BuffData ) );
            pBufferD3D12->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D12 );
        } 
    );
}


void RenderDeviceD3D12Impl :: CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)
{
    CreateDeviceObject( "shader", ShaderCreationAttribs.Desc, ppShader, 
        [&]()
        {
            ShaderD3D12Impl *pShaderD3D12( NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderD3D12Impl instance", ShaderD3D12Impl)(this, ShaderCreationAttribs ) );
            pShaderD3D12->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl::CreateTextureFromD3DResource(ID3D12Resource *pd3d12Texture, ITexture **ppTexture)
{
    TextureDesc TexDesc;
    TexDesc.Name = "Texture from d3d12 resource";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl *pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, pd3d12Texture );

            pTextureD3D12->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl::CreateTexture(const TextureDesc& TexDesc, ID3D12Resource *pd3d12Texture, class TextureD3D12Impl **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl *pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, pd3d12Texture );
            pTextureD3D12->QueryInterface( IID_TextureD3D12, reinterpret_cast<IObject**>(ppTexture) );
        }
    );
}

void RenderDeviceD3D12Impl :: CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl *pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, Data );

            pTextureD3D12->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if( *ppSampler == nullptr )
            {
                SamplerD3D12Impl *pSamplerD3D12( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerD3D12Impl instance", SamplerD3D12Impl)(this, SamplerDesc ) );
                pSamplerD3D12->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerD3D12 );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

DescriptorHeapAllocation RenderDeviceD3D12Impl :: AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count /*= 1*/)
{
    VERIFY(Type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES, "Invalid heap type");
    return m_CPUDescriptorHeaps[Type].Allocate(Count);
}

DescriptorHeapAllocation RenderDeviceD3D12Impl :: AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count /*= 1*/)
{
    VERIFY(Type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Invalid heap type");
    return m_GPUDescriptorHeaps[Type].Allocate(Count);
}

}
