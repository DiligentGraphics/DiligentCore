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
#include "DeviceContextD3D11Impl.h"
#include "BufferD3D11Impl.h"
#include "ShaderD3D11Impl.h"
#include "Texture1D_D3D11.h"
#include "Texture2D_D3D11.h"
#include "Texture3D_D3D11.h"
#include "SamplerD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "TextureViewD3D11Impl.h"
#include "PipelineStateD3D11Impl.h"
#include "SwapChainD3D11.h"
#include "D3D11DebugUtilities.h"
#include "ShaderResourceBindingD3D11Impl.h"
#include "EngineD3D11Attribs.h"
#include "EngineD3D11Defines.h"
#include "CommandListD3D11Impl.h"

using namespace Diligent;

namespace Diligent
{
    DeviceContextD3D11Impl::DeviceContextD3D11Impl( IReferenceCounters *pRefCounters, IMemoryAllocator &Allocator, IRenderDevice *pDevice, ID3D11DeviceContext *pd3d11DeviceContext, const struct EngineD3D11Attribs &EngineAttribs, bool bIsDeferred ) :
        TDeviceContextBase(pRefCounters, pDevice, bIsDeferred),
        m_pd3d11DeviceContext( pd3d11DeviceContext ),
        m_CommittedIBFormat(VT_UNDEFINED),
        m_CommittedD3D11IndexDataStartOffset(0),
        m_DebugFlags(EngineAttribs.DebugFlags),
        m_NumCommittedD3D11VBs(0),
        m_CmdListAllocator(GetRawAllocator(), sizeof(CommandListD3D11Impl), 64 )
    {
        memset(m_NumCommittedCBs,            0, sizeof(m_NumCommittedCBs));
        memset(m_NumCommittedSRVs,           0, sizeof(m_NumCommittedSRVs));
        memset(m_NumCommittedSamplers,       0, sizeof(m_NumCommittedSamplers));
        memset(m_NumCommittedUAVs,           0, sizeof(m_NumCommittedUAVs));

        memset(m_CommittedD3D11CBs,          0, sizeof(m_CommittedD3D11CBs));
        memset(m_CommittedD3D11SRVs,         0, sizeof(m_CommittedD3D11SRVs));
        memset(m_CommittedD3D11Samplers,     0, sizeof(m_CommittedD3D11Samplers));
        memset(m_CommittedD3D11UAVs,         0, sizeof(m_CommittedD3D11UAVs));
        memset(m_CommittedD3D11SRVResources, 0, sizeof(m_CommittedD3D11SRVResources));
        memset(m_CommittedD3D11UAVResources, 0, sizeof(m_CommittedD3D11UAVResources));

        memset(m_CommittedD3D11VBStrides, 0, sizeof(m_CommittedD3D11VBStrides));
        memset(m_CommittedD3D11VBOffsets, 0, sizeof(m_CommittedD3D11VBOffsets));
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextD3D11Impl, IID_DeviceContextD3D11, TDeviceContextBase )

