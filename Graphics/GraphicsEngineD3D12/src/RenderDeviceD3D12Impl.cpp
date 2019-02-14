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
#include "FenceD3D12Impl.h"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceD3D12Impl :: RenderDeviceD3D12Impl(IReferenceCounters*          pRefCounters,
                                               IMemoryAllocator&            RawMemAllocator,
                                               const EngineD3D12Attribs&    CreationAttribs,
                                               ID3D12Device*                pd3d12Device,
                                               size_t                       CommandQueueCount,
                                               ICommandQueueD3D12**         ppCmdQueues, 
                                               Uint32                       NumDeferredContexts) : 
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        CommandQueueCount,
        ppCmdQueues,
        NumDeferredContexts,
        sizeof(TextureD3D12Impl),
        sizeof(TextureViewD3D12Impl),
        sizeof(BufferD3D12Impl),
        sizeof(BufferViewD3D12Impl),
        sizeof(ShaderD3D12Impl),
        sizeof(SamplerD3D12Impl),
        sizeof(PipelineStateD3D12Impl),
        sizeof(ShaderResourceBindingD3D12Impl),
        sizeof(FenceD3D12Impl)
    },
    m_pd3d12Device  (pd3d12Device),
    m_EngineAttribs (CreationAttribs),
    m_CmdListManager(*this),
    m_CPUDescriptorHeaps
    {
        {RawMemAllocator, *this, CreationAttribs.CPUDescriptorHeapAllocationSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, *this, CreationAttribs.CPUDescriptorHeapAllocationSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, *this, CreationAttribs.CPUDescriptorHeapAllocationSize[2], D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, *this, CreationAttribs.CPUDescriptorHeapAllocationSize[3], D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE}
    },
    m_GPUDescriptorHeaps
    {
        {RawMemAllocator, *this, CreationAttribs.GPUDescriptorHeapSize[0], CreationAttribs.GPUDescriptorHeapDynamicSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
        {RawMemAllocator, *this, CreationAttribs.GPUDescriptorHeapSize[1], CreationAttribs.GPUDescriptorHeapDynamicSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE}
    },
    m_ContextPool(STD_ALLOCATOR_RAW_MEM(PooledCommandContext, GetRawAllocator(), "Allocator for vector<PooledCommandContext>")),
    m_DynamicMemoryManager(GetRawAllocator(), *this, CreationAttribs.NumDynamicHeapPagesToReserve, CreationAttribs.DynamicHeapPageSize),
    m_MipsGenerator(pd3d12Device)
{
    m_DeviceCaps.DevType = DeviceType::D3D12;
    m_DeviceCaps.MajorVersion = 12;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;
}

RenderDeviceD3D12Impl::~RenderDeviceD3D12Impl()
{
    // Wait for the GPU to complete all its operations
    IdleGPU();
    ReleaseStaleResources(true);

#ifdef DEVELOPMENT
    for (auto i=0; i < _countof(m_CPUDescriptorHeaps); ++i)
    {
        DEV_CHECK_ERR(m_CPUDescriptorHeaps[i].DvpGetTotalAllocationCount() == 0, "All CPU descriptor heap allocations must be released");
    }
    for (auto i=0; i < _countof(m_GPUDescriptorHeaps); ++i)
    {
        DEV_CHECK_ERR(m_GPUDescriptorHeaps[i].DvpGetTotalAllocationCount() == 0, "All GPU descriptor heap allocations must be released");
    }
#endif

    DEV_CHECK_ERR(m_DynamicMemoryManager.GetAllocatedPageCounter() == 0, "All allocated dynamic pages must have been returned to the manager at this point.");
    m_DynamicMemoryManager.Destroy();
    DEV_CHECK_ERR(m_CmdListManager.GetAllocatorCounter() == 0, "All allocators must have been returned to the manager at this point.");
    DEV_CHECK_ERR(m_AllocatedCtxCounter == 0, "All contexts must have been released.");

	m_ContextPool.clear();
    DestroyCommandQueues();
}

void RenderDeviceD3D12Impl::DisposeCommandContext(PooledCommandContext&& Ctx)
{
	CComPtr<ID3D12CommandAllocator> pAllocator; 
    Ctx->Close(pAllocator);
    // Since allocator has not been used, we cmd list manager can put it directly into the free allocator list
    m_CmdListManager.FreeAllocator(std::move(pAllocator));
    FreeCommandContext(std::move(Ctx));
}

void RenderDeviceD3D12Impl::FreeCommandContext(PooledCommandContext&& Ctx)
{
	std::lock_guard<std::mutex> LockGuard(m_ContextPoolMutex);
    m_ContextPool.emplace_back(std::move(Ctx));
#ifdef DEVELOPMENT
    Atomics::AtomicDecrement(m_AllocatedCtxCounter);
#endif
}

void RenderDeviceD3D12Impl::CloseAndExecuteTransientCommandContext(Uint32 CommandQueueIndex, PooledCommandContext&& Ctx)
{
    CComPtr<ID3D12CommandAllocator> pAllocator;
    ID3D12GraphicsCommandList* pCmdList = Ctx->Close(pAllocator);
    Uint64 FenceValue = 0;
    // Execute command list directly through the queue to avoid interference with command list numbers in the queue
    LockCommandQueue(CommandQueueIndex, 
        [&](ICommandQueueD3D12* pCmdQueue)
        {
            FenceValue = pCmdQueue->Submit(pCmdList);
        }
    );
	m_CmdListManager.ReleaseAllocator(std::move(pAllocator), CommandQueueIndex, FenceValue);
    FreeCommandContext(std::move(Ctx));
}

Uint64 RenderDeviceD3D12Impl::CloseAndExecuteCommandContext(Uint32 QueueIndex, PooledCommandContext&& Ctx, bool DiscardStaleObjects, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >* pSignalFences)
{
    CComPtr<ID3D12CommandAllocator> pAllocator;
    ID3D12GraphicsCommandList* pCmdList = Ctx->Close(pAllocator);

    Uint64 FenceValue = 0;
    {
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
        auto SubmittedCmdBuffInfo = TRenderDeviceBase::SubmitCommandBuffer(QueueIndex, pCmdList, true);
        FenceValue = SubmittedCmdBuffInfo.FenceValue;
        if (pSignalFences != nullptr)
        {
            for (auto& val_fence : *pSignalFences)
            {
                auto* pFenceD3D12Impl = val_fence.second.RawPtr<FenceD3D12Impl>();
                auto* pd3d12Fence = pFenceD3D12Impl->GetD3D12Fence();
                m_CommandQueues[QueueIndex].CmdQueue->SignalFence(pd3d12Fence, val_fence.first);
            }
        }
    }

	m_CmdListManager.ReleaseAllocator(std::move(pAllocator), QueueIndex, FenceValue);
    FreeCommandContext(std::move(Ctx));

    PurgeReleaseQueue(QueueIndex);

    return FenceValue;
}


void RenderDeviceD3D12Impl::IdleGPU() 
{ 
    IdleCommandQueues(true);
    ReleaseStaleResources();
}

void RenderDeviceD3D12Impl::FlushStaleResources(Uint32 CmdQueueIndex)
{
    // Submit empty command list to the queue. This will effectively signal the fence and 
    // discard all resources
    ID3D12GraphicsCommandList* pNullCmdList = nullptr;
    TRenderDeviceBase::SubmitCommandBuffer(CmdQueueIndex, pNullCmdList, true);
}

void RenderDeviceD3D12Impl::ReleaseStaleResources(bool ForceRelease)
{
    PurgeReleaseQueues(ForceRelease);
}


RenderDeviceD3D12Impl::PooledCommandContext RenderDeviceD3D12Impl::AllocateCommandContext(const Char* ID)
{
    {
    	std::lock_guard<std::mutex> LockGuard(m_ContextPoolMutex);
        if (!m_ContextPool.empty())
        {
            PooledCommandContext Ctx = std::move(m_ContextPool.back());
            m_ContextPool.pop_back();
            Ctx->Reset(m_CmdListManager);
            Ctx->SetID(ID);
#ifdef DEVELOPMENT
            Atomics::AtomicIncrement(m_AllocatedCtxCounter);
#endif
            return Ctx;
        }
    }

    auto& CmdCtxAllocator = GetRawAllocator();
    auto* pRawMem = ALLOCATE(CmdCtxAllocator, "CommandContext instance", sizeof(CommandContext));
	auto pCtx = new (pRawMem) CommandContext(m_CmdListManager);
    pCtx->SetID(ID);
#ifdef DEVELOPMENT
    Atomics::AtomicIncrement(m_AllocatedCtxCounter);
#endif
    return PooledCommandContext(pCtx, CmdCtxAllocator);
}


void RenderDeviceD3D12Impl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto DXGIFormat = TexFormatToDXGI_Format(TexFormat);

    D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = {DXGIFormat};
    auto hr = m_pd3d12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
    if (FAILED(hr))
    {
        LOG_ERROR_MESSAGE("CheckFormatSupport() failed for format ", DXGIFormat);
        return;
    }

    TexFormatInfo.Filterable      = ((FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0) || 
                                    ((FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON) != 0);
    TexFormatInfo.ColorRenderable = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0;
    TexFormatInfo.DepthRenderable = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0;
    TexFormatInfo.Tex1DFmt        = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE1D) != 0;
    TexFormatInfo.Tex2DFmt        = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) != 0;
    TexFormatInfo.Tex3DFmt        = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D) != 0;
    TexFormatInfo.TexCubeFmt      = (FormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURECUBE) != 0;

    TexFormatInfo.SampleCounts = 0x0;
    for(Uint32 SampleCount = 1; SampleCount <= D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT; SampleCount *= 2)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS QualityLevels = {DXGIFormat, SampleCount};
        hr = m_pd3d12Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &QualityLevels, sizeof(QualityLevels));
        if(SUCCEEDED(hr) && QualityLevels.NumQualityLevels > 0)
            TexFormatInfo.SampleCounts |= SampleCount;
    }
}


