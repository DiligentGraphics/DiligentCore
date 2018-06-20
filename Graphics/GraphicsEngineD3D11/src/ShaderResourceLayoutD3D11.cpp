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

#include <d3dcompiler.h>

#include "ShaderResourceLayoutD3D11.h"
#include "ShaderResourceCacheD3D11.h"
#include "BufferD3D11Impl.h"
#include "BufferViewD3D11Impl.h"
#include "TextureBaseD3D11.h"
#include "TextureViewD3D11.h"
#include "SamplerD3D11Impl.h"
#include "D3DShaderResourceLoader.h"
#include "ShaderD3D11Impl.h"

namespace Diligent
{

ShaderResourceLayoutD3D11::ShaderResourceLayoutD3D11(IObject& Owner, IMemoryAllocator& ResLayoutDataAllocator) : 
    m_Owner(Owner),
#if USE_VARIABLE_HASH_MAP
    m_VariableHash(STD_ALLOCATOR_RAW_MEM(VariableHashData, GetRawAllocator(), "Allocator for vector<BuffSRVBindInfo>")),
#endif
    m_ResourceBuffer(nullptr, STDDeleterRawMem<void>(ResLayoutDataAllocator))
{
}

ShaderResourceLayoutD3D11::~ShaderResourceLayoutD3D11()
{
    HandleResources(
        [&](ConstBuffBindInfo&cb)
        {
            cb.~ConstBuffBindInfo();
        },

        [&](TexAndSamplerBindInfo& ts)
        {
            ts.~TexAndSamplerBindInfo();
        },

        [&](TexUAVBindInfo& uav)
        {
            uav.~TexUAVBindInfo();
        },

        [&](BuffSRVBindInfo& srv)
        {
            srv.~BuffSRVBindInfo();
        },

        [&](BuffUAVBindInfo& uav)
        {
            uav.~BuffUAVBindInfo();
        }
    );
}

const D3DShaderResourceAttribs ShaderResourceLayoutD3D11::TexAndSamplerBindInfo::InvalidSamplerAttribs("Invalid sampler", D3DShaderResourceAttribs::InvalidBindPoint, 0, D3D_SIT_SAMPLER,  SHADER_VARIABLE_TYPE_NUM_TYPES, D3D_SRV_DIMENSION_UNKNOWN, D3DShaderResourceAttribs::InvalidSamplerId, false);

void ShaderResourceLayoutD3D11::Initialize(const std::shared_ptr<const ShaderResourcesD3D11>& pSrcResources,
                                           const SHADER_VARIABLE_TYPE*                        VarTypes, 
                                           Uint32                                             NumVarTypes, 
                                           ShaderResourceCacheD3D11&                          ResourceCache,
                                           IMemoryAllocator&                                  ResCacheDataAllocator,
                                           IMemoryAllocator&                                  ResLayoutDataAllocator)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-layout#Shader-Resource-Layout-Initialization

    VERIFY(&m_ResourceBuffer.get_deleter().m_Allocator == &ResLayoutDataAllocator, "Incosistent memory alloctor");

    m_pResources = pSrcResources;
    m_pResourceCache = &ResourceCache;

    // In release mode, MS compiler generates this false warning:
    // Warning	C4189 'AllowedTypeBits': local variable is initialized but not referenced
    // Most likely it somehow gets confused by the variable being eliminated during function inlining
#pragma warning(push)
#pragma warning(disable : 4189)
    Uint32 AllowedTypeBits = GetAllowedTypeBits(VarTypes, NumVarTypes);
#pragma warning(pop)

    // Count total number of resources of allowed types
    Uint32 NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers;
    pSrcResources->CountResources(VarTypes, NumVarTypes, NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers);

    // Initialize offsets
    m_TexAndSamplersOffset = 0                      + static_cast<Uint16>( NumCBs     * sizeof(ConstBuffBindInfo)     );
    m_TexUAVsOffset        = m_TexAndSamplersOffset + static_cast<Uint16>( NumTexSRVs * sizeof(TexAndSamplerBindInfo) );
    m_BuffUAVsOffset       = m_TexUAVsOffset        + static_cast<Uint16>( NumTexUAVs * sizeof(TexUAVBindInfo)        );
    m_BuffSRVsOffset       = m_BuffUAVsOffset       + static_cast<Uint16>( NumBufUAVs * sizeof(BuffUAVBindInfo)       );
    auto MemorySize        = m_BuffSRVsOffset       +                      NumBufSRVs * sizeof(BuffSRVBindInfo);
        
    if( MemorySize )
    {
        auto *pRawMem = ALLOCATE(ResLayoutDataAllocator, "Raw memory buffer for shader resource layout resources", MemorySize);
        m_ResourceBuffer.reset(pRawMem);
    }