    void DeviceContextD3D11Impl::SetPipelineState(IPipelineState *pPipelineState)
    {
        TDeviceContextBase::SetPipelineState( pPipelineState );
        auto *pPipelineStateD3D11 = ValidatedCast<PipelineStateD3D11Impl>(pPipelineState);
        auto &Desc = pPipelineStateD3D11->GetDesc();
        if (Desc.IsComputePipeline)
        {
            auto *pd3d11CS = pPipelineStateD3D11->GetD3D11ComputeShader();
            if (pd3d11CS == nullptr)
            {
                LOG_ERROR("Compute shader is not set in the pipeline");
                return;
            }

#define     COMMIT_SHADER(SN, ShaderName)\
            {                                                                           \
                auto *pd3d11Shader = pPipelineStateD3D11->GetD3D11##ShaderName();       \
                if (m_CommittedD3DShaders[SN##Ind] != pd3d11Shader)                     \
                {                                                                       \
                    m_CommittedD3DShaders[SN##Ind] = pd3d11Shader;                      \
                    m_pd3d11DeviceContext->SN##SetShader(pd3d11Shader, nullptr, 0);     \
                }                                                                       \
            }

            COMMIT_SHADER(CS, ComputeShader);
        }
        else
        {
            COMMIT_SHADER(VS, VertexShader);
            COMMIT_SHADER(PS, PixelShader);
            COMMIT_SHADER(GS, GeometryShader);
            COMMIT_SHADER(HS, HullShader);
            COMMIT_SHADER(DS, DomainShader);
#undef      COMMIT_SHADER

            m_pd3d11DeviceContext->OMSetBlendState( pPipelineStateD3D11->GetD3D11BlendState(), m_BlendFactors, Desc.GraphicsPipeline.SampleMask );
            m_pd3d11DeviceContext->RSSetState( pPipelineStateD3D11->GetD3D11RasterizerState() );
            m_pd3d11DeviceContext->OMSetDepthStencilState( pPipelineStateD3D11->GetD3D11DepthStencilState(), m_StencilRef );

            auto *pd3d11InputLayout = pPipelineStateD3D11->GetD3D11InputLayout();
            // It is safe to perform raw pointer comparison as the device context
            // keeps bound input layout alive
            if( m_CommittedD3D11InputLayout != pd3d11InputLayout )
            {
                m_pd3d11DeviceContext->IASetInputLayout( pd3d11InputLayout );
                m_CommittedD3D11InputLayout = pd3d11InputLayout;
            }

            auto PrimTopology = Desc.GraphicsPipeline.PrimitiveTopology;
            if (m_CommittedPrimitiveTopology != PrimTopology)
            {
                m_CommittedPrimitiveTopology = PrimTopology;
                m_CommittedD3D11PrimTopology = TopologyToD3D11Topology(PrimTopology);
                m_pd3d11DeviceContext->IASetPrimitiveTopology(m_CommittedD3D11PrimTopology);
            }
        }
    }

/// Helper macro used to create an array of device context methods to
/// set particular resource for every shader stage
#define DEFINE_D3D11CTX_FUNC_POINTERS(ArrayName, FuncName) \
    typedef decltype (&ID3D11DeviceContext::VS##FuncName) T##FuncName##Type;  \
    static const T##FuncName##Type ArrayName[] =    \
    {                                           \
        &ID3D11DeviceContext::VS##FuncName,  \
        &ID3D11DeviceContext::PS##FuncName,  \
        &ID3D11DeviceContext::GS##FuncName,  \
        &ID3D11DeviceContext::HS##FuncName,  \
        &ID3D11DeviceContext::DS##FuncName,  \
        &ID3D11DeviceContext::CS##FuncName   \
    };

    DEFINE_D3D11CTX_FUNC_POINTERS(SetCBMethods,      SetConstantBuffers)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSRVMethods,     SetShaderResources)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSamplerMethods, SetSamplers)

    typedef decltype (&ID3D11DeviceContext::CSSetUnorderedAccessViews) TSetUnorderedAccessViewsType;
    static const TSetUnorderedAccessViewsType SetUAVMethods[] =
    {
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        &ID3D11DeviceContext::CSSetUnorderedAccessViews
    };

    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/committing-shader-resources-to-the-gpu-pipeline/
    template<bool TransitionResources,
             bool CommitResources>
    void DeviceContextD3D11Impl::TransitionAndCommitShaderResources(IPipelineState *pPSO, IShaderResourceBinding *pShaderResourceBinding)
    {
        static_assert(TransitionResources || CommitResources, "At least one of TransitionResources or CommitResources flags is expected to be true");

#ifdef _DEBUG
        auto pdbgPipelineStateD3D11 = ValidatedCast<PipelineStateD3D11Impl>( pPSO );
        auto ppdbgShaders = pdbgPipelineStateD3D11->GetShaders();
#endif

        auto pShaderResBindingD3D11 = ValidatedCast<ShaderResourceBindingD3D11Impl>(pShaderResourceBinding);
        if(!pShaderResBindingD3D11)
        {
            auto pPipelineStateD3D11 = ValidatedCast<PipelineStateD3D11Impl>( pPSO );
            pShaderResBindingD3D11 = pPipelineStateD3D11->GetDefaultResourceBinding();
        }
#ifdef _DEBUG
        else
        {
            if (pdbgPipelineStateD3D11->IsIncompatibleWith(pShaderResourceBinding->GetPipelineState()))
            {
                LOG_ERROR_MESSAGE("Shader resource binding does not match Pipeline State");
                return;
            }
        }
#endif

        if(!pShaderResBindingD3D11->IsStaticResourcesBound())
            pShaderResBindingD3D11->BindStaticShaderResources();
        
        auto NumShaders = pShaderResBindingD3D11->GetNumActiveShaders();
        VERIFY(NumShaders == pdbgPipelineStateD3D11->GetNumShaders(), "Number of active shaders in shader resource binding is not consistent with the number of shaders in the pipeline state");

        for (Uint32 s = 0; s < NumShaders; ++s)
        {
            auto ShaderTypeInd = pShaderResBindingD3D11->GetActiveShaderTypeIndex(s);
#ifdef _DEBUG
            auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>(ppdbgShaders[s]);
            VERIFY_EXPR( ShaderTypeInd == static_cast<Int32>(pShaderD3D11->GetShaderTypeIndex()) );
#endif
            
            auto &Cache = pShaderResBindingD3D11->GetResourceCache(s);
            auto PackedResCounts = Cache.GetPackedCounts();

            ShaderResourceCacheD3D11::CachedCB* CachedCBs;
            ID3D11Buffer** d3d11CBs;
            ShaderResourceCacheD3D11::CachedResource* CachedSRVResources;
            ID3D11ShaderResourceView** d3d11SRVs;
            ShaderResourceCacheD3D11::CachedSampler* CachedSamplers;
            ID3D11SamplerState** d3d11Samplers;
            ShaderResourceCacheD3D11::CachedResource* CachedUAVResources;
            ID3D11UnorderedAccessView** d3d11UAVs;
            Cache.GetResourceArrays(CachedCBs, d3d11CBs, CachedSRVResources, d3d11SRVs, CachedSamplers, d3d11Samplers, CachedUAVResources, d3d11UAVs);

#ifdef VERIFY_SHADER_BINDINGS
            {
                pShaderResBindingD3D11->GetResourceLayout(s).dbgVerifyBindings();
                // Static resource bindings are verified in BindStaticShaderResources()
            }
#endif

            // Transition and commit Constant Buffers
            auto NumCBs = ShaderResourceCacheD3D11::UnpackCBCount(PackedResCounts);
            if(NumCBs)
            {
                auto *CommittedD3D11CBs = m_CommittedD3D11CBs[ShaderTypeInd];
                UINT MinSlot = UINT_MAX;
                UINT MaxSlot = 0;
                for(Uint32 cb=0; cb < NumCBs; ++cb)
                {
                    VERIFY_EXPR(cb < Cache.GetCBCount());
                    
                    if(TransitionResources)
                    {
                        auto &CB = CachedCBs[cb];
                        if( auto *pBuff = const_cast<BufferD3D11Impl*>(CB.pBuff.RawPtr()) )
                        {
                            // WARNING! This code is not thread-safe. If several threads change
                            // the buffer state, the results will be undefined.
                            // The solution may be to keep track of the state for each thread
                            // individually, or not rely on the state and check current context bindings
                            if(!pBuff->CheckState(D3D11BufferState::ConstantBuffer))
                            {
                                if( pBuff->CheckState(D3D11BufferState::UnorderedAccess) )
                                {
                                    UnbindResourceFromUAV( pBuff, d3d11CBs[cb] );
                                    pBuff->ClearState(D3D11BufferState::UnorderedAccess);
                                }
                                pBuff->AddState(D3D11BufferState::ConstantBuffer);
                            }
                        }
                    }
#ifdef _DEBUG
                    else
                    {
                        VERIFY_EXPR(CommitResources);
                        auto &CB = CachedCBs[cb];
                        if( auto *pBuff = const_cast<BufferD3D11Impl*>(CB.pBuff.RawPtr()) )
                        {
                            if (!pBuff->CheckState(D3D11BufferState::ConstantBuffer))
                            {
                                LOG_ERROR_MESSAGE("Buffer \"", pBuff->GetDesc().Name, "\" has not been transitioned to Constant Buffer state. Did you forget to call TransitionResources()?");
                            }
                        }
                    }
#endif

                    if(CommitResources)
                    {
                        bool IsNewCB = CommittedD3D11CBs[cb] != d3d11CBs[cb];
                        MinSlot = IsNewCB ? std::min(MinSlot, cb) : MinSlot;
                        MaxSlot = IsNewCB ? cb : MaxSlot;
                        CommittedD3D11CBs[cb] = d3d11CBs[cb];
                    }
                }
                
                if(CommitResources)
                {
                    if(MinSlot != UINT_MAX)
                    {
                        auto SetCBMethod = SetCBMethods[ShaderTypeInd];
                        (m_pd3d11DeviceContext->*SetCBMethod)(MinSlot, MaxSlot-MinSlot+1, CommittedD3D11CBs+MinSlot);
                        m_NumCommittedCBs[ShaderTypeInd] = std::max(m_NumCommittedCBs[ShaderTypeInd], static_cast<Uint8>(NumCBs));
                    }

                    if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
                    {
                        dbgVerifyCommittedCBs(pShaderD3D11->GetDesc().ShaderType);
                    }
                }
            }


            // Transition and commit Shader Resource Views
            auto NumSRVs = ShaderResourceCacheD3D11::UnpackSRVCount(PackedResCounts);
            if(NumSRVs)
            {
                auto *CommittedD3D11SRVs = m_CommittedD3D11SRVs[ShaderTypeInd];
                auto *CommittedD3D11SRVRes = m_CommittedD3D11SRVResources[ShaderTypeInd];

                UINT MinSlot = UINT_MAX;
                UINT MaxSlot = 0;

                for(Uint32 srv=0; srv < NumSRVs; ++srv)
                {
                    VERIFY_EXPR(srv < Cache.GetSRVCount());
                    auto &SRVRes = CachedSRVResources[srv];
                    // WARNING! This code is not thread-safe. If several threads change
                    // the resource state, the results will be undefined.
                    // The solution may be to keep track of the state for each thread
                    // individually, or not rely on the state and check current context bindings
                    if( TransitionResources )
                    {
                        if (auto *pTexture = const_cast<TextureBaseD3D11*>(SRVRes.pTexture))
                        {
                            if( !pTexture->CheckState(D3D11TextureState::ShaderResource) )
                            {
                                if( pTexture->CheckState(D3D11TextureState::UnorderedAccess) )
                                {
                                    UnbindResourceFromUAV(pTexture, SRVRes.pd3d11Resource);
                                    pTexture->ClearState(D3D11TextureState::UnorderedAccess);
                                }
                                if( pTexture->CheckState(D3D11TextureState::RenderTarget) )
                                    UnbindTextureFromRenderTarget(pTexture);
                                if( pTexture->CheckState(D3D11TextureState::DepthStencil) )
                                    UnbindTextureFromDepthStencil(pTexture);
                                pTexture->ResetState(D3D11TextureState::ShaderResource);
                            }
                        }
                        else if(auto *pBuffer = const_cast<BufferD3D11Impl*>(SRVRes.pBuffer))
                        {
                            if( !pBuffer->CheckState(D3D11BufferState::ShaderResource) )
                            {
                                if( pBuffer->CheckState(D3D11BufferState::UnorderedAccess) )
                                {
                                    UnbindResourceFromUAV( pBuffer, SRVRes.pd3d11Resource );
                                    pBuffer->ClearState(D3D11BufferState::UnorderedAccess);
                                }
                                pBuffer->AddState(D3D11BufferState::ShaderResource);
                            }
                        }
                    }
#ifdef _DEBUG
                    else
                    {
                        VERIFY_EXPR(CommitResources);
                        if (auto *pTexture = const_cast<TextureBaseD3D11*>(SRVRes.pTexture))
                        {
                            if( !pTexture->CheckState(D3D11TextureState::ShaderResource) )
                            {
                                LOG_ERROR_MESSAGE("Texture \"", pTexture->GetDesc().Name, "\" has not been transitioned to Shader Resource state. Did you forget to call TransitionResources()?");
                            }
                        }
                        else if(auto *pBuffer = const_cast<BufferD3D11Impl*>(SRVRes.pBuffer))
                        {
                            if( !pBuffer->CheckState(D3D11BufferState::ShaderResource) )
                            {
                                LOG_ERROR_MESSAGE("Texture \"", pBuffer->GetDesc().Name, "\" has not been transitioned to Shader Resource state. Did you forget to call TransitionResources()?");
                            }
                        }
                    }
#endif

                    if(CommitResources)
                    {
                        bool IsNewSRV = CommittedD3D11SRVs[srv] != d3d11SRVs[srv];
                        MinSlot = IsNewSRV ? std::min(MinSlot, srv) : MinSlot;
                        MaxSlot = IsNewSRV ? srv : MaxSlot;

                        CommittedD3D11SRVRes[srv] = SRVRes.pd3d11Resource;
                        CommittedD3D11SRVs[srv] = d3d11SRVs[srv];
                    }
                }

                if(CommitResources)
                {
                    if(MinSlot != UINT_MAX)
                    {
                        auto SetSRVMethod = SetSRVMethods[ShaderTypeInd];
                        (m_pd3d11DeviceContext->*SetSRVMethod)(MinSlot, MaxSlot-MinSlot+1, CommittedD3D11SRVs+MinSlot);
                        m_NumCommittedSRVs[ShaderTypeInd] = std::max(m_NumCommittedSRVs[ShaderTypeInd], static_cast<Uint8>(NumSRVs));
                    }

                    if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
                    {
                        dbgVerifyCommittedSRVs(pShaderD3D11->GetDesc().ShaderType);
                    }
                }
            }


            // Commit samplers (no transitions for samplers)
            if(CommitResources)
            {
                auto NumSamplers = ShaderResourceCacheD3D11::UnpackSamplerCount(PackedResCounts);
                if(NumSamplers)
                {
                    auto *CommittedD3D11Samplers = m_CommittedD3D11Samplers[ShaderTypeInd];
                    UINT MinSlot = std::numeric_limits<UINT>::max();
                    UINT MaxSlot = 0;
                    for(Uint32 sam=0; sam < NumSamplers; ++sam)
                    {
                        VERIFY_EXPR(sam < Cache.GetSamplerCount());

                        bool IsNewSam = CommittedD3D11Samplers[sam] != d3d11Samplers[sam];
                        MinSlot = IsNewSam ? std::min(MinSlot, sam) : MinSlot;
                        MaxSlot = IsNewSam ? sam : MaxSlot;

                        CommittedD3D11Samplers[sam] = d3d11Samplers[sam];
                    }

                    if(MinSlot != UINT_MAX)
                    {
                        auto SetSamplerMethod = SetSamplerMethods[ShaderTypeInd];
                        (m_pd3d11DeviceContext->*SetSamplerMethod)(MinSlot, MaxSlot-MinSlot+1, CommittedD3D11Samplers+MinSlot);
                        m_NumCommittedSamplers[ShaderTypeInd] = std::max(m_NumCommittedSamplers[ShaderTypeInd], static_cast<Uint8>(NumSamplers));
                    }

                    if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
                    {
                        dbgVerifyCommittedSamplers(pShaderD3D11->GetDesc().ShaderType);
                    }
                }
            }


            // Commit Unordered Access Views
            auto NumUAVs = ShaderResourceCacheD3D11::UnpackUAVCount(PackedResCounts);
            if(NumUAVs)
            {
                auto *CommittedD3D11UAVs = m_CommittedD3D11UAVs[ShaderTypeInd];
                auto *CommittedD3D11UAVRes = m_CommittedD3D11UAVResources[ShaderTypeInd];

                UINT MinSlot = UINT_MAX;
                UINT MaxSlot = 0;

                for(Uint32 uav=0; uav < NumUAVs; ++uav)
                {
                    VERIFY_EXPR(uav < Cache.GetUAVCount());
                    auto &UAVRes = CachedUAVResources[uav];
                    // WARNING! This code is not thread-safe. If several threads change
                    // the resource state, the results will be undefined.
                    // The solution may be to keep track of the state for each thread
                    // individually, or not rely on the state and check current context bindings
                    if(TransitionResources)
                    {
                        if ( auto* pTexture = const_cast<TextureBaseD3D11*>(UAVRes.pTexture) )
                        {
                            if( !pTexture->CheckState(D3D11TextureState::UnorderedAccess) )
                            {
                                if( pTexture->CheckState(D3D11TextureState::ShaderResource) )
                                    UnbindTextureFromInput( pTexture, UAVRes.pd3d11Resource );
                                pTexture->ResetState(D3D11TextureState::UnorderedAccess);
                            }
                        }
                        else if( auto *pBuffer = const_cast<BufferD3D11Impl*>(UAVRes.pBuffer) )
                        {
                            if( !pBuffer->CheckState(D3D11BufferState::UnorderedAccess) )
                            {
                                if( pBuffer->CheckState(D3D11BufferState::AnyInput) )
                                    UnbindBufferFromInput( pBuffer, UAVRes.pd3d11Resource );
                                pBuffer->ResetState(D3D11BufferState::UnorderedAccess);
                            }
                        }
                    }
#ifdef _DEBUG
                    else
                    {
                        if ( auto* pTexture = const_cast<TextureBaseD3D11*>(UAVRes.pTexture) )
                        {
                            if( !pTexture->CheckState(D3D11TextureState::UnorderedAccess) )
                            {
                                LOG_ERROR_MESSAGE("Texture \"", pTexture->GetDesc().Name, "\" has not been transitioned to Unordered Access state. Did you forget to call TransitionResources()?");
                            }
                        }
                        else if( auto *pBuffer = const_cast<BufferD3D11Impl*>(UAVRes.pBuffer) )
                        {
                            if( !pBuffer->CheckState(D3D11BufferState::UnorderedAccess) )
                            {
                                LOG_ERROR_MESSAGE("Buffer \"", pBuffer->GetDesc().Name, "\" has not been transitioned to Unordered Access state. Did you forget to call TransitionResources()?");
                            }
                        }
                    }
#endif
                    if(CommitResources)
                    {
                        bool IsNewUAV = CommittedD3D11UAVs[uav] != d3d11UAVs[uav];
                        MinSlot = IsNewUAV ? std::min(MinSlot, uav) : MinSlot;
                        MaxSlot = IsNewUAV ? uav : MaxSlot;

                        CommittedD3D11UAVRes[uav] = UAVRes.pd3d11Resource;
                        CommittedD3D11UAVs[uav] = d3d11UAVs[uav];
                    }
                }

                if(CommitResources)
                {
                    if(MinSlot != UINT_MAX)
                    {
                        auto SetUAVMethod = SetUAVMethods[ShaderTypeInd];
                        (m_pd3d11DeviceContext->*SetUAVMethod)(MinSlot, MaxSlot-MinSlot+1, CommittedD3D11UAVs+MinSlot, nullptr);
                        m_NumCommittedUAVs[ShaderTypeInd] = std::max(m_NumCommittedUAVs[ShaderTypeInd], static_cast<Uint8>(NumUAVs));
                    }

                    if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
                    {
                        dbgVerifyCommittedUAVs(pShaderD3D11->GetDesc().ShaderType);
                    }
                }
            }

#ifdef VERIFY_SHADER_BINDINGS
            if( CommitResources && m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedShaderResources )
            {
                // Use full resource layout to verify that all required resources are committed
                pShaderD3D11->GetResources()->dbgVerifyCommittedResources(
                            m_CommittedD3D11CBs[ShaderTypeInd], 
                            m_CommittedD3D11SRVs[ShaderTypeInd], 
                            m_CommittedD3D11SRVResources[ShaderTypeInd], 
                            m_CommittedD3D11Samplers[ShaderTypeInd], 
                            m_CommittedD3D11UAVs[ShaderTypeInd],
                            m_CommittedD3D11UAVResources[ShaderTypeInd],
                            Cache);
            }
#endif
        }
    }

    void DeviceContextD3D11Impl::TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)
    {
        TransitionAndCommitShaderResources<true, false>(pPipelineState, pShaderResourceBinding);
    }

    void DeviceContextD3D11Impl::CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)
    {
        if( !DeviceContextBase::CommitShaderResources<PipelineStateD3D11Impl>(pShaderResourceBinding, Flags, 0 /*Dummy*/) )
            return;

        if(Flags & COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES)
            TransitionAndCommitShaderResources<true, true>(m_pPipelineState, pShaderResourceBinding);
        else
            TransitionAndCommitShaderResources<false, true>(m_pPipelineState, pShaderResourceBinding);
    }