IMPLEMENT_QUERY_INTERFACE( RenderDeviceD3D12Impl, IID_RenderDeviceD3D12, TRenderDeviceBase )

void RenderDeviceD3D12Impl::CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState** ppPipelineState)
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

void RenderDeviceD3D12Impl :: CreateBufferFromD3DResource(ID3D12Resource* pd3d12Buffer, const BufferDesc& BuffDesc, RESOURCE_STATE InitialState, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D12Impl *pBufferD3D12( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D12Impl instance", BufferD3D12Impl)(m_BuffViewObjAllocator, this, BuffDesc, InitialState, pd3d12Buffer ) );
            pBufferD3D12->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D12Impl *pBufferD3D12( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D12Impl instance", BufferD3D12Impl)(m_BuffViewObjAllocator, this, BuffDesc, pBuffData ) );
            pBufferD3D12->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D12 );
        } 
    );
}


void RenderDeviceD3D12Impl :: CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader** ppShader)
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

void RenderDeviceD3D12Impl::CreateTextureFromD3DResource(ID3D12Resource* pd3d12Texture, RESOURCE_STATE InitialState, ITexture** ppTexture)
{
    TextureDesc TexDesc;
    TexDesc.Name = "Texture from d3d12 resource";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl *pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, InitialState, pd3d12Texture);

            pTextureD3D12->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl::CreateTexture(const TextureDesc& TexDesc, ID3D12Resource* pd3d12Texture, RESOURCE_STATE InitialState, TextureD3D12Impl** ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl* pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, InitialState, pd3d12Texture);
            pTextureD3D12->QueryInterface( IID_TextureD3D12, reinterpret_cast<IObject**>(ppTexture) );
        }
    );
}

void RenderDeviceD3D12Impl :: CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureD3D12Impl* pTextureD3D12 = NEW_RC_OBJ(m_TexObjAllocator, "TextureD3D12Impl instance", TextureD3D12Impl)(m_TexViewObjAllocator, this, TexDesc, pData );

            pTextureD3D12->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D12->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D12 );
        } 
    );
}

void RenderDeviceD3D12Impl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)
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

void RenderDeviceD3D12Impl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject( "Fence", Desc, ppFence, 
        [&]()
        {
            FenceD3D12Impl* pFenceD3D12( NEW_RC_OBJ(m_FenceAllocator, "FenceD3D12Impl instance", FenceD3D12Impl)
                                                   (this, Desc) );
            pFenceD3D12->QueryInterface( IID_Fence, reinterpret_cast<IObject**>(ppFence) );
            OnCreateDeviceObject( pFenceD3D12 );
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