    VERIFY_EXPR(NumCBs < 255);
    VERIFY_EXPR(NumTexSRVs < 255);
    VERIFY_EXPR(NumTexUAVs < 255);
    VERIFY_EXPR(NumBufSRVs < 255);
    VERIFY_EXPR(NumBufUAVs < 255);
    m_NumCBs     = static_cast<Uint8>(NumCBs);
    m_NumTexSRVs = static_cast<Uint8>(NumTexSRVs);
    m_NumTexUAVs = static_cast<Uint8>(NumTexUAVs);
    m_NumBufSRVs = static_cast<Uint8>(NumBufSRVs);
    m_NumBufUAVs = static_cast<Uint8>(NumBufUAVs);

    // Current resource index for every resource type
    Uint32 cb = 0;
    Uint32 texSrv = 0;
    Uint32 texUav = 0;
    Uint32 bufSrv = 0;
    Uint32 bufUav = 0;

    Uint32 NumCBSlots = 0;
    Uint32 NumSRVSlots = 0;
    Uint32 NumSamplerSlots = 0;
    Uint32 NumUAVSlots = 0;
    pSrcResources->ProcessResources(
        VarTypes, NumVarTypes,

        [&](const D3DShaderResourceAttribs &CB, Uint32)
        {
            VERIFY_EXPR( IsAllowedType(CB.VariableType, AllowedTypeBits) );
            // Initialize current CB in place, increment CB counter
            new (&GetCB(cb++)) ConstBuffBindInfo( CB, *this );
            NumCBSlots = std::max(NumCBSlots, static_cast<Uint32>(CB.BindPoint + CB.BindCount));
        },

        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            VERIFY_EXPR( IsAllowedType(TexSRV.VariableType, AllowedTypeBits) );
            // Set reference to a special static instance representing invalid sampler            
            // if no sampler is assigned to texture SRV           
            const D3DShaderResourceAttribs& SamplerAttribs = TexSRV.IsValidSampler() ? 
                pSrcResources->GetSampler(TexSRV.GetSamplerId()) : TexAndSamplerBindInfo::InvalidSamplerAttribs;
            // Initialize tex SRV in place, increment counter of tex SRVs
            new (&GetTexSRV(texSrv++)) TexAndSamplerBindInfo( TexSRV, SamplerAttribs, *this );
            NumSRVSlots = std::max(NumSRVSlots, static_cast<Uint32>(TexSRV.BindPoint + TexSRV.BindCount));
            if( SamplerAttribs.IsValidBindPoint() )
                NumSamplerSlots = std::max(NumSamplerSlots, static_cast<Uint32>(SamplerAttribs.BindPoint + SamplerAttribs.BindCount));
        },

        [&](const D3DShaderResourceAttribs &TexUAV, Uint32)
        {
            VERIFY_EXPR( IsAllowedType(TexUAV.VariableType, AllowedTypeBits) );
             
            // Initialize tex UAV in place, increment counter of tex UAVs
            new (&GetTexUAV(texUav++)) TexUAVBindInfo( TexUAV, *this );
            NumUAVSlots = std::max(NumUAVSlots, static_cast<Uint32>(TexUAV.BindPoint + TexUAV.BindCount));
        },

        [&](const D3DShaderResourceAttribs &BuffSRV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(BuffSRV.VariableType, AllowedTypeBits));
            
            // Initialize buff SRV in place, increment counter of buff SRVs
            new (&GetBufSRV(bufSrv++)) BuffSRVBindInfo( BuffSRV, *this );
            NumSRVSlots = std::max(NumSRVSlots, static_cast<Uint32>(BuffSRV.BindPoint + BuffSRV.BindCount));
        },

        [&](const D3DShaderResourceAttribs &BuffUAV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(BuffUAV.VariableType, AllowedTypeBits));
            
            // Initialize buff UAV in place, increment counter of buff UAVs
            new (&GetBufUAV(bufUav++)) BuffUAVBindInfo( BuffUAV, *this );
            NumUAVSlots = std::max(NumUAVSlots, static_cast<Uint32>(BuffUAV.BindPoint + BuffUAV.BindCount));
        }
    );

    // Shader resource cache in the SRB is initialized by the constructor of ShaderResourceBindingD3D11Impl to
    // hold all variable types. The corresponding layout in the SRB is initialized to keep mutable and dynamic 
    // variables only
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache#Shader-Resource-Cache-Initialization
    if (!m_pResourceCache->IsInitialized())
    {
        // NOTE that here we are using max bind points required to cache only the shader variables of allowed types!
        m_pResourceCache->Initialize(NumCBSlots, NumSRVSlots, NumSamplerSlots, NumUAVSlots, ResCacheDataAllocator);
    }

    VERIFY(cb == m_NumCBs, "Not all CBs are initialized, which will result in a crash when dtor is called");
    VERIFY(texSrv == m_NumTexSRVs, "Not all Tex SRVs are initialized, which will result in a crash when dtor is called");
    VERIFY(texUav == m_NumTexUAVs, "Not all Tex UAVs are initialized, which will result in a crash when dtor is called");
    VERIFY(bufSrv == m_NumBufSRVs, "Not all Buf SRVs are initialized, which will result in a crash when dtor is called");
    VERIFY(bufUav == m_NumBufUAVs, "Not all Buf UAVs are initialized, which will result in a crash when dtor is called");
    
    InitVariablesHashMap();
}