    void DeviceContextD3D11Impl::SetStencilRef(Uint32 StencilRef)
    {
        if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        {
            ID3D11DepthStencilState *pd3d11DSS = m_pPipelineState ? ValidatedCast<PipelineStateD3D11Impl>(m_pPipelineState.RawPtr())->GetD3D11DepthStencilState() : nullptr;
            m_pd3d11DeviceContext->OMSetDepthStencilState( pd3d11DSS, m_StencilRef );
        }
    }


    void DeviceContextD3D11Impl::SetBlendFactors(const float* pBlendFactors)
    {
        if (TDeviceContextBase::SetBlendFactors(pBlendFactors, 0))
        {
            Uint32 SampleMask = 0xFFFFFFFF;
            ID3D11BlendState *pd3d11BS = nullptr;
            if(m_pPipelineState)
            {
                SampleMask = m_pPipelineState->GetDesc().GraphicsPipeline.SampleMask;
                pd3d11BS = ValidatedCast<PipelineStateD3D11Impl>(m_pPipelineState.RawPtr())->GetD3D11BlendState();
            }
            m_pd3d11DeviceContext->OMSetBlendState(pd3d11BS, m_BlendFactors, SampleMask);
        }
    }

    void DeviceContextD3D11Impl::CommitD3D11IndexBuffer(VALUE_TYPE IndexType)
    {
        if( !m_pIndexBuffer )
        {
            LOG_ERROR_MESSAGE( "Index buffer is not set up for indexed draw command" );
            return;
        }

        BufferD3D11Impl *pBuffD3D11 = static_cast<BufferD3D11Impl *>(m_pIndexBuffer.RawPtr());
        if( pBuffD3D11->CheckState( D3D11BufferState::UnorderedAccess ) )
        {
            UnbindResourceFromUAV(pBuffD3D11, pBuffD3D11->m_pd3d11Buffer);
            pBuffD3D11->ClearState( D3D11BufferState::UnorderedAccess );
        }

        if( m_CommittedD3D11IndexBuffer != pBuffD3D11->m_pd3d11Buffer ||
            m_CommittedIBFormat != IndexType ||
            m_CommittedD3D11IndexDataStartOffset != m_IndexDataStartOffset )
        {
            DXGI_FORMAT D3D11IndexFmt = DXGI_FORMAT_UNKNOWN;
            if( IndexType == VT_UINT32 )
                D3D11IndexFmt = DXGI_FORMAT_R32_UINT;
            else if( IndexType == VT_UINT16 )
                D3D11IndexFmt = DXGI_FORMAT_R16_UINT;
            else
            {
                LOG_ERROR_MESSAGE( "Unsupported index format. Only R16_UINT and R32_UINT are allowed." );
                return;
            }

            m_CommittedD3D11IndexBuffer = pBuffD3D11->m_pd3d11Buffer;
            m_CommittedIBFormat = IndexType;
            m_CommittedD3D11IndexDataStartOffset = m_IndexDataStartOffset;
            m_pd3d11DeviceContext->IASetIndexBuffer( pBuffD3D11->m_pd3d11Buffer, D3D11IndexFmt, m_IndexDataStartOffset );
        }

        pBuffD3D11->AddState(D3D11BufferState::IndexBuffer);
        m_bCommittedD3D11IBUpToDate = true;
    }

