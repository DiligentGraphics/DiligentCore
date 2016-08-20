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

RenderDeviceD3D12Impl :: RenderDeviceD3D12Impl(IMemoryAllocator &RawMemAllocator, const EngineD3D12Attribs &CreationAttribs, ID3D12Device *pd3d12Device, ID3D12CommandQueue *pd3d12CmdQueue, Uint32 NumDeferredContexts) : 
    TRenderDeviceBase(RawMemAllocator, NumDeferredContexts, sizeof(TextureD3D12Impl), sizeof(TextureViewD3D12Impl), sizeof(BufferD3D12Impl), sizeof(BufferViewD3D12Impl), sizeof(ShaderD3D12Impl), sizeof(SamplerD3D12Impl), sizeof(PipelineStateD3D12Impl), sizeof(ShaderResourceBindingD3D12Impl)),
    m_pd3d12Device(pd3d12Device),
    m_pd3d12CmdQueue(pd3d12CmdQueue),
    m_EngineAttribs(CreationAttribs),
    m_NextFenceValue(1),
    m_LastCompletedFenceValue(0),
    m_FenceEventHandle( CreateEvent(nullptr, false, false, nullptr) ),
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
    m_ContextPool(STD_ALLOCATOR_RAW_MEM(ContextPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<CommandContext>>")),
    m_AvailableContexts(STD_ALLOCATOR_RAW_MEM(CommandContext*, GetRawAllocator(), "Allocator for vector<CommandContext*>")),
    m_D3D12ObjReleaseQueue(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, GetRawAllocator(), "Allocator for queue<ReleaseQueueElemType>")),
    m_UploadHeaps(STD_ALLOCATOR_RAW_MEM(UploadHeapPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<DynamicUploadHeap>>"))
{
    m_DeviceCaps.DevType = DeviceType::D3D12;
    m_DeviceCaps.MajorVersion = 12;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;

    VERIFY_EXPR(m_FenceEventHandle != INVALID_HANDLE_VALUE);

    auto hr = pd3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(m_pFence), reinterpret_cast<void**>(static_cast<ID3D12Fence**>(&m_pFence)));
    VERIFY(SUCCEEDED(hr), "Failed to create fence");
	m_pFence->SetName(L"CommandListManager::m_pFence");
	m_pFence->Signal(m_LastCompletedFenceValue);

    hr = pd3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(m_pNumCompletedFramesFence), reinterpret_cast<void**>(static_cast<ID3D12Fence**>(&m_pNumCompletedFramesFence)));
    VERIFY(SUCCEEDED(hr), "Failed to create fence");
	m_pNumCompletedFramesFence->SetName(L"Last completed frame fence");
	m_pNumCompletedFramesFence->Signal(m_NumCompletedFrames);
}

RenderDeviceD3D12Impl::~RenderDeviceD3D12Impl()
{
    // Finish current frame. This will release resources taken by previous frames, and
    // will signal current frame as completed. If we do not call FinishFrame(), then
    // current frame will not be signaled as completed and the second call to FinishFrame()
    // will not release current frame's resources.
    FinishFrame();
    // Wait for the GPU to complete all its operations
    IdleGPU();
    // Call FinishFrame() again to release all resources associated with the current frame
    FinishFrame();
    CloseHandle(m_FenceEventHandle);

	//LinearAllocator::DestroyAll();
	//DynamicDescriptorHeap::DestroyAll();
	m_ContextPool.clear();
}

void RenderDeviceD3D12Impl::DisposeCommandContext(CommandContext* pCtx)
{
	std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
    m_AvailableContexts.push_back(pCtx);
}

Uint64 RenderDeviceD3D12Impl::CloseAndExecuteCommandContext(CommandContext *pCtx)
{
    CComPtr<ID3D12CommandAllocator> pAllocator;
	auto *pCmdList = pCtx->Close(&pAllocator);

	uint64_t FenceValue = 0;
    {
	    std::lock_guard<std::mutex> LockGuard(m_FenceMutex);

	    // Kickoff the command list
        ID3D12CommandList *const ppCmdLists[] = {pCmdList};
	    m_pd3d12CmdQueue->ExecuteCommandLists(1, ppCmdLists);

	    // Signal the next fence value (with the GPU)
	    m_pd3d12CmdQueue->Signal(m_pFence, m_NextFenceValue);

	    // And increment the fence value.  
	    FenceValue = m_NextFenceValue++;
    }

	m_CmdListManager.DiscardAllocator(FenceValue, pAllocator);
    pCtx->DiscardUsedDescriptorHeaps(m_CurrentFrame);

    {
	    std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
    	m_AvailableContexts.push_back(pCtx);
    }

	return FenceValue;
}


Uint64 RenderDeviceD3D12Impl::IncrementFence(void)
{
	std::lock_guard<std::mutex> LockGuard(m_FenceMutex);
    m_pd3d12CmdQueue->Signal(m_pFence, m_NextFenceValue);
	return m_NextFenceValue++;
}

void RenderDeviceD3D12Impl::IdleGPU(bool ReleasePendingObjects) 
{ 
    auto FenceValue = IncrementFence();
    WaitForFence(FenceValue); 
    if(ReleasePendingObjects)
    {
        // Do not wait until the end of the frame and force deletion. 
        // This is necessary to release outstanding references to the
        // swap chain buffers when it is resized in the middle of the frame.
        // Since GPU has been idled, it is safe to do so
        ProcessReleaseQueue(true);
    }
}


bool RenderDeviceD3D12Impl::IsFenceComplete(Uint64 FenceValue)
{
	// Avoid querying the fence value by testing against the last one seen.
	// The max() is to protect against an unlikely race condition that could cause the last
	// completed fence value to regress.
	if (FenceValue > m_LastCompletedFenceValue)
		m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_pFence->GetCompletedValue());

	return FenceValue <= m_LastCompletedFenceValue;
}