void ShaderResourceLayoutD3D11::CopyResources(ShaderResourceCacheD3D11& DstCache)
{
    VERIFY(m_pResourceCache, "Resource cache must not be null");

    VERIFY( DstCache.GetCBCount() >= m_pResourceCache->GetCBCount(), "Dst cache is not large enough to contain all CBs" );
    VERIFY( DstCache.GetSRVCount() >= m_pResourceCache->GetSRVCount(), "Dst cache is not large enough to contain all SRVs" );
    VERIFY( DstCache.GetSamplerCount() >= m_pResourceCache->GetSamplerCount(), "Dst cache is not large enough to contain all samplers" );
    VERIFY( DstCache.GetUAVCount() >= m_pResourceCache->GetUAVCount(), "Dst cache is not large enough to contain all UAVs" );

    ShaderResourceCacheD3D11::CachedCB* CachedCBs = nullptr;
    ID3D11Buffer** d3d11CBs = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedSRVResources = nullptr;
    ID3D11ShaderResourceView** d3d11SRVs = nullptr;
    ShaderResourceCacheD3D11::CachedSampler* CachedSamplers = nullptr;
    ID3D11SamplerState** d3d11Samplers = nullptr;
    ShaderResourceCacheD3D11::CachedResource* CachedUAVResources = nullptr;
    ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
    m_pResourceCache->GetResourceArrays(CachedCBs, d3d11CBs, CachedSRVResources, d3d11SRVs, CachedSamplers, d3d11Samplers, CachedUAVResources, d3d11UAVs);


    ShaderResourceCacheD3D11::CachedCB* DstCBs = nullptr;
    ID3D11Buffer** DstD3D11CBs = nullptr;
    ShaderResourceCacheD3D11::CachedResource* DstSRVResources = nullptr;
    ID3D11ShaderResourceView** DstD3D11SRVs = nullptr;
    ShaderResourceCacheD3D11::CachedSampler* DstSamplers = nullptr;
    ID3D11SamplerState** DstD3D11Samplers = nullptr;
    ShaderResourceCacheD3D11::CachedResource* DstUAVResources = nullptr;
    ID3D11UnorderedAccessView** DstD3D11UAVs = nullptr;
    DstCache.GetResourceArrays(DstCBs, DstD3D11CBs, DstSRVResources, DstD3D11SRVs, DstSamplers, DstD3D11Samplers, DstUAVResources, DstD3D11UAVs);

    HandleResources(
        [&](const ConstBuffBindInfo&cb)
        {
            for(auto CBSlot = cb.Attribs.BindPoint; CBSlot < cb.Attribs.BindPoint+cb.Attribs.BindCount; ++CBSlot)
            {
                VERIFY_EXPR(CBSlot < m_pResourceCache->GetCBCount() && CBSlot < DstCache.GetCBCount());
                DstCBs[CBSlot] = CachedCBs[CBSlot];
                DstD3D11CBs[CBSlot] = d3d11CBs[CBSlot];
            }
        },

        [&](const TexAndSamplerBindInfo& ts)
        {
            for(auto SRVSlot = ts.Attribs.BindPoint; SRVSlot < ts.Attribs.BindPoint + ts.Attribs.BindCount; ++SRVSlot)
            {
                VERIFY_EXPR(SRVSlot < m_pResourceCache->GetSRVCount() && SRVSlot < DstCache.GetSRVCount());
                DstSRVResources[SRVSlot] = CachedSRVResources[SRVSlot];
                DstD3D11SRVs[SRVSlot] = d3d11SRVs[SRVSlot];
                if( ts.SamplerAttribs.IsValidBindPoint() )
                {
                    VERIFY_EXPR(ts.SamplerAttribs.BindCount == ts.Attribs.BindCount || ts.SamplerAttribs.BindCount == 1);
                    Uint32 SamplerSlot = ts.SamplerAttribs.BindPoint + (ts.SamplerAttribs.BindCount == 1 ? 0 : (SRVSlot - ts.Attribs.BindPoint));
                    if( !ts.SamplerAttribs.IsStaticSampler() )
                    {
                        VERIFY_EXPR( SamplerSlot < m_pResourceCache->GetSamplerCount() && SamplerSlot < DstCache.GetSamplerCount() );
                        DstSamplers[SamplerSlot] = CachedSamplers[SamplerSlot];
                        DstD3D11Samplers[SamplerSlot] = d3d11Samplers[SamplerSlot];
                    }
                    else
                    {
                        VERIFY(DstSamplers[SamplerSlot].pSampler != nullptr && DstD3D11Samplers[SamplerSlot] != nullptr, "Static samplers must be initialized when shader resource cache is created");
                    }
                }
            }
        },

        [&](const TexUAVBindInfo& uav)
        {
            for(auto UAVSlot = uav.Attribs.BindPoint; UAVSlot < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++UAVSlot)
            {
                VERIFY_EXPR(UAVSlot < m_pResourceCache->GetUAVCount() && UAVSlot < DstCache.GetUAVCount());
                DstUAVResources[UAVSlot] = CachedUAVResources[UAVSlot];
                DstD3D11UAVs[UAVSlot] = d3d11UAVs[UAVSlot];
            }
        },

        [&](const BuffSRVBindInfo& srv)
        {
            for(auto SRVSlot = srv.Attribs.BindPoint; SRVSlot < srv.Attribs.BindPoint + srv.Attribs.BindCount; ++SRVSlot)
            {
                VERIFY_EXPR(SRVSlot < m_pResourceCache->GetSRVCount() && SRVSlot < DstCache.GetSRVCount());
                DstSRVResources[SRVSlot] = CachedSRVResources[SRVSlot];
                DstD3D11SRVs[SRVSlot] = d3d11SRVs[SRVSlot];
            }
        },

        [&](const BuffUAVBindInfo& uav)
        {
            for(auto UAVSlot = uav.Attribs.BindPoint; UAVSlot < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++UAVSlot)
            {
                VERIFY_EXPR(UAVSlot < m_pResourceCache->GetUAVCount() && UAVSlot < DstCache.GetUAVCount());
                DstUAVResources[UAVSlot] = CachedUAVResources[UAVSlot];
                DstD3D11UAVs[UAVSlot] = d3d11UAVs[UAVSlot];
            }
        }
    );
}