    void DeviceContextD3D11Impl::CommitD3D11VertexBuffers(PipelineStateD3D11Impl *pPipelineStateD3D11)
    {
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        UINT NumBuffersToSet = std::max(m_NumVertexStreams, m_NumCommittedD3D11VBs );

        bool BindVBs = m_NumVertexStreams != m_NumCommittedD3D11VBs;

        const auto *TightStrides = pPipelineStateD3D11->GetTightStrides();
        for( UINT Slot = 0; Slot < m_NumVertexStreams; ++Slot )
        {
            auto &CurrStream = m_VertexStreams[Slot];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            auto *pBuffD3D11Impl = ValidatedCast<BufferD3D11Impl>(CurrStream.pBuffer.RawPtr());
            ID3D11Buffer *pd3d11Buffer = pBuffD3D11Impl->m_pd3d11Buffer;
            auto Stride = CurrStream.Stride ? CurrStream.Stride : TightStrides[Slot];
            auto Offset = CurrStream.Offset;

            if(pBuffD3D11Impl->CheckState( D3D11BufferState::UnorderedAccess ))
            {
                UnbindResourceFromUAV(pBuffD3D11Impl, pd3d11Buffer);
                pBuffD3D11Impl->ClearState( D3D11BufferState::UnorderedAccess );
            }

            // It is safe to perform raw pointer check because device context keeps
            // all buffers alive.
            if (m_CommittedD3D11VertexBuffers[Slot] != pd3d11Buffer ||
                m_CommittedD3D11VBStrides[Slot] != Stride ||
                m_CommittedD3D11VBOffsets[Slot] != Offset)
            {
                BindVBs = true;

                m_CommittedD3D11VertexBuffers[Slot] = pd3d11Buffer;
                m_CommittedD3D11VBStrides[Slot] = Stride;
                m_CommittedD3D11VBOffsets[Slot] = Offset;

                pBuffD3D11Impl->AddState( D3D11BufferState::VertexBuffer );
            }
        }

        // Unbind all buffers at the end
        for (Uint32 Slot = m_NumVertexStreams; Slot < m_NumCommittedD3D11VBs; ++Slot)
        {
            m_CommittedD3D11VertexBuffers[Slot] = nullptr;
            m_CommittedD3D11VBStrides[Slot] = 0;
            m_CommittedD3D11VBOffsets[Slot] = 0;
        }

        m_NumCommittedD3D11VBs = m_NumVertexStreams;
        
        if( BindVBs )
        {
            m_pd3d11DeviceContext->IASetVertexBuffers( 0, NumBuffersToSet, m_CommittedD3D11VertexBuffers, m_CommittedD3D11VBStrides, m_CommittedD3D11VBOffsets );
        }

        m_bCommittedD3D11VBsUpToDate = true;
    }

    void DeviceContextD3D11Impl::Draw( DrawAttribs &DrawAttribs )
    {
#ifdef _DEBUG
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }
#endif

        auto *pPipelineStateD3D11 = ValidatedCast<PipelineStateD3D11Impl>(m_pPipelineState.RawPtr());
#ifdef _DEBUG
        if (pPipelineStateD3D11->GetDesc().IsComputePipeline)
        {
            LOG_ERROR("No graphics pipeline state is bound");
            return;
        }
#endif

        auto *pd3d11InputLayout = pPipelineStateD3D11->GetD3D11InputLayout();
        if( pd3d11InputLayout != nullptr && !m_bCommittedD3D11VBsUpToDate )
        {
            CommitD3D11VertexBuffers(pPipelineStateD3D11);
        }

        if( DrawAttribs.IsIndexed )
        {
            if( m_CommittedIBFormat != DrawAttribs.IndexType )
                m_bCommittedD3D11IBUpToDate = false;
            if(!m_bCommittedD3D11IBUpToDate)
            {
                CommitD3D11IndexBuffer(DrawAttribs.IndexType);
            }
        }
        
#ifdef _DEBUG
        if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
        {
            // Verify bindings after all resources are set
            dbgVerifyRenderTargetFormats();
            dbgVerifyCommittedSRVs();
            dbgVerifyCommittedUAVs();
            dbgVerifyCommittedSamplers();
            dbgVerifyCommittedCBs();
            dbgVerifyCommittedVertexBuffers();
            dbgVerifyCommittedIndexBuffer();
            dbgVerifyCommittedShaders();
        }
#endif