Uint64 RenderDeviceD3D12Impl::FinishFrame()
{
    m_NumCompletedFrames = std::max(m_NumCompletedFrames, m_pNumCompletedFramesFence->GetCompletedValue());
    for (auto &UploadHeap : m_UploadHeaps)
    {
        // Currently upload heaps are free-threaded, so other threads must not allocate
        // resources at the same time
        UploadHeap->FinishFrame(m_CurrentFrame, m_NumCompletedFrames);
    }

    for(Uint32 CPUHeap=0; CPUHeap < _countof(m_CPUDescriptorHeaps); ++CPUHeap)
    {
        m_CPUDescriptorHeaps[CPUHeap].ReleaseStaleAllocations(m_NumCompletedFrames);
    }
        
    for(Uint32 GPUHeap=0; GPUHeap < _countof(m_GPUDescriptorHeaps); ++GPUHeap)
    {
        m_GPUDescriptorHeaps[GPUHeap].ReleaseStaleAllocations(m_NumCompletedFrames);
    }


    ProcessReleaseQueue();

    // How does this work in a multi-threaded environment??
    auto CompletedFrame = m_CurrentFrame++;
    m_pd3d12CmdQueue->Signal(m_pNumCompletedFramesFence, m_CurrentFrame);
	return CompletedFrame;
}

bool RenderDeviceD3D12Impl::IsFrameComplete(Uint64 Frame)
{
	// Avoid querying the fence value by testing against the last one seen.
	// The max() is to protect against an unlikely race condition that could cause the last
	// completed fence value to regress.
	if (Frame > m_NumCompletedFrames)
		m_NumCompletedFrames = std::max(m_NumCompletedFrames, m_pNumCompletedFramesFence->GetCompletedValue());

	return Frame <= m_NumCompletedFrames;
}

void RenderDeviceD3D12Impl::WaitForFence(Uint64 FenceValue)
{
	if (IsFenceComplete(FenceValue))
		return;

	// TODO:  Think about how this might affect a multi-threaded situation.  Suppose thread A
	// wants to wait for fence 100, then thread B comes along and wants to wait for 99.  If
	// the fence can only have one event set on completion, then thread B has to wait for 
	// 100 before it knows 99 is ready.  Maybe insert sequential events?
	{
		std::lock_guard<std::mutex> LockGuard(m_EventMutex);

		m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
		WaitForSingleObject(m_FenceEventHandle, INFINITE);
		m_LastCompletedFenceValue = FenceValue;
	}
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
        Uint32 DynamicDescriptorAllocationChunkSize[] = {256, 32};
        auto &CmdCtxAllocator = GetRawAllocator();
        auto *pRawMem = ALLOCATE(CmdCtxAllocator, "CommandContext instance", sizeof(CommandContext));
		ret = new (pRawMem) CommandContext( GetRawAllocator(), m_CmdListManager, m_GPUDescriptorHeaps, DynamicDescriptorAllocationChunkSize);
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
    std::lock_guard<std::mutex> LockGuard(m_ReleasedObjectsMutex);
    m_D3D12ObjReleaseQueue.emplace_back( m_CurrentFrame, CComPtr<ID3D12Object>(pObj) );
}

void RenderDeviceD3D12Impl::ProcessReleaseQueue(bool ForceRelease)
{
    std::lock_guard<std::mutex> LockGuard(m_ReleasedObjectsMutex);

    // Release all objects whose frame number value < number of completed frames
    while (m_D3D12ObjReleaseQueue.size())
    {
        auto &FirstObj = m_D3D12ObjReleaseQueue.front();
        // GPU must have been idled when ForceRelease == true 
        if (FirstObj.first < m_NumCompletedFrames || ForceRelease)
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
            PipelineStateD3D12Impl *pPipelineStateD3D12( NEW(m_PSOAllocator, "PipelineStateD3D12Impl instance", PipelineStateD3D12Impl, this, PipelineDesc ) );
            pPipelineStateD3D12->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateD3D12 );
        } 
    );
}


void RenderDeviceD3D12Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D12Impl *pBufferD3D12( NEW(m_BufObjAllocator, "BufferD3D12Impl instance", BufferD3D12Impl, m_BuffViewObjAllocator, this, BuffDesc, BuffData ) );
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
            ShaderD3D12Impl *pShaderD3D12( NEW(m_ShaderObjAllocator, "ShaderD3D12Impl instance", ShaderD3D12Impl, this, ShaderCreationAttribs ) );
            pShaderD3D12->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl::CreateTexture(TextureDesc& TexDesc, ID3D12Resource *pd3d12Texture, class TextureD3D12Impl **ppTexture)
{
    TextureD3D12Impl *pTextureD3D12 = NEW(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl, m_TexViewObjAllocator, this, TexDesc, pd3d12Texture );
    pTextureD3D12->QueryInterface( IID_TextureD3D12, reinterpret_cast<IObject**>(ppTexture) );
}

void RenderDeviceD3D12Impl :: CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl *pTextureD3D12 = NEW(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl, m_TexViewObjAllocator, this, TexDesc, Data );

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
                SamplerD3D12Impl *pSamplerD3D12( NEW(m_SamplerObjAllocator, "SamplerD3D12Impl instance", SamplerD3D12Impl, this, SamplerDesc ) );
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