void ShaderResourceLayoutD3D11::InitVariablesHashMap()
{
#if USE_VARIABLE_HASH_MAP
    // After all resources are loaded, we can populate shader variable hash map.
    // The map contains raw pointers, but none of the arrays will ever change.
    HandleResources(
        [&](ConstBuffBindInfo&cb)
        {
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(cb.Attribs.Name, true), &cb ) );
        },

        [&](TexAndSamplerBindInfo& ts)
        {
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(ts.Attribs.Name, true), &ts ) );
        },

        [&](TexUAVBindInfo& uav)
        {
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(uav.Attribs.Name, true), &uav ) );
        },

        [&](BuffSRVBindInfo& srv)
        {
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(srv.Attribs.Name, true), &srv ) );
        },

        [&](BuffUAVBindInfo& uav)
        {
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(uav.Attribs.Name, true), &uav ) );
        }
    );
#endif
}

#define LOG_RESOURCE_BINDING_ERROR(ResType, pResource, Attribs, ArrayInd, ShaderName, ...)\
do{                                                                                       \
    const auto* ResName = pResource->GetDesc().Name;                                      \
    if(Attribs.BindCount>1)                                                               \
        LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " \"", ResName, "\" to variable \"", Attribs.Name,\
                           "[", ArrayInd, "]\" in shader \"", ShaderName, "\". ", __VA_ARGS__ );         \
    else                                                                                                 \
        LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " \"", ResName, "\" to variable \"", Attribs.Name,\
                           "\" in shader \"", ShaderName, "\". ", __VA_ARGS__ );                         \
}while(false)

void ShaderResourceLayoutD3D11::ConstBuffBindInfo::BindResource(IDeviceObject*                   pBuffer,
                                                                Uint32                           ArrayIndex,
                                                                const ShaderResourceLayoutD3D11* dbgResLayout)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    RefCntAutoPtr<BufferD3D11Impl> pBuffD3D11Impl;
    if( pBuffer )
    {
        // We cannot use ValidatedCast<> here as the resource retrieved from the
        // resource mapping can be of wrong type
        IBufferD3D11 *pBuffD3D11 = nullptr;
        pBuffer->QueryInterface(IID_BufferD3D11, reinterpret_cast<IObject**>(&pBuffD3D11));
        if( pBuffD3D11 )
        {
            pBuffD3D11Impl.Attach(ValidatedCast<BufferD3D11Impl>(pBuffD3D11));
            if( !(pBuffD3D11Impl->GetDesc().BindFlags & BIND_UNIFORM_BUFFER) )
            {
                pBuffD3D11Impl.Release();
                LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNIFORM_BUFFER flag.");
            }
        }
        else
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs, ArrayIndex, m_ParentResLayout.GetShaderName(), "Incorrect resource type: buffer is expected.");
        }
    }