        if( DrawAttribs.IsIndirect )
        {
            VERIFY( DrawAttribs.pIndirectDrawAttribs, "Indirect draw command attributes buffer is not set" );
            auto *pBufferD3D11 = static_cast<BufferD3D11Impl*>(DrawAttribs.pIndirectDrawAttribs);
            ID3D11Buffer *pd3d11ArgsBuff = pBufferD3D11 ? pBufferD3D11->m_pd3d11Buffer : nullptr;
            if( DrawAttribs.IsIndexed )
                m_pd3d11DeviceContext->DrawIndexedInstancedIndirect( pd3d11ArgsBuff, DrawAttribs.IndirectDrawArgsOffset );
            else
                m_pd3d11DeviceContext->DrawInstancedIndirect( pd3d11ArgsBuff, DrawAttribs.IndirectDrawArgsOffset );
        }
        else
        {
            if( DrawAttribs.NumInstances > 1 )
            {
                if( DrawAttribs.IsIndexed )
                    m_pd3d11DeviceContext->DrawIndexedInstanced( DrawAttribs.NumIndices, DrawAttribs.NumInstances, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation );
                else
                    m_pd3d11DeviceContext->DrawInstanced( DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.StartVertexLocation, DrawAttribs.FirstInstanceLocation );
            }
            else
            {
                if( DrawAttribs.IsIndexed )
                    m_pd3d11DeviceContext->DrawIndexed( DrawAttribs.NumIndices, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex );
                else
                    m_pd3d11DeviceContext->Draw( DrawAttribs.NumVertices, DrawAttribs.StartVertexLocation );
            }
        }
    }

    void DeviceContextD3D11Impl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
    {
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }
        if (!m_pPipelineState->GetDesc().IsComputePipeline)
        {
            LOG_ERROR("No compute pipeline state is bound");
            return;
        }

        if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
        {
            // Verify bindings
            dbgVerifyCommittedSRVs();
            dbgVerifyCommittedUAVs();
            dbgVerifyCommittedSamplers();
            dbgVerifyCommittedCBs();
            dbgVerifyCommittedShaders();
        }

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            CHECK_DYNAMIC_TYPE( BufferD3D11Impl, DispatchAttrs.pIndirectDispatchAttribs );
            auto *pd3d11Buff = static_cast<BufferD3D11Impl*>(DispatchAttrs.pIndirectDispatchAttribs)->GetD3D11Buffer();
            m_pd3d11DeviceContext->DispatchIndirect( pd3d11Buff, DispatchAttrs.DispatchArgsByteOffset );
        }
        else
            m_pd3d11DeviceContext->Dispatch( DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ );
    }

    void DeviceContextD3D11Impl::ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        ID3D11DepthStencilView *pd3d11DSV = nullptr;
        if( pView != nullptr )
        {
#ifdef _DEBUG
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
#endif
            auto *pViewD3D11 = ValidatedCast<TextureViewD3D11Impl>(pView);
            pd3d11DSV = static_cast<ID3D11DepthStencilView *>(pViewD3D11->GetD3D11View());
        }
        else
        {
            if (m_pSwapChain)
            {
                pd3d11DSV = ValidatedCast<ISwapChainD3D11>(m_pSwapChain.RawPtr())->GetDSV();
                VERIFY_EXPR(pd3d11DSV != nullptr);
            }
            else
            {
                LOG_ERROR("Failed to clear default depth stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }
        UINT32 d3d11ClearFlags = 0;
        if( ClearFlags & CLEAR_DEPTH_FLAG )   d3d11ClearFlags |= D3D11_CLEAR_DEPTH;
        if( ClearFlags & CLEAR_STENCIL_FLAG ) d3d11ClearFlags |= D3D11_CLEAR_STENCIL;
        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.
        m_pd3d11DeviceContext->ClearDepthStencilView( pd3d11DSV, d3d11ClearFlags, fDepth, Stencil );
    }

    void DeviceContextD3D11Impl::ClearRenderTarget( ITextureView *pView, const float *RGBA )
    {
        ID3D11RenderTargetView *pd3d11RTV = nullptr;
        if( pView != nullptr )
        {
#ifdef _DEBUG
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
#endif
            auto *pViewD3D11 = ValidatedCast<TextureViewD3D11Impl>(pView);
            pd3d11RTV = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());
        }
        else
        {
            if (m_pSwapChain)
            {
                pd3d11RTV = ValidatedCast<ISwapChainD3D11>(m_pSwapChain.RawPtr())->GetRTV();
                VERIFY_EXPR(pd3d11RTV != nullptr);
            }
            else
            {
                LOG_ERROR("Failed to clear default render target: swap chain is not initialized in the device context");
                return;
            }
        }

        static const float Zero[4] = { 0.f, 0.f, 0.f, 0.f };
        if( RGBA == nullptr )
            RGBA = Zero;

        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.
        m_pd3d11DeviceContext->ClearRenderTargetView( pd3d11RTV, RGBA );
    }

    void DeviceContextD3D11Impl::Flush()
    {
        m_pd3d11DeviceContext->Flush();
    }

    void DeviceContextD3D11Impl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pStrides, pOffsets, Flags );
        m_bCommittedD3D11VBsUpToDate = false;
    }

    void DeviceContextD3D11Impl::SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
        m_bCommittedD3D11IBUpToDate = false;
    }

    void DeviceContextD3D11Impl::SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumViewports < MaxViewports, "Too many viewports are being set" );
        NumViewports = std::min( NumViewports, MaxViewports );

        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );
        
        D3D11_VIEWPORT d3d11Viewports[MaxViewports];
        VERIFY( NumViewports == m_NumViewports, "Unexpected number of viewports" );
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
        {
            d3d11Viewports[vp].TopLeftX = m_Viewports[vp].TopLeftX;
            d3d11Viewports[vp].TopLeftY = m_Viewports[vp].TopLeftY;
            d3d11Viewports[vp].Width    = m_Viewports[vp].Width;
            d3d11Viewports[vp].Height   = m_Viewports[vp].Height;
            d3d11Viewports[vp].MinDepth = m_Viewports[vp].MinDepth;
            d3d11Viewports[vp].MaxDepth = m_Viewports[vp].MaxDepth;
        }
        // All viewports must be set atomically as one operation. 
        // Any viewports not defined by the call are disabled.
        m_pd3d11DeviceContext->RSSetViewports( NumViewports, d3d11Viewports );
    }

    void DeviceContextD3D11Impl::SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxScissorRects = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumRects < MaxScissorRects, "Too many scissor rects are being set" );
        NumRects = std::min( NumRects, MaxScissorRects );

        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        D3D11_RECT d3d11ScissorRects[MaxScissorRects];
        VERIFY( NumRects == m_NumScissorRects, "Unexpected number of scissor rects" );
        for( Uint32 sr = 0; sr < NumRects; ++sr )
        {
            d3d11ScissorRects[sr].left   = m_ScissorRects[sr].left;
            d3d11ScissorRects[sr].top    = m_ScissorRects[sr].top;
            d3d11ScissorRects[sr].right  = m_ScissorRects[sr].right;
            d3d11ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
        }

        // All scissor rects must be set atomically as one operation. 
        // Any scissor rects not defined by the call are disabled.
        m_pd3d11DeviceContext->RSSetScissorRects( NumRects, d3d11ScissorRects );
    }

    void DeviceContextD3D11Impl::CommitRenderTargets()
    {
        const Uint32 MaxD3D11RTs = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        Uint32 NumRenderTargets = m_NumBoundRenderTargets;
        VERIFY( NumRenderTargets <= MaxD3D11RTs, "D3D11 only allows 8 simultaneous render targets" );
        NumRenderTargets = std::min( MaxD3D11RTs, NumRenderTargets );

        // Do not waste time setting RTVs to null
        ID3D11RenderTargetView *pd3d11RTs[MaxD3D11RTs];
        ID3D11DepthStencilView *pd3d11DSV = nullptr;

        if( m_IsDefaultFramebufferBound )
        {
            if (m_pSwapChain)
            {
                NumRenderTargets = 1;
                auto *pSwapChainD3D11 = ValidatedCast<ISwapChainD3D11>(m_pSwapChain.RawPtr());
                pd3d11RTs[0] = pSwapChainD3D11->GetRTV();
                pd3d11DSV = pSwapChainD3D11->GetDSV();
                VERIFY_EXPR(pd3d11RTs[0] != nullptr && pd3d11DSV != nullptr);
            }
            else
            {
                LOG_ERROR("Failed to commit default render target and depth stencil: swap chain is not initialized in the device context");
                return;
            }
        }
        else
        {
            for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
            {
                auto *pView = m_pBoundRenderTargets[rt].RawPtr();
                if( pView )
                {
                    auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pView);
                    pd3d11RTs[rt] = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());
                }
                else
                    pd3d11RTs[rt] = nullptr;
            }

            auto *pDepthStencil = m_pBoundDepthStencil.RawPtr();
            if( pDepthStencil != nullptr )
            {
                auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pDepthStencil);
                pd3d11DSV = static_cast<ID3D11DepthStencilView*>(pViewD3D11->GetD3D11View());
            }
        }

        m_pd3d11DeviceContext->OMSetRenderTargets(NumRenderTargets, pd3d11RTs, pd3d11DSV);
    }


    void UnbindView( ID3D11DeviceContext *pContext, TSetShaderResourcesType SetSRVMethod, UINT Slot )
    {
        ID3D11ShaderResourceView *ppNullView[] = { nullptr };
        (pContext->*SetSRVMethod)(Slot, 1, ppNullView);
    }

    void UnbindView( ID3D11DeviceContext *pContext, TSetUnorderedAccessViewsType SetUAVMethod, UINT Slot )
    {
        ID3D11UnorderedAccessView *ppNullView[] = { nullptr };
        (pContext->*SetUAVMethod)(Slot, 1, ppNullView, nullptr);
    }

    /// \tparam TD3D11ResourceViewType - Type of the D3D11 resource view (ID3D11ShaderResourceView or ID3D11UnorderedAccessView)
    /// \tparam TSetD3D11View - Type of the D3D11 device context method used to set the D3D11 view
    /// \param CommittedResourcesArr - Pointer to the array of strong references to currently bound
    ///                            shader resources, for each shader stage
    /// \param CommittedD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 shader resources, for each shader stage
    /// \param pResToUnbind - Resource to unbind
    /// \param SetD3D11ViewMethods - Array of pointers to device context methods used to set the view,
    ///                              for every shader stage
    template<typename TD3D11ResourceViewType,
             typename TSetD3D11View,
             size_t NumSlots>
    void DeviceContextD3D11Impl::UnbindResourceView( TD3D11ResourceViewType CommittedD3D11ViewsArr[][NumSlots], 
                                                     ID3D11Resource* CommittedD3D11ResourcesArr[][NumSlots], 
                                                     Uint8 NumCommittedResourcesArr[],
                                                     IDeviceObject *pResToUnbind,
                                                     ID3D11Resource *pd3d11ResToUndind,
                                                     TSetD3D11View SetD3D11ViewMethods[])
    {
        for( Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd )
        {
            auto *CommittedD3D11Views = CommittedD3D11ViewsArr[ShaderTypeInd];
            auto *CommittedD3D11Resources = CommittedD3D11ResourcesArr[ShaderTypeInd];
            auto &NumCommittedSlots = NumCommittedResourcesArr[ShaderTypeInd];

            for( Uint32 Slot = 0; Slot < NumCommittedSlots; ++Slot )
            {
                if( CommittedD3D11Resources[Slot] == pd3d11ResToUndind )
                {
                    CommittedD3D11Resources[Slot] = nullptr;
                    CommittedD3D11Views[Slot] = nullptr;

                    auto SetViewMethod = SetD3D11ViewMethods[ShaderTypeInd];
                    UnbindView( m_pd3d11DeviceContext, SetViewMethod, Slot );
                }
            }

            // Pop null resources from the end of arrays
            while( NumCommittedSlots > 0 && CommittedD3D11Resources[NumCommittedSlots-1] == nullptr )
            {
                VERIFY( CommittedD3D11Views[NumSlots-1] == nullptr, "Unexpected non-null resource view" );
                --NumCommittedSlots;
            }
        }
    }

    void DeviceContextD3D11Impl::UnbindTextureFromInput( TextureBaseD3D11 *pTexture, ID3D11Resource *pd3d11Resource )
    {
        VERIFY( pTexture, "Null texture provided" );
        if( !pTexture )return;

        UnbindResourceView( m_CommittedD3D11SRVs, m_CommittedD3D11SRVResources, m_NumCommittedSRVs, pTexture, pd3d11Resource, SetSRVMethods );
        pTexture->ClearState(D3D11TextureState::ShaderResource);
    }

    void DeviceContextD3D11Impl::UnbindBufferFromInput( BufferD3D11Impl *pBuffer, ID3D11Resource *pd3d11Buffer )
    {
        VERIFY( pBuffer, "Null buffer provided" );
        if( !pBuffer )return;

        if( pBuffer->CheckState(D3D11BufferState::ShaderResource) )
        {
            UnbindResourceView( m_CommittedD3D11SRVs, m_CommittedD3D11SRVResources, m_NumCommittedSRVs, pBuffer, pd3d11Buffer, SetSRVMethods );
            pBuffer->ClearState( D3D11BufferState::ShaderResource );
        }
        
        if( pBuffer->CheckState(D3D11BufferState::IndexBuffer) )
        {
            auto pd3d11IndBuffer = ValidatedCast<BufferD3D11Impl>( pBuffer )->GetD3D11Buffer();
            if( pd3d11IndBuffer == m_CommittedD3D11IndexBuffer )
            {
                // Only unbind D3D11 buffer from the context!
                // m_pIndexBuffer.Release();
                m_CommittedD3D11IndexBuffer.Release();
                m_CommittedIBFormat = VT_UNDEFINED;
                m_CommittedD3D11IndexDataStartOffset = 0;
                m_bCommittedD3D11IBUpToDate = false;
                m_pd3d11DeviceContext->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, m_CommittedD3D11IndexDataStartOffset );
            }
            if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
            {
                dbgVerifyCommittedIndexBuffer();
            }
            pBuffer->ClearState( D3D11BufferState::IndexBuffer );
        }

        if( pBuffer->CheckState( D3D11BufferState::VertexBuffer ) )
        {
            auto pd3d11VB = ValidatedCast<BufferD3D11Impl>( pBuffer )->GetD3D11Buffer();
            for( Uint32 Slot = 0; Slot < m_NumCommittedD3D11VBs; ++Slot )
            {
                auto &CommittedD3D11VB = m_CommittedD3D11VertexBuffers[Slot];
                if(CommittedD3D11VB == pd3d11VB)
                {
                    // Unbind only D3D11 buffer
                    //*VertStream = VertexStreamInfo();
                    ID3D11Buffer *ppNullBuffer[] = { nullptr };
                    const UINT Zero[] = { 0 };
                    m_CommittedD3D11VertexBuffers[Slot] = nullptr;
                    m_CommittedD3D11VBStrides[Slot] = 0;
                    m_CommittedD3D11VBOffsets[Slot] = 0;
                    m_bCommittedD3D11VBsUpToDate = false;
                    m_pd3d11DeviceContext->IASetVertexBuffers( Slot, _countof( ppNullBuffer ), ppNullBuffer, Zero, Zero );
                }
            }
            if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
            {
                dbgVerifyCommittedVertexBuffers();
            }
            pBuffer->ClearState( D3D11BufferState::VertexBuffer );
        }

        if( pBuffer->CheckState( D3D11BufferState::ConstantBuffer ) )
        {
            for( Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd )
            {
                auto *CommittedD3D11CBs = m_CommittedD3D11CBs[ShaderTypeInd];
                auto NumSlots = m_NumCommittedCBs[ShaderTypeInd];
                for( Uint32 Slot = 0; Slot < NumSlots; ++Slot )
                {
                    if( CommittedD3D11CBs[Slot] == pd3d11Buffer )
                    {
                        CommittedD3D11CBs[Slot] = nullptr;
                        auto SetCBMethod = SetCBMethods[ShaderTypeInd];
                        ID3D11Buffer *ppNullBuffer[] = { nullptr };
                        (m_pd3d11DeviceContext->*SetCBMethod)(Slot, 1, ppNullBuffer);
                    }
                }
            }
            if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
            {
                dbgVerifyCommittedCBs();
            }
            pBuffer->ClearState( D3D11BufferState::ConstantBuffer );
        }
    }

    void DeviceContextD3D11Impl::UnbindResourceFromUAV( IDeviceObject *pResource, ID3D11Resource *pd3d11Resource )
    {
        VERIFY( pResource, "Null resource provided" );
        if( !pResource )return;

        UnbindResourceView( m_CommittedD3D11UAVs, m_CommittedD3D11UAVResources, m_NumCommittedUAVs, pResource, pd3d11Resource, SetUAVMethods );
    }

    void DeviceContextD3D11Impl::UnbindTextureFromRenderTarget( TextureBaseD3D11 *pTexture )
    {
        VERIFY( pTexture, "Null resource provided" );
        if( !pTexture )return;

        bool bCommitRenderTargets = false;
        for( Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt )
        {
            if( auto pTexView = m_pBoundRenderTargets[rt] )
            {
                if( pTexView->GetTexture() == pTexture )
                {
                    m_pBoundRenderTargets[rt].Release();
                    bCommitRenderTargets = true;
                }
            }
        }

        if( bCommitRenderTargets )
            CommitRenderTargets();

        pTexture->ClearState(D3D11TextureState::RenderTarget);
    }

    void DeviceContextD3D11Impl::UnbindTextureFromDepthStencil(TextureBaseD3D11 *pTexD3D11)
    {
        VERIFY( pTexD3D11, "Null resource provided" );
        if( !pTexD3D11 )return;
        
        if( m_pBoundDepthStencil && m_pBoundDepthStencil->GetTexture() == pTexD3D11 )
        {
            m_pBoundDepthStencil.Release();
            CommitRenderTargets();
        }
        pTexD3D11->ClearState(D3D11TextureState::DepthStencil);
    }

    void DeviceContextD3D11Impl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            for( Uint32 RT = 0; RT < NumRenderTargets; ++RT )
                if( ppRenderTargets[RT] )
                {
                    auto *pTex = ValidatedCast<TextureBaseD3D11>(ppRenderTargets[RT]->GetTexture());
                    UnbindTextureFromInput( pTex, pTex->GetD3D11Texture() );
                    pTex->ResetState(D3D11TextureState::RenderTarget);
                }
            if( pDepthStencil )
            {
                auto *pTex = ValidatedCast<TextureBaseD3D11>(pDepthStencil->GetTexture());
                UnbindTextureFromInput( pTex, pTex->GetD3D11Texture() );
                pTex->ResetState(D3D11TextureState::DepthStencil);
            }

            CommitRenderTargets();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }
    }

    template<typename TD3D11ResourceType, typename TSetD3D11ResMethodType>
    void SetD3D11ResourcesHelper(ID3D11DeviceContext *pDeviceCtx, 
                                 TSetD3D11ResMethodType SetD3D11ResMethod, 
                                 UINT StartSlot, UINT NumSlots, 
                                 TD3D11ResourceType **ppResources)
    {
        (pDeviceCtx->*SetD3D11ResMethod)(StartSlot, NumSlots, ppResources);
    }

    template<>
    void SetD3D11ResourcesHelper(ID3D11DeviceContext *pDeviceCtx, 
                                 TSetUnorderedAccessViewsType SetD3D11UAVMethod, 
                                 UINT StartSlot, UINT NumSlots, 
                                 ID3D11UnorderedAccessView **ppUAVs)
    {
        (pDeviceCtx->*SetD3D11UAVMethod)(StartSlot, NumSlots, ppUAVs, nullptr);
    }

    template<typename TD3D11ResourceType, typename TSetD3D11ResMethodType>
    void ReleaseCommittedShaderResourcesHelper(TD3D11ResourceType CommittedD3D11Res[],
                                               Uint8 NumCommittedResources,
                                               TSetD3D11ResMethodType SetD3D11ResMethod,
                                               ID3D11DeviceContext *pDeviceCtx)
    {
        if( NumCommittedResources > 0)
        {
            memset( CommittedD3D11Res, 0, NumCommittedResources * sizeof( CommittedD3D11Res[0] ) );
            SetD3D11ResourcesHelper( pDeviceCtx, SetD3D11ResMethod, 0, NumCommittedResources, CommittedD3D11Res );
        }
    }

    void DeviceContextD3D11Impl::ReleaseCommittedShaderResources()
    {
        for( int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType )
        {
            ReleaseCommittedShaderResourcesHelper( m_CommittedD3D11CBs     [ShaderType], m_NumCommittedCBs     [ShaderType], SetCBMethods[ShaderType],      m_pd3d11DeviceContext);
            ReleaseCommittedShaderResourcesHelper( m_CommittedD3D11SRVs    [ShaderType], m_NumCommittedSRVs    [ShaderType], SetSRVMethods[ShaderType],     m_pd3d11DeviceContext);
            ReleaseCommittedShaderResourcesHelper( m_CommittedD3D11Samplers[ShaderType], m_NumCommittedSamplers[ShaderType], SetSamplerMethods[ShaderType], m_pd3d11DeviceContext);
            ReleaseCommittedShaderResourcesHelper( m_CommittedD3D11UAVs    [ShaderType], m_NumCommittedUAVs    [ShaderType], SetUAVMethods[ShaderType],     m_pd3d11DeviceContext);
            memset(m_CommittedD3D11SRVResources[ShaderType], 0, sizeof(m_CommittedD3D11SRVResources[ShaderType][0])*m_NumCommittedSRVs[ShaderType] );
            memset(m_CommittedD3D11UAVResources[ShaderType], 0, sizeof(m_CommittedD3D11UAVResources[ShaderType][0])*m_NumCommittedUAVs[ShaderType] );
            m_NumCommittedCBs[ShaderType] = 0;
            m_NumCommittedSRVs[ShaderType] = 0;
            m_NumCommittedSamplers[ShaderType] = 0;
            m_NumCommittedUAVs[ShaderType] = 0;
        }

        if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
        {
            dbgVerifyCommittedSRVs();
            dbgVerifyCommittedUAVs();
            dbgVerifyCommittedSamplers();
            dbgVerifyCommittedCBs();
        }

        // We do not unbind vertex buffers and index buffer as this can explicitly 
        // be done by the user
    }


    void DeviceContextD3D11Impl::FinishCommandList(ICommandList **ppCommandList)
    {
        CComPtr<ID3D11CommandList> pd3d11CmdList;
        m_pd3d11DeviceContext->FinishCommandList(
            FALSE, // A Boolean flag that determines whether the runtime saves deferred context state before it 
                   // executes FinishCommandList and restores it afterwards. 
                   // * TRUE indicates that the runtime needs to save and restore the state. 
                   // * FALSE indicates that the runtime will not save or restore any state. 
                   //   In this case, the deferred context will return to its default state 
                   //   after the call to FinishCommandList() completes as if 
                   //   ID3D11DeviceContext::ClearState() was called. 
            &pd3d11CmdList);

        CommandListD3D11Impl *pCmdListD3D11( NEW_RC_OBJ(m_CmdListAllocator, "CommandListD3D11Impl instance", CommandListD3D11Impl)(m_pDevice, pd3d11CmdList) );
        pCmdListD3D11->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );

        // Device context is now in default state
        InvalidateState();

        if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
        {
            // Verify bindings
            dbgVerifyCommittedSRVs();
            dbgVerifyCommittedUAVs();
            dbgVerifyCommittedSamplers();
            dbgVerifyCommittedCBs();
            dbgVerifyCommittedVertexBuffers();
            dbgVerifyCommittedIndexBuffer();
            dbgVerifyCommittedShaders();
        }
    }

    void DeviceContextD3D11Impl::ExecuteCommandList(ICommandList *pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR("Only immediate context can execute command list");
            return;
        }

        CommandListD3D11Impl* pCmdListD3D11 = ValidatedCast<CommandListD3D11Impl>(pCommandList);
        auto *pd3d11CmdList = pCmdListD3D11->GetD3D11CommandList();
        m_pd3d11DeviceContext->ExecuteCommandList(pd3d11CmdList, 
            FALSE // A Boolean flag that determines whether the target context state is 
                  // saved prior to and restored after the execution of a command list. 
                  // * TRUE indicates that the runtime needs to save and restore the state. 
                  // * FALSE indicate that no state shall be saved or restored, which causes the 
                  //   target context to return to its default state after the command list executes as if
                  //   ID3D11DeviceContext::ClearState() was called.
            );

        // Device context is now in default state
        InvalidateState();

        if(m_DebugFlags & (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance)
        {
            // Verify bindings
            dbgVerifyCommittedSRVs();
            dbgVerifyCommittedUAVs();
            dbgVerifyCommittedSamplers();
            dbgVerifyCommittedCBs();
            dbgVerifyCommittedVertexBuffers();
            dbgVerifyCommittedIndexBuffer();
            dbgVerifyCommittedShaders();
        }
    }
       
    void DeviceContextD3D11Impl::ClearStateCache()
    {
        TDeviceContextBase::ClearStateCache();
        
        for( int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType )
        {
            memset(m_CommittedD3D11CBs     [ShaderType], 0, sizeof(m_CommittedD3D11CBs     [ShaderType][0])*m_NumCommittedCBs     [ShaderType] );
            memset(m_CommittedD3D11SRVs    [ShaderType], 0, sizeof(m_CommittedD3D11SRVs    [ShaderType][0])*m_NumCommittedSRVs    [ShaderType] );
            memset(m_CommittedD3D11Samplers[ShaderType], 0, sizeof(m_CommittedD3D11Samplers[ShaderType][0])*m_NumCommittedSamplers[ShaderType] );
            memset(m_CommittedD3D11UAVs    [ShaderType], 0, sizeof(m_CommittedD3D11UAVs    [ShaderType][0])*m_NumCommittedUAVs    [ShaderType] );
            memset(m_CommittedD3D11SRVResources[ShaderType], 0, sizeof(m_CommittedD3D11SRVResources[ShaderType][0])*m_NumCommittedSRVs[ShaderType] );
            memset(m_CommittedD3D11UAVResources[ShaderType], 0, sizeof(m_CommittedD3D11UAVResources[ShaderType][0])*m_NumCommittedUAVs[ShaderType] );

            m_NumCommittedCBs     [ShaderType] = 0;
            m_NumCommittedSRVs    [ShaderType] = 0;
            m_NumCommittedSamplers[ShaderType] = 0;
            m_NumCommittedUAVs    [ShaderType] = 0;

            m_CommittedD3DShaders[ShaderType].Release();
        }

        for(Uint32 vb=0; vb < m_NumCommittedD3D11VBs; ++vb)
        {
            m_CommittedD3D11VertexBuffers[vb] = nullptr;
            m_CommittedD3D11VBStrides    [vb] = 0;
            m_CommittedD3D11VBOffsets    [vb] = 0;
        }
        m_NumCommittedD3D11VBs = 0;
        m_bCommittedD3D11VBsUpToDate = false;
        
        m_CommittedD3D11InputLayout = nullptr;
        
        m_CommittedD3D11IndexBuffer.Release();
        m_CommittedIBFormat = VT_UNDEFINED;
        m_CommittedD3D11IndexDataStartOffset = 0;
        m_bCommittedD3D11IBUpToDate = false;

        m_CommittedD3D11PrimTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        m_CommittedPrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    void DeviceContextD3D11Impl::InvalidateState()
    {
        TDeviceContextBase::InvalidateState();

        ReleaseCommittedShaderResources();
        for (int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType)
            m_CommittedD3DShaders[ShaderType].Release();
        m_pd3d11DeviceContext->VSSetShader(nullptr, nullptr, 0);
        m_pd3d11DeviceContext->GSSetShader(nullptr, nullptr, 0);
        m_pd3d11DeviceContext->PSSetShader(nullptr, nullptr, 0);
        m_pd3d11DeviceContext->HSSetShader(nullptr, nullptr, 0);
        m_pd3d11DeviceContext->DSSetShader(nullptr, nullptr, 0);
        m_pd3d11DeviceContext->CSSetShader(nullptr, nullptr, 0);
        ID3D11RenderTargetView *d3d11NullRTV[] = {nullptr};
        m_pd3d11DeviceContext->OMSetRenderTargets(1, d3d11NullRTV, nullptr);

        if (m_NumCommittedD3D11VBs > 0)
        {
            for (Uint32 vb = 0; vb < m_NumCommittedD3D11VBs; ++vb)
            {
                m_CommittedD3D11VertexBuffers[vb] = nullptr;
                m_CommittedD3D11VBStrides[vb] = 0;
                m_CommittedD3D11VBOffsets[vb] = 0;
            }
            m_pd3d11DeviceContext->IASetVertexBuffers(0, m_NumCommittedD3D11VBs, m_CommittedD3D11VertexBuffers, m_CommittedD3D11VBStrides, m_CommittedD3D11VBOffsets);
            m_NumCommittedD3D11VBs = 0;
        }
        
        m_bCommittedD3D11VBsUpToDate = false;
        
        if (m_CommittedD3D11InputLayout != nullptr)
        {
            m_pd3d11DeviceContext->IASetInputLayout(nullptr);
            m_CommittedD3D11InputLayout = nullptr;
        }
        
        if (m_CommittedD3D11IndexBuffer)
        {
            m_pd3d11DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
            m_CommittedD3D11IndexBuffer.Release();
        }

        m_CommittedIBFormat = VT_UNDEFINED;
        m_CommittedD3D11IndexDataStartOffset = 0;
        m_bCommittedD3D11IBUpToDate = false;

        m_CommittedD3D11PrimTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        m_CommittedPrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

#ifdef VERIFY_CONTEXT_BINDINGS
    void DeviceContextD3D11Impl::dbgVerifyRenderTargetFormats()
    {
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }

        TEXTURE_FORMAT BoundRTVFormats[8] = {TEX_FORMAT_UNKNOWN};
        TEXTURE_FORMAT BoundDSVFormat = TEX_FORMAT_UNKNOWN;
        Uint32 NumBoundRTVs = 0;
        if (m_IsDefaultFramebufferBound)
        {
            if (m_pSwapChain)
            {
                BoundRTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
                BoundDSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
                NumBoundRTVs = 1;
            }
            else
            {
                LOG_WARNING_MESSAGE("Failed to get bound render targets and depth-stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }
        else
        {
            NumBoundRTVs = m_NumBoundRenderTargets;
            for (Uint32 rt = 0; rt < NumBoundRTVs; ++rt)
            {
                if (auto pRT = m_pBoundRenderTargets[rt])
                    BoundRTVFormats[rt] = pRT->GetDesc().Format;
                else
                    BoundRTVFormats[rt] = TEX_FORMAT_UNKNOWN;
            }

            BoundDSVFormat = m_pBoundDepthStencil ? m_pBoundDepthStencil->GetDesc().Format : TEX_FORMAT_UNKNOWN;
        }

        const auto &PSODesc = m_pPipelineState->GetDesc();
        const auto &GraphicsPipeline = PSODesc.GraphicsPipeline;
        if (/*GraphicsPipeline.NumRenderTargets != 0 && */GraphicsPipeline.NumRenderTargets != NumBoundRTVs)
        {
            LOG_WARNING_MESSAGE("Number of currently bound render targets (", NumBoundRTVs, ") does not match the number of outputs specified by the PSO \"", PSODesc.Name, "\" (", GraphicsPipeline.NumRenderTargets, "). This is OK on D3D11 device, but will most likely be an issue on D3D12." );
        }

        if (GraphicsPipeline.DepthStencilDesc.DepthEnable && BoundDSVFormat != GraphicsPipeline.DSVFormat)
        {
            LOG_WARNING_MESSAGE("Currently bound depth-stencil buffer format (", GetTextureFormatAttribs(BoundDSVFormat).Name, ") does not match the DSV format specified by the PSO \"", PSODesc.Name, "\" (", GetTextureFormatAttribs(GraphicsPipeline.DSVFormat).Name, "). This is OK on D3D11 device, but will most likely be an issue on D3D12." );
        }
        
        for (Uint32 rt = 0; rt < NumBoundRTVs; ++rt)
        {
            auto BoundFmt = BoundRTVFormats[rt];
            auto PSOFmt = GraphicsPipeline.RTVFormats[rt];
            if (BoundFmt != PSOFmt)
            {
                LOG_WARNING_MESSAGE("Render target bound to slot ", rt, " (", GetTextureFormatAttribs(BoundFmt).Name, ") does not match the RTV format specified by the PSO \"", PSODesc.Name, "\" (", GetTextureFormatAttribs(PSOFmt).Name, "). This is OK on D3D11 device, but will most likely be an issue on D3D12." );
            }
        }
    }

    DEFINE_D3D11CTX_FUNC_POINTERS(GetCBMethods,      GetConstantBuffers)
    DEFINE_D3D11CTX_FUNC_POINTERS(GetSRVMethods,     GetShaderResources)
    DEFINE_D3D11CTX_FUNC_POINTERS(GetSamplerMethods, GetSamplers)

    typedef decltype (&ID3D11DeviceContext::CSGetUnorderedAccessViews) TGetUnorderedAccessViewsType;
    static const TGetUnorderedAccessViewsType GetUAVMethods[] =
    {
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        &ID3D11DeviceContext::CSGetUnorderedAccessViews
    };

    /// \tparam MaxResources - Maximum number of resources that can be bound to D3D11 context
    /// \tparam TD3D11ResourceType - Type of D3D11 resource being checked (ID3D11ShaderResourceView, 
    ///                              ID3D11UnorderedAccessView, ID3D11Buffer or ID3D11SamplerState).
    /// \tparam TGetD3D11ResourcesType - Type of the device context method used to get the bound 
    ///                                  resources
    /// \param CommittedD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 resources, for each shader stage
    /// \param GetD3D11ResMethods - Pointer to the array of device context methods to get the bound
    ///                             resources, for each shader stage
    /// \param ResourceName - Resource name
    /// \param ShaderType - Shader type for which to check the resources. If Diligent::SHADER_TYPE_UNKNOWN
    ///                     is provided, all shader stages will be checked
    template<UINT MaxResources, 
             typename TD3D11ResourceType, 
             typename TGetD3D11ResourcesType>
    void DeviceContextD3D11Impl::dbgVerifyCommittedResources(TD3D11ResourceType CommittedD3D11ResourcesArr[][MaxResources],
                                                             Uint8 NumCommittedResourcesArr[],
                                                             TGetD3D11ResourcesType GetD3D11ResMethods[],
                                                             const Char *ResourceName,
                                                             SHADER_TYPE ShaderType)
    {
        int iStartInd = 0, iEndInd = NumShaderTypes;
        if( ShaderType != SHADER_TYPE_UNKNOWN )
        {
            iStartInd = GetShaderTypeIndex(ShaderType);
            iEndInd = iStartInd + 1;
        }
        for( int iShaderTypeInd = iStartInd; iShaderTypeInd < iEndInd; ++iShaderTypeInd )
        {
            const auto ShaderName = GetShaderTypeLiteralName( GetShaderTypeFromIndex(iShaderTypeInd) );
            TD3D11ResourceType pctxResources[MaxResources] = {};
            auto GetResMethod = GetD3D11ResMethods[iShaderTypeInd];
            if( GetResMethod )
            {
                (m_pd3d11DeviceContext->*GetResMethod)(0, _countof( pctxResources ), pctxResources);
            }
            const auto* CommittedResources = CommittedD3D11ResourcesArr[iShaderTypeInd];
            auto NumCommittedResources = NumCommittedResourcesArr[iShaderTypeInd];
            for( Uint32 Slot = 0; Slot < _countof( pctxResources ); ++Slot )
            {
                if( Slot < NumCommittedResources )
                {
                    VERIFY( CommittedResources[Slot] == pctxResources[Slot], ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot );
                }
                else
                {
                    VERIFY( pctxResources[Slot] == nullptr, ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot );
                    VERIFY( CommittedResources[Slot] == nullptr, ResourceName, " unexpected non-null resource found for ", ShaderName, " shader type at slot ", Slot );
                }
                
                if( pctxResources[Slot] )
                    pctxResources[Slot]->Release();
            }
        }
    }

    template<UINT MaxResources, typename TD3D11ViewType>
    void DeviceContextD3D11Impl::dbgVerifyViewConsistency(TD3D11ViewType CommittedD3D11ViewArr[][MaxResources],
                                                          ID3D11Resource* CommittedD3D11ResourcesArr[][MaxResources],
                                                          Uint8 NumCommittedResourcesArr[],
                                                          const Char *ResourceName,
                                                          SHADER_TYPE ShaderType)
    {
        int iStartInd = 0, iEndInd = NumShaderTypes;
        if( ShaderType != SHADER_TYPE_UNKNOWN )
        {
            iStartInd = GetShaderTypeIndex(ShaderType);
            iEndInd = iStartInd + 1;
        }
        for( int iShaderTypeInd = iStartInd; iShaderTypeInd < iEndInd; ++iShaderTypeInd )
        {
            const auto ShaderName = GetShaderTypeLiteralName( GetShaderTypeFromIndex(iShaderTypeInd) );
            auto* Views = CommittedD3D11ViewArr[iShaderTypeInd];
            auto* Resources = CommittedD3D11ResourcesArr[iShaderTypeInd];
            auto NumCommittedResources = NumCommittedResourcesArr[iShaderTypeInd];
            for( Uint32 Slot = 0; Slot < NumCommittedResources; ++Slot )
            {
                if(Views[Slot] != nullptr)
                {
                    CComPtr<ID3D11Resource> pRefRes;
                    Views[Slot]->GetResource(&pRefRes);
                    VERIFY( pRefRes == Resources[Slot], "Inconsistent ", ResourceName, " detected at slot ", Slot, " in shader ", ShaderName, ". The resource in the view does not match cached D3D11 resource" );
                }
            }
        }
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedSRVs(SHADER_TYPE ShaderType)
    {
        dbgVerifyCommittedResources<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>( m_CommittedD3D11SRVs, m_NumCommittedSRVs, GetSRVMethods, "SRV", ShaderType );
        dbgVerifyViewConsistency<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>( m_CommittedD3D11SRVs, m_CommittedD3D11SRVResources, m_NumCommittedSRVs, "SRV", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedUAVs(SHADER_TYPE ShaderType)
    {
        dbgVerifyCommittedResources<D3D11_PS_CS_UAV_REGISTER_COUNT>( m_CommittedD3D11UAVs, m_NumCommittedUAVs, GetUAVMethods, "UAV", ShaderType );
        dbgVerifyViewConsistency<D3D11_PS_CS_UAV_REGISTER_COUNT>( m_CommittedD3D11UAVs, m_CommittedD3D11UAVResources, m_NumCommittedUAVs, "UAV", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedSamplers(SHADER_TYPE ShaderType)
    {
        dbgVerifyCommittedResources<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>( m_CommittedD3D11Samplers, m_NumCommittedSamplers, GetSamplerMethods, "Sampler", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedCBs(SHADER_TYPE ShaderType)
    {
        dbgVerifyCommittedResources<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>( m_CommittedD3D11CBs, m_NumCommittedCBs, GetCBMethods, "Constant buffer", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedIndexBuffer()
    {
        RefCntAutoPtr<ID3D11Buffer> pctxIndexBuffer;
        DXGI_FORMAT Fmt = DXGI_FORMAT_UNKNOWN;
        UINT Offset = 0;
        m_pd3d11DeviceContext->IAGetIndexBuffer( &pctxIndexBuffer, &Fmt, &Offset );
        if( m_CommittedD3D11IndexBuffer && !pctxIndexBuffer )
            UNEXPECTED( "D3D11 index buffer is not bound to the context" );
        if( !m_CommittedD3D11IndexBuffer && pctxIndexBuffer )
            UNEXPECTED( "Unexpected D3D11 index buffer is bound to the context" );

        if( m_CommittedD3D11IndexBuffer && pctxIndexBuffer )
        {
            VERIFY(m_CommittedD3D11IndexBuffer == pctxIndexBuffer, "Index buffer binding mismatch detected");
            if( Fmt==DXGI_FORMAT_R32_UINT )
            {
                VERIFY(m_CommittedIBFormat == VT_UINT32, "Index buffer format mismatch detected");
            }
            else if( Fmt==DXGI_FORMAT_R16_UINT )
            {
                VERIFY(m_CommittedIBFormat == VT_UINT16, "Index buffer format mismatch detected");
            }
            VERIFY(m_CommittedD3D11IndexDataStartOffset == Offset, "Index buffer offset mismatch detected");
        }
    }

    void DeviceContextD3D11Impl::dbgVerifyCommittedVertexBuffers()
    {
        CComPtr<ID3D11InputLayout> pInputLayout;
        m_pd3d11DeviceContext->IAGetInputLayout(&pInputLayout);
        VERIFY( pInputLayout == m_CommittedD3D11InputLayout, "Inconsistent input layout" );

        const Uint32 MaxVBs = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        ID3D11Buffer *pVBs[MaxVBs];
        UINT Strides[MaxVBs];
        UINT Offsets[MaxVBs];
        m_pd3d11DeviceContext->IAGetVertexBuffers( 0, MaxVBs, pVBs, Strides, Offsets );
        auto NumBoundVBs = m_NumCommittedD3D11VBs;
        for( Uint32 Slot = 0; Slot < MaxVBs; ++Slot )
        {
            if( Slot < NumBoundVBs )
            {
                const auto &BoundD3D11VB = m_CommittedD3D11VertexBuffers[Slot];
                auto BoundVBStride = m_CommittedD3D11VBStrides[Slot];
                auto BoundVBOffset = m_CommittedD3D11VBOffsets[Slot];
                if(BoundD3D11VB && !pVBs[Slot] )
                    VERIFY( pVBs[Slot] == nullptr, "Missing D3D11 buffer detected at slot ", Slot );
                if(!BoundD3D11VB && pVBs[Slot] )
                    VERIFY( pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot );
                if( BoundD3D11VB && pVBs[Slot] )
                {
                    VERIFY( BoundD3D11VB == pVBs[Slot], "Vertex buffer mismatch detected at slot ", Slot );
                    VERIFY( BoundVBOffset == Offsets[Slot], "Offset mismatch detected at slot ", Slot );
                    VERIFY( BoundVBStride == Strides[Slot], "Stride mismatch detected at slot ", Slot );
                }
            }
            else
            {
                VERIFY( pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot );
            }

            if( pVBs[Slot] )
                pVBs[Slot]->Release();
        }
    }

    template<typename TD3D11ShaderType, typename TGetShaderMethodType>
    void dbgVerifyCommittedShadersHelper(SHADER_TYPE ShaderType,
                                const CComPtr<ID3D11DeviceChild> BoundD3DShaders[], 
                                ID3D11DeviceContext *pCtx,
                                TGetShaderMethodType GetShaderMethod)
    {
        RefCntAutoPtr<TD3D11ShaderType> pctxShader;
        (pCtx->*GetShaderMethod)(&pctxShader, nullptr, nullptr);
        const auto &BoundShader = BoundD3DShaders[GetShaderTypeIndex( ShaderType )];
        VERIFY( BoundShader == pctxShader, GetShaderTypeLiteralName(ShaderType), " binding mismatch detected" );
    }
    void DeviceContextD3D11Impl::dbgVerifyCommittedShaders()
    {
#define VERIFY_SHADER(NAME, Name, N) dbgVerifyCommittedShadersHelper<ID3D11##Name##Shader>(SHADER_TYPE_##NAME, m_CommittedD3DShaders, m_pd3d11DeviceContext, &ID3D11DeviceContext::N##SGetShader )
        // These shaders which are not set will be unbound from the D3D11 device context
        VERIFY_SHADER( VERTEX,   Vertex,   V );
        VERIFY_SHADER( PIXEL,    Pixel,    P );
        VERIFY_SHADER( GEOMETRY, Geometry, G );
        VERIFY_SHADER( DOMAIN,   Domain,   D );
        VERIFY_SHADER( HULL,     Hull,     H );
        VERIFY_SHADER( COMPUTE,  Compute,  C );
    }

#endif
}
