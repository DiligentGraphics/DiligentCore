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

#include <d3dcompiler.h>
#include "ShaderResourcesD3D11.h"
#include "ShaderBase.h"
#include "D3DShaderResourceLoader.h"
#include "ShaderResourceCacheD3D11.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{


ShaderResourcesD3D11::ShaderResourcesD3D11(RenderDeviceD3D11Impl *pDeviceD3D11Impl, ID3DBlob *pShaderBytecode, const ShaderDesc &ShdrDesc) :
    ShaderResources(GetRawAllocator(), ShdrDesc.ShaderType),
    m_ShaderName(ShdrDesc.Name),
    m_StaticSamplers(nullptr, STDDeleterRawMem< void >(GetRawAllocator()))
{
    Uint32 CurrCB = 0, CurrTexSRV = 0, CurrTexUAV = 0, CurrBufSRV = 0, CurrBufUAV = 0, CurrSampler = 0;
    LoadD3DShaderResources<D3D11_SHADER_DESC, D3D11_SHADER_INPUT_BIND_DESC, ID3D11ShaderReflection>(
        pShaderBytecode,

        [&](Uint32 NumCBs, Uint32 NumTexSRVs, Uint32 NumTexUAVs, Uint32 NumBufSRVs, Uint32 NumBufUAVs, Uint32 NumSamplers)
        {
            Initialize(GetRawAllocator(), NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers);
        },

        [&](D3DShaderResourceAttribs&& CBAttribs)
        {
            VERIFY( CBAttribs.BindPoint + CBAttribs.BindCount-1 <= MaxAllowedBindPoint, "CB bind point exceeds supported range" )
            m_MaxCBBindPoint = std::max(m_MaxCBBindPoint, static_cast<MaxBindPointType>(CBAttribs.BindPoint + CBAttribs.BindCount-1));

            new (&GetCB(CurrCB++)) D3DShaderResourceAttribs(std::move(CBAttribs));
        },

        [&](D3DShaderResourceAttribs &&TexUAV)
        {
            VERIFY( TexUAV.BindPoint + TexUAV.BindCount-1 <= MaxAllowedBindPoint, "Tex UAV bind point exceeds supported range" )
            m_MaxUAVBindPoint = std::max(m_MaxUAVBindPoint, static_cast<MaxBindPointType>(TexUAV.BindPoint + TexUAV.BindCount-1));

            new (&GetTexUAV(CurrTexUAV++)) D3DShaderResourceAttribs( std::move(TexUAV) );
        },

        [&](D3DShaderResourceAttribs &&BuffUAV)
        {
            VERIFY( BuffUAV.BindPoint + BuffUAV.BindCount-1 <= MaxAllowedBindPoint, "Buff UAV bind point exceeds supported range" )
            m_MaxUAVBindPoint = std::max(m_MaxUAVBindPoint, static_cast<MaxBindPointType>(BuffUAV.BindPoint + BuffUAV.BindCount-1));

            new (&GetBufUAV(CurrBufUAV++)) D3DShaderResourceAttribs( std::move(BuffUAV) );
        },

        [&](D3DShaderResourceAttribs &&BuffSRV)
        {
            VERIFY( BuffSRV.BindPoint + BuffSRV.BindCount-1 <= MaxAllowedBindPoint, "Buff SRV bind point exceeds supported range" )
            m_MaxSRVBindPoint = std::max(m_MaxSRVBindPoint, static_cast<MaxBindPointType>(BuffSRV.BindPoint + BuffSRV.BindCount-1));

            new (&GetBufSRV(CurrBufSRV++)) D3DShaderResourceAttribs( std::move(BuffSRV) );
        },

        [&](D3DShaderResourceAttribs &&SamplerAttribs)
        {
            VERIFY( SamplerAttribs.BindPoint + SamplerAttribs.BindCount-1 <= MaxAllowedBindPoint, "Sampler bind point exceeds supported range" )
            m_MaxSamplerBindPoint = std::max(m_MaxSamplerBindPoint, static_cast<MaxBindPointType>(SamplerAttribs.BindPoint + SamplerAttribs.BindCount-1));
            m_NumStaticSamplers += SamplerAttribs.IsStaticSampler() ? 1 : 0;

            new (&GetSampler(CurrSampler++)) D3DShaderResourceAttribs( std::move(SamplerAttribs) );
        },

        [&](D3DShaderResourceAttribs &&TexAttribs)
        {
            VERIFY(CurrSampler == GetNumSamplers(), "All samplers must be initialized before texture SRVs" );

            VERIFY( TexAttribs.BindPoint + TexAttribs.BindCount-1 <= MaxAllowedBindPoint, "Tex SRV bind point exceeds supported range" )
            m_MaxSRVBindPoint = std::max(m_MaxSRVBindPoint, static_cast<MaxBindPointType>(TexAttribs.BindPoint + TexAttribs.BindCount-1));

            auto SamplerId = FindAssignedSamplerId(TexAttribs);
            new (&GetTexSRV(CurrTexSRV++)) D3DShaderResourceAttribs( std::move(TexAttribs), SamplerId);
        },

        ShdrDesc,
        D3DSamplerSuffix);

    VERIFY(CurrCB == GetNumCBs(), "Not all CBs are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called");
    VERIFY(CurrTexSRV == GetNumTexSRV(), "Not all Tex SRVs are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrTexUAV == GetNumTexUAV(), "Not all Tex UAVs are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrBufSRV == GetNumBufSRV(), "Not all Buf SRVs are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrBufUAV == GetNumBufUAV(), "Not all Buf UAVs are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called" );
    VERIFY(CurrSampler == GetNumSamplers(), "Not all Samplers are initialized, which will result in a crash when ~D3DShaderResourceAttribs() is called" );

    // Create static samplers
    if (m_NumStaticSamplers > 0)
    {
        auto MemSize = m_NumStaticSamplers * sizeof(StaticSamplerAttribs);
        auto *pRawMem = ALLOCATE(GetRawAllocator(), "Allocator for array of RefCntAutoPtr<ISampler>", MemSize);
        m_StaticSamplers.reset( pRawMem );
        for (Uint32 i = 0; i < m_NumStaticSamplers; ++i)
        {
            new (&GetStaticSampler(i)) StaticSamplerAttribs;
        }
        Uint32 CurrStaticSam = 0;
        for (Uint32 s = 0; s < GetNumSamplers(); ++s)
        {
            const auto &Sam = GetSampler(s);
            if (Sam.IsStaticSampler())
            {
                Uint32 ssd = 0;
                for (; ssd < ShdrDesc.NumStaticSamplers; ++ssd)
                {
                    const auto& StaticSamplerDesc = ShdrDesc.StaticSamplers[ssd];
                    if ( StrCmpSuff(Sam.Name.c_str(), StaticSamplerDesc.TextureName, D3DSamplerSuffix))
                    {
                        auto &StaticSamplerAttrs = GetStaticSampler(CurrStaticSam++);
                        StaticSamplerAttrs.first = &Sam;
                        pDeviceD3D11Impl->CreateSampler(StaticSamplerDesc.Desc, &StaticSamplerAttrs.second);
                        break;
                    }
                }
                VERIFY(ssd < ShdrDesc.NumStaticSamplers, "Static sampler was not found!")
            }
        }
        VERIFY_EXPR(CurrStaticSam == m_NumStaticSamplers);
    }
}

ShaderResourcesD3D11::~ShaderResourcesD3D11()
{
    for(Uint32 ss=0; ss < m_NumStaticSamplers; ++ss)
        GetStaticSampler(ss).~StaticSamplerAttribs();
}

void ShaderResourcesD3D11::InitStaticSamplers(ShaderResourceCacheD3D11 &ResourceCache)const
{
    auto NumCachedSamplers = ResourceCache.GetSamplerCount();
    for(Uint32 ss=0; ss < m_NumStaticSamplers; ++ss)
    {
        auto &StaticSampler = const_cast<ShaderResourcesD3D11*>(this)->GetStaticSampler(ss);
        const auto *pSamAttribs = StaticSampler.first;
        auto EndBindPoint = std::min( static_cast<Uint32>(pSamAttribs->BindPoint) + pSamAttribs->BindCount, NumCachedSamplers);
        for(Uint32 BindPoint = pSamAttribs->BindPoint; BindPoint < EndBindPoint; ++BindPoint )
            ResourceCache.SetSampler(BindPoint, ValidatedCast<SamplerD3D11Impl>(StaticSampler.second.RawPtr()) );
    }
}

#ifdef VERIFY_SHADER_BINDINGS
static String DbgMakeResourceName(const D3DShaderResourceAttribs &Attr, Uint32 BindPoint)
{
    VERIFY( BindPoint >= (Uint32)Attr.BindPoint && BindPoint < (Uint32)Attr.BindPoint + Attr.BindCount, "Bind point is out of allowed range")
    if(Attr.BindCount == 1)
        return Attr.Name;
    else
        return String(Attr.Name) + '[' + std::to_string(BindPoint - Attr.BindPoint) + ']';
}

void ShaderResourcesD3D11::dbgVerifyCommittedResources(ID3D11Buffer*              CommittedD3D11CBs[],
                                                       ID3D11ShaderResourceView*  CommittedD3D11SRVs[],
                                                       ID3D11Resource*            CommittedD3D11SRVResources[],
                                                       ID3D11SamplerState*        CommittedD3D11Samplers[],
                                                       ID3D11UnorderedAccessView* CommittedD3D11UAVs[],
                                                       ID3D11Resource*            CommittedD3D11UAVResources[],
                                                       ShaderResourceCacheD3D11 &ResourceCache)const
{
    ShaderResourceCacheD3D11::CachedCB* CachedCBs = nullptr;
    ID3D11Buffer** d3d11CBs = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedSRVResources = nullptr;
    ID3D11ShaderResourceView** d3d11SRVs = nullptr;
    ShaderResourceCacheD3D11::CachedSampler* CachedSamplers = nullptr;
    ID3D11SamplerState** d3d11Samplers = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedUAVResources = nullptr;
    ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
    ResourceCache.GetResourceArrays(CachedCBs, d3d11CBs, CachedSRVResources, d3d11SRVs, CachedSamplers, d3d11Samplers, CachedUAVResources, d3d11UAVs);

    ProcessResources(
        nullptr, 0,

        [&](const D3DShaderResourceAttribs &cb)
        {
            for(auto BindPoint = cb.BindPoint; BindPoint < cb.BindPoint + cb.BindCount; ++BindPoint)
            {
                if (BindPoint >= ResourceCache.GetCBCount())
                {
                    LOG_ERROR_MESSAGE( "Unable to find constant buffer \"", DbgMakeResourceName(cb,BindPoint), "\" (slot ", BindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetCBCount()," CB slots only. This should never happen and may be the result of using wrong resource cache." );
                    continue;
                }
                auto &CB = CachedCBs[BindPoint];
                if(CB.pBuff == nullptr)
                {
                    LOG_ERROR_MESSAGE( "Constant buffer \"", DbgMakeResourceName(cb,BindPoint), "\" (slot ", BindPoint, ") is not initialized in the resource cache." );
                    continue;
                }

                if (!(CB.pBuff->GetDesc().BindFlags & BIND_UNIFORM_BUFFER))
                {
                    LOG_ERROR_MESSAGE( "Buffer \"", CB.pBuff->GetDesc().Name, "\" committed in the device context as constant buffer to variable \"", DbgMakeResourceName(cb,BindPoint), "\" (slot ", BindPoint, ") in shader \"", GetShaderName(), "\" does not have BIND_UNIFORM_BUFFER flag" );
                    continue;
                }

                VERIFY_EXPR(d3d11CBs[BindPoint] == CB.pBuff->GetD3D11Buffer());

                if(CommittedD3D11CBs[BindPoint] == nullptr )
                {
                    LOG_ERROR_MESSAGE( "No D3D11 resource committed to constant buffer \"", DbgMakeResourceName(cb,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\"" );
                    continue;
                }

                if(CommittedD3D11CBs[BindPoint] != d3d11CBs[BindPoint] )
                {
                    LOG_ERROR_MESSAGE( "D3D11 resource committed to constant buffer \"", DbgMakeResourceName(cb,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                    continue;
                }
            }
        },

        [&](const D3DShaderResourceAttribs& tex)
        {
            for(auto BindPoint = tex.BindPoint; BindPoint < tex.BindPoint + tex.BindCount; ++BindPoint)
            {
                if (BindPoint >= ResourceCache.GetSRVCount())
                {
                    LOG_ERROR_MESSAGE( "Unable to find texture SRV \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetSRVCount()," SRV slots only. This should never happen and may be the result of using wrong resource cache." );
                    continue;
                }
                auto &SRVRes = CachedSRVResources[BindPoint];
                if(SRVRes.pBuffer != nullptr)
                {
                    LOG_ERROR_MESSAGE( "Unexpected buffer bound to variable \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint, "). Texture is expected." );
                    continue;
                }
                if(SRVRes.pTexture == nullptr)
                {
                    LOG_ERROR_MESSAGE( "Texture \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint, ") is not initialized in the resource cache." );
                    continue;
                }

                if (!(SRVRes.pTexture->GetDesc().BindFlags & BIND_SHADER_RESOURCE))
                {
                    LOG_ERROR_MESSAGE( "Texture \"", SRVRes.pTexture->GetDesc().Name, "\" committed in the device context as SRV to variable \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint, ") in shader \"", GetShaderName(), "\" does not have BIND_SHADER_RESOURCE flag" );
                }

                if(CommittedD3D11SRVs[BindPoint] == nullptr )
                {
                    LOG_ERROR_MESSAGE( "No D3D11 resource committed to texture SRV \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\"" );
                    continue;
                }

                if(CommittedD3D11SRVs[BindPoint] != d3d11SRVs[BindPoint] )
                {
                    LOG_ERROR_MESSAGE( "D3D11 resource committed to texture SRV \"", DbgMakeResourceName(tex,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                    continue;
                }
            }

            if( tex.IsValidSampler() )
            {
                const auto& SamAttribs = GetSampler( tex.GetSamplerId() );
                VERIFY_EXPR(SamAttribs.IsValidBindPoint());
                VERIFY_EXPR(SamAttribs.BindCount == 1 || SamAttribs.BindCount == tex.BindCount);

                for(auto SamBindPoint = SamAttribs.BindPoint; SamBindPoint < SamAttribs.BindPoint + SamAttribs.BindCount; ++SamBindPoint)
                {
                    if (SamBindPoint >= ResourceCache.GetSamplerCount())
                    {
                        LOG_ERROR_MESSAGE( "Unable to find sampler \"", DbgMakeResourceName(SamAttribs,SamBindPoint), "\" (slot ", SamBindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetSamplerCount()," Sampler slots only. This should never happen and may be the result of using wrong resource cache." );
                        continue;
                    }
                    auto &Sam = CachedSamplers[SamBindPoint];
                    if(Sam.pSampler == nullptr)
                    {
                        LOG_ERROR_MESSAGE( "Sampler \"", DbgMakeResourceName(SamAttribs,SamBindPoint), "\" (slot ", SamBindPoint, ") is not initialized in the resource cache." );
                        continue;
                    }
                    VERIFY_EXPR(d3d11Samplers[SamBindPoint] == Sam.pSampler->GetD3D11SamplerState());

                    if(CommittedD3D11Samplers[SamBindPoint] == nullptr )
                    {
                        LOG_ERROR_MESSAGE( "No D3D11 sampler committed to variable \"", DbgMakeResourceName(SamAttribs,SamBindPoint), "\" (slot ", SamBindPoint ,") in shader \"", GetShaderName(), "\"" );
                        continue;
                    }

                    if(CommittedD3D11Samplers[SamBindPoint] != d3d11Samplers[SamBindPoint])
                    {
                        LOG_ERROR_MESSAGE( "D3D11 sampler committed to variable \"", DbgMakeResourceName(SamAttribs,SamBindPoint), "\" (slot ", SamBindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                        continue;
                    }
                }
            }
        },

        [&](const D3DShaderResourceAttribs &uav)
        {
            for(auto BindPoint = uav.BindPoint; BindPoint < uav.BindPoint + uav.BindCount; ++BindPoint)
            {
                if (BindPoint >= ResourceCache.GetUAVCount())
                {
                    LOG_ERROR_MESSAGE( "Unable to find texture UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetUAVCount()," UAV slots only. This should never happen and may be the result of using wrong resource cache." );
                    continue;
                }
                auto &UAVRes = CachedUAVResources[BindPoint];
                if(UAVRes.pBuffer != nullptr)
                {
                    LOG_ERROR_MESSAGE( "Unexpected buffer bound to variable \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, "). Texture is expected." );
                    continue;
                }
                if(UAVRes.pTexture == nullptr)
                {
                    LOG_ERROR_MESSAGE( "Texture \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") is not initialized in the resource cache." );
                    continue;
                }

                if (!(UAVRes.pTexture->GetDesc().BindFlags & BIND_UNORDERED_ACCESS))
                {
                    LOG_ERROR_MESSAGE( "Texture \"", UAVRes.pTexture->GetDesc().Name, "\" committed in the device context as UAV to variable \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") in shader \"", GetShaderName(), "\" does not have BIND_UNORDERED_ACCESS flag" );
                }

                if(CommittedD3D11UAVs[BindPoint] == nullptr )
                {
                    LOG_ERROR_MESSAGE( "No D3D11 resource committed to texture UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\"" );
                    continue;
                }

                if(CommittedD3D11UAVs[BindPoint] != d3d11UAVs[BindPoint] )
                {
                    LOG_ERROR_MESSAGE( "D3D11 resource committed to texture UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                    continue;
                }
            }
        },


        [&](const D3DShaderResourceAttribs &buf)
        {
            for(auto BindPoint = buf.BindPoint; BindPoint < buf.BindPoint + buf.BindCount; ++BindPoint)
            {
                if (BindPoint >= ResourceCache.GetSRVCount())
                {
                    LOG_ERROR_MESSAGE( "Unable to find buffer SRV \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetSRVCount()," SRV slots only. This should never happen and may be the result of using wrong resource cache." );
                    continue;
                }
                auto &SRVRes = CachedSRVResources[BindPoint];
                if(SRVRes.pTexture != nullptr)
                {
                    LOG_ERROR_MESSAGE( "Unexpected texture bound to variable \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint, "). Buffer is expected." );
                    continue;
                }
                if(SRVRes.pBuffer == nullptr)
                {
                    LOG_ERROR_MESSAGE( "Buffer \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint ,") is not initialized in the resource cache." );
                    continue;
                }

                if (!(SRVRes.pBuffer->GetDesc().BindFlags & BIND_SHADER_RESOURCE))
                {
                    LOG_ERROR_MESSAGE( "Buffer \"", SRVRes.pBuffer->GetDesc().Name, "\" committed in the device context as SRV to variable \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint, ") in shader \"", GetShaderName(), "\" does not have BIND_SHADER_RESOURCE flag" );
                }

                if(CommittedD3D11SRVs[BindPoint] == nullptr )
                {
                    LOG_ERROR_MESSAGE( "No D3D11 resource committed to buffer SRV \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\"" );
                    continue;
                }

                if(CommittedD3D11SRVs[BindPoint] != d3d11SRVs[BindPoint] )
                {
                    LOG_ERROR_MESSAGE( "D3D11 resource committed to buffer SRV \"", DbgMakeResourceName(buf,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                    continue;
                }
            }
        },

        [&](const D3DShaderResourceAttribs &uav)
        {
            for(auto BindPoint = uav.BindPoint; BindPoint < uav.BindPoint + uav.BindCount; ++BindPoint)
            {
                if (BindPoint >= ResourceCache.GetUAVCount())
                {
                    LOG_ERROR_MESSAGE( "Unable to find buffer UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") in the resource cache: the cache reserves ", ResourceCache.GetUAVCount()," UAV slots only. This should never happen and may be the result of using wrong resource cache." );
                    continue;
                }
                auto &UAVRes = CachedUAVResources[BindPoint];
                if(UAVRes.pTexture != nullptr)
                {
                    LOG_ERROR_MESSAGE( "Unexpected texture bound to variable \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, "). Buffer is expected." );
                    return;
                }
                if(UAVRes.pBuffer == nullptr)
                {
                    LOG_ERROR_MESSAGE( "Buffer UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") is not initialized in the resource cache." );
                    return;
                }

                if (!(UAVRes.pBuffer->GetDesc().BindFlags & BIND_UNORDERED_ACCESS))
                {
                    LOG_ERROR_MESSAGE( "Buffer \"", UAVRes.pBuffer->GetDesc().Name, "\" committed in the device context as UAV to variable \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint, ") in shader \"", GetShaderName(), "\" does not have BIND_UNORDERED_ACCESS flag" );
                }

                if(CommittedD3D11UAVs[BindPoint] == nullptr )
                {
                    LOG_ERROR_MESSAGE( "No D3D11 resource committed to buffer UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\"" );
                    return;
                }

                if(CommittedD3D11UAVs[BindPoint] != d3d11UAVs[BindPoint] )
                {
                    LOG_ERROR_MESSAGE( "D3D11 resource committed to buffer UAV \"", DbgMakeResourceName(uav,BindPoint), "\" (slot ", BindPoint ,") in shader \"", GetShaderName(), "\" does not match the resource in the resource cache" );
                    return;
                }
            }
        }
    );
}
#endif

}