#ifdef VERIFY_SHADER_BINDINGS
    if( Attribs.VariableType != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto &CachedCB = pResourceCache->GetCB(Attribs.BindPoint + ArrayIndex);
        if( CachedCB.pBuff != nullptr && CachedCB.pBuff != pBuffD3D11Impl)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.VariableType);
            LOG_ERROR_MESSAGE( "Non-null constant buffer is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic." );
        }
    }
#endif
    pResourceCache->SetCB(Attribs.BindPoint + ArrayIndex, std::move(pBuffD3D11Impl) );
}



bool ShaderResourceLayoutD3D11::ConstBuffBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsCBBound(Attribs.BindPoint + ArrayIndex);
}



#ifdef VERIFY_SHADER_BINDINGS
template<typename TResourceViewType,        ///< Type of the view (ITextureViewD3D11 or IBufferViewD3D11)
         typename TViewTypeEnum>            ///< Type of the expected view enum
bool dbgVerifyViewType( const char *ViewTypeName,
                        TResourceViewType pViewD3D11,
                        const D3DShaderResourceAttribs& Attribs, 
                        Uint32 ArrayIndex,
                        TViewTypeEnum dbgExpectedViewType,
                        const String &ShaderName )
{
    const auto& ViewDesc = pViewD3D11->GetDesc();
    auto ViewType = ViewDesc.ViewType;
    if (ViewType == dbgExpectedViewType)
    {
        return true;
    }
    else
    {
        const auto *ExpectedViewTypeName = GetViewTypeLiteralName( dbgExpectedViewType );
        const auto *ActualViewTypeName = GetViewTypeLiteralName( ViewType );
        LOG_RESOURCE_BINDING_ERROR(ViewTypeName, pViewD3D11, Attribs, ArrayIndex, ShaderName, 
                                   "Incorrect view type: ", ExpectedViewTypeName, " is expected, ", ActualViewTypeName, " provided." );
        return false;
    }
}
#endif

void ShaderResourceLayoutD3D11::TexAndSamplerBindInfo::BindResource( IDeviceObject*                   pView,
                                                                     Uint32                           ArrayIndex,
                                                                     const ShaderResourceLayoutD3D11* dbgResLayout )
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TextureViewD3D11Impl> pViewD3D11(pView, IID_TextureViewD3D11);
#ifdef VERIFY_SHADER_BINDINGS
    if(pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, "", "Incorect resource type: texture view is expected.");
    if(pViewD3D11 && !dbgVerifyViewType("texture view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, TEXTURE_VIEW_SHADER_RESOURCE, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if( Attribs.VariableType != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto &CachedSRV = pResourceCache->GetSRV(Attribs.BindPoint + ArrayIndex);
        if( CachedSRV.pView != nullptr && CachedSRV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.VariableType);
            LOG_ERROR_MESSAGE( "Non-null texture SRV is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic." );
        }
    }
#endif
    
    if( SamplerAttribs.IsValidBindPoint() )
    {
        VERIFY_EXPR(SamplerAttribs.BindCount == Attribs.BindCount || SamplerAttribs.BindCount == 1);
        auto SamplerBindPoint = SamplerAttribs.BindPoint + (SamplerAttribs.BindCount != 1 ? ArrayIndex : 0);
        if( !SamplerAttribs.IsStaticSampler() )
        {
            SamplerD3D11Impl *pSamplerD3D11Impl = nullptr;
            if( pViewD3D11 )
            {
                pSamplerD3D11Impl = ValidatedCast<SamplerD3D11Impl>(pViewD3D11->GetSampler());
#ifdef VERIFY_SHADER_BINDINGS
                if(pSamplerD3D11Impl==nullptr)
                {
                    if(SamplerAttribs.BindCount > 1)
                        LOG_ERROR_MESSAGE( "Failed to bind sampler to variable \"", SamplerAttribs.Name, "[", ArrayIndex,"]\". Sampler is not set in the texture view \"", pViewD3D11->GetDesc().Name, "\"" );
                    else
                        LOG_ERROR_MESSAGE( "Failed to bind sampler to variable \"", SamplerAttribs.Name, "\". Sampler is not set in the texture view \"", pViewD3D11->GetDesc().Name, "\"" );
                }
#endif
            }
            
            pResourceCache->SetSampler(SamplerBindPoint, pSamplerD3D11Impl);
        }
        else
        {
            VERIFY_EXPR(SamplerAttribs.BindCount == Attribs.BindCount || SamplerAttribs.BindCount == 1);
            VERIFY( pResourceCache->IsSamplerBound(SamplerBindPoint), "Static samplers must be bound once when shader cache is created" );
        }
    }          

    pResourceCache->SetTexSRV(Attribs.BindPoint + ArrayIndex, std::move(pViewD3D11));
}


