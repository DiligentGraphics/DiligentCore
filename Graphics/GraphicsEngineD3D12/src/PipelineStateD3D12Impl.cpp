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
#include "PipelineStateD3D12Impl.h"
#include "ShaderD3D12Impl.h"
#include "D3D12TypeConversions.h"
#include "RenderDeviceD3D12Impl.h"
#include "DXGITypeConversions.h"
#include "ShaderResourceBindingD3D12Impl.h"
#include "CommandContext.h"
#include "EngineMemory.h"
#include "StringTools.h"

namespace Diligent
{

D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType_To_D3D12_PRIMITIVE_TOPOLOGY_TYPE( PRIMITIVE_TOPOLOGY_TYPE TopologyType )
{
    static bool bIsInit = false;
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE D3D12TopologyType[PRIMITIVE_TOPOLOGY_TYPE_NUM_TYPES] = {};
    if( !bIsInit )
    {
        D3D12TopologyType[ PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED] =  D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        D3D12TopologyType[ PRIMITIVE_TOPOLOGY_TYPE_POINT    ] =  D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        D3D12TopologyType[ PRIMITIVE_TOPOLOGY_TYPE_LINE     ] =  D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;  
        D3D12TopologyType[ PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ] =  D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        D3D12TopologyType[ PRIMITIVE_TOPOLOGY_TYPE_PATCH    ] =  D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

        bIsInit = true;
    }

    if( TopologyType >= PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED && TopologyType < PRIMITIVE_TOPOLOGY_TYPE_NUM_TYPES )
    {
        auto d3d12TopType = D3D12TopologyType[TopologyType];
        return d3d12TopType;
    }
    else
    {
        UNEXPECTED( "Incorrect topology type operation (", TopologyType, ")" )
        return static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(0);
    }
}

void PipelineStateD3D12Impl::ParseShaderResourceLayout(IShader *pShader)
{
    VERIFY_EXPR(pShader);

    auto ShaderType = pShader->GetDesc().ShaderType;
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto *pShaderD3D12 = ValidatedCast<ShaderD3D12Impl>(pShader);
    
    VERIFY(m_pShaderResourceLayouts[ShaderInd] == nullptr, "Shader resource layout has already been initialized");

    auto pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(pShaderD3D12->GetDevice());
    auto &ShaderResLayoutAllocator = GetRawAllocator();

    auto *pRawMem = ALLOCATE(ShaderResLayoutAllocator, "Raw memory for ShaderResourceLayoutD3D12", sizeof(ShaderResourceLayoutD3D12));
    m_pShaderResourceLayouts[ShaderInd] = new (pRawMem) ShaderResourceLayoutD3D12(*this, GetRawAllocator());
    m_pShaderResourceLayouts[ShaderInd]->Initialize(pDeviceD3D12Impl->GetD3D12Device(), pShaderD3D12->GetShaderResources(), GetRawAllocator(), nullptr, 0, nullptr, &m_RootSig);
}

PipelineStateD3D12Impl :: PipelineStateD3D12Impl(IReferenceCounters *pRefCounters, RenderDeviceD3D12Impl *pDeviceD3D12, const PipelineStateDesc &PipelineDesc) : 
    TPipelineStateBase(pRefCounters, pDeviceD3D12, PipelineDesc),
    m_DummyVar(*this),
    m_ResourceCacheDataAllocator(GetRawAllocator(), PipelineDesc.SRBAllocationGranularity),
    m_pDefaultShaderResBinding(nullptr, STDDeleter<ShaderResourceBindingD3D12Impl, FixedBlockMemoryAllocator>(pDeviceD3D12->GetSRBAllocator()) )
{
    auto pd3d12Device = pDeviceD3D12->GetD3D12Device();
    if (PipelineDesc.IsComputePipeline)
    {
        auto &ComputePipeline = PipelineDesc.ComputePipeline;

        if( ComputePipeline.pCS == nullptr )
            LOG_ERROR_AND_THROW("Compute shader is not set in the pipeline desc");

        D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12PSODesc = {};
        d3d12PSODesc.pRootSignature = nullptr;
        
        auto *pByteCode = ValidatedCast<ShaderD3D12Impl>(ComputePipeline.pCS)->GetShaderByteCode();
        d3d12PSODesc.CS.pShaderBytecode = pByteCode->GetBufferPointer();
        d3d12PSODesc.CS.BytecodeLength = pByteCode->GetBufferSize();

        // For single GPU operation, set this to zero. If there are multiple GPU nodes, 
        // set bits to identify the nodes (the device's physical adapters) for which the 
        // graphics pipeline state is to apply. Each bit in the mask corresponds to a single node. 
        d3d12PSODesc.NodeMask = 0;

        d3d12PSODesc.CachedPSO.pCachedBlob = nullptr;
        d3d12PSODesc.CachedPSO.CachedBlobSizeInBytes = 0;
        
        // The only valid bit is D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG, which can only be set on WARP devices.
        d3d12PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        ParseShaderResourceLayout(ComputePipeline.pCS);
        m_RootSig.Finalize(pd3d12Device);
        d3d12PSODesc.pRootSignature = m_RootSig.GetD3D12RootSignature();

        HRESULT hr = pd3d12Device->CreateComputePipelineState(&d3d12PSODesc, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>( static_cast<ID3D12PipelineState**>(&m_pd3d12PSO)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
    }
    else
    {
        const auto& GraphicsPipeline = PipelineDesc.GraphicsPipeline;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12PSODesc = {};

        m_RootSig.AllocateStaticSamplers( GetShaders(), GetNumShaders() );
            
#define INIT_SHADER(VarName, ExpectedType)\
        if (GraphicsPipeline.p##VarName)                                            \
        {                                                                           \
            auto ShaderType = GraphicsPipeline.p##VarName->GetDesc().ShaderType;    \
            if( ShaderType != ExpectedType )                                        \
                LOG_ERROR_AND_THROW( GetShaderTypeLiteralName(ShaderType), " shader is provided while ", GetShaderTypeLiteralName(ExpectedType), " is expected") \
            auto *pByteCode = ValidatedCast<ShaderD3D12Impl>(GraphicsPipeline.p##VarName)->GetShaderByteCode(); \
            d3d12PSODesc.VarName.pShaderBytecode = pByteCode->GetBufferPointer();   \
            d3d12PSODesc.VarName.BytecodeLength = pByteCode->GetBufferSize();       \
            ParseShaderResourceLayout(GraphicsPipeline.p##VarName);                 \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            d3d12PSODesc.VarName.pShaderBytecode = nullptr;                         \
            d3d12PSODesc.VarName.BytecodeLength = 0;                                \
        }

        INIT_SHADER(VS, SHADER_TYPE_VERTEX);
        INIT_SHADER(PS, SHADER_TYPE_PIXEL);
        INIT_SHADER(GS, SHADER_TYPE_GEOMETRY);
        INIT_SHADER(DS, SHADER_TYPE_DOMAIN);
        INIT_SHADER(HS, SHADER_TYPE_HULL);
#undef INIT_SHADER

        m_RootSig.Finalize(pd3d12Device);
        d3d12PSODesc.pRootSignature = m_RootSig.GetD3D12RootSignature();
        
        memset(&d3d12PSODesc.StreamOutput, 0, sizeof(d3d12PSODesc.StreamOutput));

        BlendStateDesc_To_D3D12_BLEND_DESC(GraphicsPipeline.BlendDesc, d3d12PSODesc.BlendState);
        // The sample mask for the blend state.
        d3d12PSODesc.SampleMask = GraphicsPipeline.SampleMask;
    
        RasterizerStateDesc_To_D3D12_RASTERIZER_DESC(GraphicsPipeline.RasterizerDesc, d3d12PSODesc.RasterizerState);
        DepthStencilStateDesc_To_D3D12_DEPTH_STENCIL_DESC(GraphicsPipeline.DepthStencilDesc, d3d12PSODesc.DepthStencilState);

        std::vector<D3D12_INPUT_ELEMENT_DESC, STDAllocatorRawMem<D3D12_INPUT_ELEMENT_DESC>> d312InputElements( STD_ALLOCATOR_RAW_MEM(D3D12_INPUT_ELEMENT_DESC, GetRawAllocator(), "Allocator for vector<D3D12_INPUT_ELEMENT_DESC>") );
        if (m_LayoutElements.size() > 0)
        {
            LayoutElements_To_D3D12_INPUT_ELEMENT_DESCs(m_LayoutElements, d312InputElements);
            d3d12PSODesc.InputLayout.NumElements = static_cast<UINT>(d312InputElements.size());
            d3d12PSODesc.InputLayout.pInputElementDescs = d312InputElements.data();
        }
        else
        {
            d3d12PSODesc.InputLayout.NumElements = 0;
            d3d12PSODesc.InputLayout.pInputElementDescs = nullptr;
        }

        d3d12PSODesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        d3d12PSODesc.PrimitiveTopologyType = PrimitiveTopologyType_To_D3D12_PRIMITIVE_TOPOLOGY_TYPE(GraphicsPipeline.PrimitiveTopologyType);

        d3d12PSODesc.NumRenderTargets = GraphicsPipeline.NumRenderTargets;
        for (Uint32 rt = 0; rt < GraphicsPipeline.NumRenderTargets; ++rt)
            d3d12PSODesc.RTVFormats[rt] = TexFormatToDXGI_Format(GraphicsPipeline.RTVFormats[rt]);
        for (Uint32 rt = GraphicsPipeline.NumRenderTargets; rt < 8; ++rt)
            d3d12PSODesc.RTVFormats[rt] = TexFormatToDXGI_Format(GraphicsPipeline.RTVFormats[rt]);
        d3d12PSODesc.DSVFormat = TexFormatToDXGI_Format(GraphicsPipeline.DSVFormat);

        d3d12PSODesc.SampleDesc.Count = GraphicsPipeline.SmplDesc.Count;
        d3d12PSODesc.SampleDesc.Quality = GraphicsPipeline.SmplDesc.Quality;

        // For single GPU operation, set this to zero. If there are multiple GPU nodes, 
        // set bits to identify the nodes (the device's physical adapters) for which the 
        // graphics pipeline state is to apply. Each bit in the mask corresponds to a single node. 
        d3d12PSODesc.NodeMask = 0;

        d3d12PSODesc.CachedPSO.pCachedBlob = nullptr;
        d3d12PSODesc.CachedPSO.CachedBlobSizeInBytes = 0;

        // The only valid bit is D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG, which can only be set on WARP devices.
        d3d12PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        HRESULT hr = pd3d12Device->CreateGraphicsPipelineState(&d3d12PSODesc, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>( static_cast<ID3D12PipelineState**>(&m_pd3d12PSO)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
    }

    if (*m_Desc.Name != 0)
    {
        m_pd3d12PSO->SetName(WidenString(m_Desc.Name).c_str());
        String RootSignatureDesc("Root signature for PSO \"");
        RootSignatureDesc.append(m_Desc.Name);
        RootSignatureDesc.push_back('\"');
        m_RootSig.GetD3D12RootSignature()->SetName(WidenString(RootSignatureDesc).c_str());
    }

    if(PipelineDesc.SRBAllocationGranularity > 1)
        m_ResLayoutDataAllocators.Init(m_NumShaders, PipelineDesc.SRBAllocationGranularity);

    auto &SRBAllocator = pDeviceD3D12->GetSRBAllocator();
    // Default shader resource binding must be initialized after resource layouts are parsed!
    m_pDefaultShaderResBinding.reset( NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D12Impl instance", ShaderResourceBindingD3D12Impl, this)(this, true) );
}

PipelineStateD3D12Impl::~PipelineStateD3D12Impl()
{
    auto &ShaderResLayoutAllocator = GetRawAllocator();
    for(Int32 l = 0; l < _countof(m_pShaderResourceLayouts); ++l)
    {
        if (m_pShaderResourceLayouts[l] != nullptr)
        {
            m_pShaderResourceLayouts[l]->~ShaderResourceLayoutD3D12();
            ShaderResLayoutAllocator.Free(m_pShaderResourceLayouts[l]);
        }
    }

    // D3D12 object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
    pDeviceD3D12Impl->SafeReleaseD3D12Object(m_pd3d12PSO);
}

IMPLEMENT_QUERY_INTERFACE( PipelineStateD3D12Impl, IID_PipelineStateD3D12, TPipelineStateBase )

void PipelineStateD3D12Impl::BindShaderResources(IResourceMapping *pResourceMapping, Uint32 Flags)
{
    if( m_Desc.IsComputePipeline )
    { 
        if(m_pCS)m_pCS->BindResources(pResourceMapping, Flags);
    }
    else
    {
        if(m_pVS)m_pVS->BindResources(pResourceMapping, Flags);
        if(m_pPS)m_pPS->BindResources(pResourceMapping, Flags);
        if(m_pGS)m_pGS->BindResources(pResourceMapping, Flags);
        if(m_pDS)m_pDS->BindResources(pResourceMapping, Flags);
        if(m_pHS)m_pHS->BindResources(pResourceMapping, Flags);
    }
}

void PipelineStateD3D12Impl::CreateShaderResourceBinding(IShaderResourceBinding **ppShaderResourceBinding)
{
    auto *pRenderDeviceD3D12 = ValidatedCast<RenderDeviceD3D12Impl>( GetDevice() );
    auto &SRBAllocator = pRenderDeviceD3D12->GetSRBAllocator();
    auto pResBindingD3D12 = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D12Impl instance", ShaderResourceBindingD3D12Impl)(this, false);
    pResBindingD3D12->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

const ShaderResourceLayoutD3D12& PipelineStateD3D12Impl::GetShaderResLayout(SHADER_TYPE ShaderType)const 
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(m_pShaderResourceLayouts[ShaderInd] != nullptr)
    return *m_pShaderResourceLayouts[ShaderInd];
}

ShaderResourceCacheD3D12* PipelineStateD3D12Impl::CommitAndTransitionShaderResources(IShaderResourceBinding *pShaderResourceBinding, 
                                                                                     CommandContext &Ctx,
                                                                                     bool CommitResources,
                                                                                     bool TransitionResources)const
{
#ifdef VERIFY_SHADER_BINDINGS
    if (pShaderResourceBinding == nullptr &&
        (m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_MUTABLE) != 0 ||
         m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_DYNAMIC) != 0))
    {
        LOG_ERROR_MESSAGE("Pipeline state \"", m_Desc.Name, "\" contains mutable/dynamic shader variables and requires shader resource binding to commit all resources, but none is provided.")
    }
#endif

    // If the shaders contain no resources or static resources only, shader resource binding may be null. 
    // In this case use special internal SRB object
    auto *pResBindingD3D12Impl = pShaderResourceBinding ? ValidatedCast<ShaderResourceBindingD3D12Impl>(pShaderResourceBinding) : m_pDefaultShaderResBinding.get();
    
#ifdef VERIFY_SHADER_BINDINGS
    {
        auto *pRefPSO = pResBindingD3D12Impl->GetPipelineState();
        if (pRefPSO != this)
        {
            LOG_ERROR_MESSAGE("Shader resource binding does not match the pipeline state \"", m_Desc.Name, "\". Operation will be ignored.");
            return nullptr;
        }
    }
#endif

    // First time only, copy static shader resources to the cache
    if(!pResBindingD3D12Impl->StaticResourcesInitialized())
        pResBindingD3D12Impl->InitializeStaticResources(this);

#ifdef VERIFY_SHADER_BINDINGS
    pResBindingD3D12Impl->dbgVerifyResourceBindings(this);
#endif

    auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>( GetDevice() );
    auto &ResourceCache = pResBindingD3D12Impl->GetResourceCache();
    if(CommitResources)
    {
        if(m_Desc.IsComputePipeline)
            Ctx.AsComputeContext().SetRootSignature( GetD3D12RootSignature() );
        else
            Ctx.AsGraphicsContext().SetRootSignature( GetD3D12RootSignature() );

        if(TransitionResources)
            (m_RootSig.*m_RootSig.TransitionAndCommitDescriptorHandles)(pDeviceD3D12Impl, ResourceCache, Ctx, m_Desc.IsComputePipeline);
        else
            (m_RootSig.*m_RootSig.CommitDescriptorHandles)(pDeviceD3D12Impl, ResourceCache, Ctx, m_Desc.IsComputePipeline);
    }
    else
    {
        VERIFY(TransitionResources, "Resources should be transitioned or committed or both")
        m_RootSig.TransitionResources(ResourceCache, Ctx);
    }
    return &ResourceCache;
}


bool PipelineStateD3D12Impl::dbgContainsShaderResources()const
{
    return m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_STATIC) != 0 ||
           m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_MUTABLE) != 0 ||
           m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_DYNAMIC) != 0;
}

}