void ShaderResourceLayoutD3D11::BuffSRVBindInfo::BindResource( IDeviceObject*                   pView,
                                                               Uint32                           ArrayIndex,
                                                               const ShaderResourceLayoutD3D11* dbgResLayout )
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY(ArrayIndex < Attribs.BindCount, "Array index is out of range");

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferViewD3D11Impl> pViewD3D11(pView, IID_BufferViewD3D11);
#ifdef VERIFY_SHADER_BINDINGS
    if(pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, "", "Incorect resource type: buffer view is expected.");
    if(pViewD3D11 && !dbgVerifyViewType("buffer view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, BUFFER_VIEW_SHADER_RESOURCE, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if( Attribs.VariableType != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto &CachedSRV = pResourceCache->GetSRV(Attribs.BindPoint + ArrayIndex);
        if( CachedSRV.pView != nullptr && CachedSRV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.VariableType);
            LOG_ERROR_MESSAGE( "Non-null buffer SRV is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic." );
        }
    }
#endif

    pResourceCache->SetBufSRV(Attribs.BindPoint + ArrayIndex, std::move(pViewD3D11));
}


void ShaderResourceLayoutD3D11::TexUAVBindInfo::BindResource( IDeviceObject*                   pView,
                                                              Uint32                           ArrayIndex,
                                                              const ShaderResourceLayoutD3D11* dbgResLayout )
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY(ArrayIndex < Attribs.BindCount, "Array index is out of range");

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TextureViewD3D11Impl> pViewD3D11(pView, IID_TextureViewD3D11);
#ifdef VERIFY_SHADER_BINDINGS
    if(pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, "", "Incorect resource type: texture view is expected.");
    if(pViewD3D11 && !dbgVerifyViewType("texture view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, TEXTURE_VIEW_UNORDERED_ACCESS, m_ParentResLayout.GetShaderName()))
        pViewD3D11.Release();

    if( Attribs.VariableType != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto &CachedUAV = pResourceCache->GetUAV(Attribs.BindPoint + ArrayIndex);
        if( CachedUAV.pView != nullptr && CachedUAV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.VariableType);
            LOG_ERROR_MESSAGE( "Non-null texture UAV is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic." );
        }
    }
#endif

    pResourceCache->SetTexUAV(Attribs.BindPoint + ArrayIndex, std::move(pViewD3D11));
}


void ShaderResourceLayoutD3D11::BuffUAVBindInfo::BindResource( IDeviceObject*                   pView,
                                                               Uint32                           ArrayIndex,
                                                               const ShaderResourceLayoutD3D11* dbgResLayout )
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY(ArrayIndex < Attribs.BindCount, "Array index is out of range");

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferViewD3D11Impl> pViewD3D11(pView, IID_BufferViewD3D11);
#ifdef VERIFY_SHADER_BINDINGS
    if(pView && !pViewD3D11)
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs, ArrayIndex, "", "Incorect resource type: buffer view is expected.");
    if(pViewD3D11 && !dbgVerifyViewType("buffer view", pViewD3D11.RawPtr(), Attribs, ArrayIndex, BUFFER_VIEW_UNORDERED_ACCESS, m_ParentResLayout.GetShaderName()) )
        pViewD3D11.Release();

    if( Attribs.VariableType != SHADER_VARIABLE_TYPE_DYNAMIC)
    {
        auto &CachedUAV = pResourceCache->GetUAV(Attribs.BindPoint + ArrayIndex);
        if( CachedUAV.pView != nullptr && CachedUAV.pView != pViewD3D11)
        {
            auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.VariableType);
            LOG_ERROR_MESSAGE( "Non-null buffer UAV is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic." );
        }
    }
#endif

    pResourceCache->SetBufUAV(Attribs.BindPoint + ArrayIndex, std::move(pViewD3D11));
}


bool ShaderResourceLayoutD3D11::TexAndSamplerBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsSRVBound(Attribs.BindPoint + ArrayIndex, true);
}


bool ShaderResourceLayoutD3D11::BuffSRVBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsSRVBound(Attribs.BindPoint + ArrayIndex, false);
}

bool ShaderResourceLayoutD3D11::TexUAVBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsUAVBound(Attribs.BindPoint + ArrayIndex, true);
}

bool ShaderResourceLayoutD3D11::BuffUAVBindInfo::IsBound(Uint32 ArrayIndex)
{
    auto &pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    return pResourceCache->IsUAVBound(Attribs.BindPoint + ArrayIndex, false);
}



// Helper template class that facilitates binding CBs, SRVs, and UAVs
class BindResourceHelper
{
public:
    BindResourceHelper(IResourceMapping* pRM, Uint32 Fl, const ShaderResourceLayoutD3D11* pSRL) :
        pResourceMapping(pRM),
        Flags(Fl),
        pShaderResLayout(pSRL)
    {
        VERIFY(pResourceMapping != nullptr, "Resource mapping is null");
        VERIFY(pSRL != nullptr, "Shader resource layout is null");
    }

    template<typename ResourceType>
    void Bind( ResourceType &Res)
    {
        for(Uint16 elem=0; elem < Res.Attribs.BindCount; ++elem)
        {
            if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
                Res.BindResource(nullptr, elem, pShaderResLayout);

            if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && Res.IsBound(elem) )
                return;

            const auto* VarName = Res.Attribs.Name;
            RefCntAutoPtr<IDeviceObject> pRes;
            VERIFY_EXPR(pResourceMapping != nullptr);
            pResourceMapping->GetResource( VarName, &pRes, elem );
            if( pRes )
            {
                //  Call non-virtual function
                Res.BindResource(pRes, elem, pShaderResLayout);
            }
            else
            {
                if( (Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED) && !Res.IsBound(elem) )
                    LOG_ERROR_MESSAGE( "Cannot bind resource to shader variable \"", VarName, "\": resource view not found in the resource mapping" );
            }
        }
    }

private:
    IResourceMapping* const pResourceMapping;
    const Uint32 Flags;
    const ShaderResourceLayoutD3D11 *pShaderResLayout;
};

void ShaderResourceLayoutD3D11::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheD3D11& dbgResourceCache )
{
    VERIFY(&dbgResourceCache == m_pResourceCache, "Resource cache does not match the cache provided at initialization");

    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind resources in shader \"", GetShaderName(), "\": resource mapping is null" );
        return;
    }

    BindResourceHelper BindResHelper(pResourceMapping, Flags, this);

    HandleResources(
        [&](ConstBuffBindInfo&cb)
        {
            BindResHelper.Bind(cb);
        },

        [&](TexAndSamplerBindInfo& ts)
        {
            BindResHelper.Bind(ts);
        },

        [&](TexUAVBindInfo& uav)
        {
            BindResHelper.Bind(uav);
        },

        [&](BuffSRVBindInfo& srv)
        {
            BindResHelper.Bind(srv);
        },

        [&](BuffUAVBindInfo& uav)
        {
            BindResHelper.Bind(uav);
        }
    );
}

IShaderVariable* ShaderResourceLayoutD3D11::GetShaderVariable(const Char* Name)
{
    IShaderVariable *pVar = nullptr;
#if USE_VARIABLE_HASH_MAP
    // Name will be implicitly converted to HashMapStringKey without making a copy
    auto it = m_VariableHash.find( Name );
    if( it != m_VariableHash.end() )
        pVar = it->second;
#else
    for (Uint32 cb = 0; cb < m_NumCBs; ++cb)
        if (strcmp(GetCB(cb).Attribs.Name, Name) == 0)
            return &GetCB(cb);
    for (Uint32 t = 0; t < m_NumTexSRVs; ++t)
        if (strcmp(GetTexSRV(t).Attribs.Name, Name) == 0 )
            return &GetTexSRV(t);
    for (Uint32 u = 0; u < m_NumTexUAVs; ++u)
        if (strcmp(GetTexUAV(u).Attribs.Name, Name) == 0 )
            return &GetTexUAV(u);
    for (Uint32 s = 0; s < m_NumBufSRVs; ++s)
        if (strcmp(GetBufSRV(s).Attribs.Name, Name) == 0 )
            return &GetBufSRV(s);
    for (Uint32 u = 0; u < m_NumBufUAVs; ++u)
        if (strcmp(GetBufUAV(u).Attribs.Name, Name) == 0 )
            return &GetBufUAV(u);
#endif
    if(pVar == nullptr)
    {
        LOG_ERROR_MESSAGE( "Shader variable \"", Name, "\" is not found in shader \"", GetShaderName(), "\". Attempts to set the variable will be silently ignored." );
    }
    return pVar;
}

const Char* ShaderResourceLayoutD3D11::GetShaderName()const
{
    return m_pResources->GetShaderName();
}

#ifdef VERIFY_SHADER_BINDINGS
void ShaderResourceLayoutD3D11::dbgVerifyBindings()const
{

#define LOG_MISSING_BINDING(VarType, Attrs, BindPt)\
do{                                                \
    if(Attrs.BindCount == 1)                       \
        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", Attrs.Name, "\" in shader \"", GetShaderName(), "\"" );   \
    else                                                                                                                                  \
        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", Attrs.Name, "[", BindPt-Attrs.BindPoint, "]\" in shader \"", GetShaderName(), "\"" );\
}while(false)

    m_pResourceCache->dbgVerifyCacheConsistency();
    
    // Use const_cast to avoid duplication of the HandleResources() function
    // The function actually changes nothing
    const_cast<ShaderResourceLayoutD3D11*>(this)->HandleResources(
        [&](const ConstBuffBindInfo&cb)
        {
            for(auto BindPoint = cb.Attribs.BindPoint; BindPoint < cb.Attribs.BindPoint + cb.Attribs.BindCount; ++BindPoint)
            {
                if( !m_pResourceCache->IsCBBound(BindPoint) )
                    LOG_MISSING_BINDING("constant buffer", cb.Attribs, BindPoint);
            }
        },

        [&](const TexAndSamplerBindInfo& ts)
        {
            for(auto BindPoint = ts.Attribs.BindPoint; BindPoint < ts.Attribs.BindPoint + ts.Attribs.BindCount; ++BindPoint)
            {
                if( !m_pResourceCache->IsSRVBound(BindPoint, true) )
                    LOG_MISSING_BINDING("texture", ts.Attribs, BindPoint);

                if( ts.SamplerAttribs.IsValidBindPoint() )
                {
                    VERIFY_EXPR(ts.SamplerAttribs.BindCount == ts.Attribs.BindCount || ts.SamplerAttribs.BindCount == 1);
                    auto SamBindPoint = ts.SamplerAttribs.BindPoint + ((ts.SamplerAttribs.BindCount == 1) ? 0 : (BindPoint - ts.Attribs.BindPoint) );
                    if(!m_pResourceCache->IsSamplerBound(SamBindPoint) )
                        LOG_MISSING_BINDING("sampler", ts.SamplerAttribs, SamBindPoint);

                    // Verify that if single sampler is used for all texture array elements, all samplers set in the resource views are consistent
                    if (ts.Attribs.BindCount > 1 && ts.SamplerAttribs.BindCount == 1)
                    {
                        ShaderResourceCacheD3D11::CachedSampler *pCachedSamplers = nullptr;
                        ID3D11SamplerState **ppCachedD3D11Samplers = nullptr;
                        m_pResourceCache->GetSamplerArrays(pCachedSamplers, ppCachedD3D11Samplers);
                        VERIFY_EXPR(ts.SamplerAttribs.BindPoint < m_pResourceCache->GetSamplerCount());
                        const auto &Sampler = pCachedSamplers[ts.SamplerAttribs.BindPoint];

                        ShaderResourceCacheD3D11::CachedResource *pCachedResources = nullptr;
                        ID3D11ShaderResourceView **ppCachedD3D11Resources = nullptr;
                        m_pResourceCache->GetSRVArrays(pCachedResources, ppCachedD3D11Resources);
                        VERIFY_EXPR(BindPoint < m_pResourceCache->GetSRVCount());
                        auto &CachedResource = pCachedResources[BindPoint];
                        if(CachedResource.pView)
                        {
                            auto *pTexView = CachedResource.pView.RawPtr<ITextureView>();
                            auto *pSampler = pTexView->GetSampler();
                            if(pSampler != nullptr && pSampler != Sampler.pSampler.RawPtr())
                                LOG_ERROR_MESSAGE( "All elements of texture array \"", ts.Attribs.Name, "\" in shader \"", GetShaderName(), "\" share the same sampler. However, the sampler set in view for element ", BindPoint - ts.Attribs.BindPoint, " does not match bound sampler. This may cause incorrect behavior on GL platform."  );
                        }
                    }
                }
            }
        },

        [&](const TexUAVBindInfo& uav)
        {
            for(auto BindPoint = uav.Attribs.BindPoint; BindPoint < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++BindPoint)
            {
                if( !m_pResourceCache->IsUAVBound(BindPoint, true) )
                    LOG_MISSING_BINDING("texture UAV", uav.Attribs, BindPoint);
            }
        },

        [&](const BuffSRVBindInfo& buf)
        {
            for(auto BindPoint = buf.Attribs.BindPoint; BindPoint < buf.Attribs.BindPoint + buf.Attribs.BindCount; ++BindPoint)
            {
                if( !m_pResourceCache->IsSRVBound(BindPoint, false) )
                    LOG_MISSING_BINDING("buffer", buf.Attribs, BindPoint);
            }
        },

        [&](const BuffUAVBindInfo& uav)
        {
            for(auto BindPoint = uav.Attribs.BindPoint; BindPoint < uav.Attribs.BindPoint + uav.Attribs.BindCount; ++BindPoint)
            {
                if( !m_pResourceCache->IsUAVBound(BindPoint, false) )
                    LOG_MISSING_BINDING("buffer UAV", uav.Attribs, BindPoint);
            }
        }
    );
#undef LOG_MISSING_BINDING
}

#endif
}
