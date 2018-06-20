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

#include "ShaderResourceCacheD3D11.h"
#include "ShaderResourceLayoutD3D11.h"
#include "TextureBaseD3D11.h"
#include "BufferD3D11Impl.h"
#include "SamplerD3D11Impl.h"
#include "MemoryAllocator.h"

namespace Diligent
{
    void ShaderResourceCacheD3D11::Initialize(Int32 CBCount, Int32 SRVCount, Int32 SamplerCount, Int32 UAVCount, IMemoryAllocator& MemAllocator)
    {
        // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
        if (IsInitialized())
        {
            LOG_ERROR_MESSAGE("Resource cache is already intialized");
            return;
        }


        VERIFY(CBCount <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, "Constant buffer count ", CBCount, " exceeds D3D11 limit ", D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT );
        VERIFY(SRVCount <= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, "SRV count ", SRVCount, " exceeds D3D11 limit ", D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT );
        VERIFY(SamplerCount <= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, "Sampler count ", SamplerCount, " exceeds D3D11 limit ", D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT );
        VERIFY(UAVCount <= D3D11_PS_CS_UAV_REGISTER_COUNT, "UAV count ", UAVCount, " exceeds D3D11 limit ", D3D11_PS_CS_UAV_REGISTER_COUNT );

        m_ResourceCounts = PackResourceCounts(CBCount, SRVCount, SamplerCount, UAVCount);
        VERIFY_EXPR(GetCBCount()      == static_cast<Uint32>(CBCount));
        VERIFY_EXPR(GetSRVCount()     == static_cast<Uint32>(SRVCount));
        VERIFY_EXPR(GetSamplerCount() == static_cast<Uint32>(SamplerCount));
        VERIFY_EXPR(GetUAVCount()     == static_cast<Uint32>(UAVCount));

        CachedCB* CBs = nullptr;
        ID3D11Buffer** d3d11CBs = nullptr; 
        CachedResource* SRVResources = nullptr;
        ID3D11ShaderResourceView** d3d11SRVs = nullptr;
        CachedSampler* Samplers = nullptr;
        ID3D11SamplerState** d3d11Samplers = nullptr;
        CachedResource* UAVResources = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
        GetResourceArrays(CBs, d3d11CBs, SRVResources, d3d11SRVs, Samplers, d3d11Samplers, UAVResources, d3d11UAVs);

        VERIFY_EXPR(m_pResourceData == nullptr);
        size_t BufferSize =  reinterpret_cast<Uint8*>(d3d11UAVs + UAVCount) - m_pResourceData;

        VERIFY_EXPR( BufferSize ==
                    (sizeof(CBs[0])          + sizeof(d3d11CBs[0]))      * CBCount + 
                    (sizeof(SRVResources[0]) + sizeof(d3d11SRVs[0]))     * SRVCount + 
                    (sizeof(Samplers[0])     + sizeof(d3d11Samplers[0])) * SamplerCount + 
                    (sizeof(UAVResources[0]) + sizeof(d3d11UAVs[0]))     * UAVCount );

#ifdef _DEBUG
        m_pdbgMemoryAllocator = &MemAllocator;
#endif
        if( BufferSize > 0 )
        {
            m_pResourceData = reinterpret_cast<Uint8*>(MemAllocator.Allocate(BufferSize, "Shader resource cache data buffer", __FILE__, __LINE__ ));
            memset(m_pResourceData, 0, BufferSize);
        }


        GetResourceArrays(CBs, d3d11CBs, SRVResources, d3d11SRVs, Samplers, d3d11Samplers, UAVResources, d3d11UAVs);
#ifdef _DEBUG
        {
            CachedCB* dbgCBs = nullptr;
            ID3D11Buffer** dbgd3d11CBs = nullptr;
            GetCBArrays(dbgCBs, dbgd3d11CBs);
            VERIFY_EXPR(dbgCBs == CBs && dbgd3d11CBs == d3d11CBs);
        }
        {
            CachedResource* dbgSRVResources = nullptr;
            ID3D11ShaderResourceView** dbgd3d11SRVs = nullptr;
            GetSRVArrays(dbgSRVResources, dbgd3d11SRVs);
            VERIFY_EXPR(dbgSRVResources == SRVResources && dbgd3d11SRVs == d3d11SRVs);
        }
        {
            CachedSampler* dbgSamplers = nullptr;
            ID3D11SamplerState** dbgd3d11Samplers = nullptr;
            GetSamplerArrays(dbgSamplers, dbgd3d11Samplers);
            VERIFY_EXPR(dbgSamplers == Samplers && dbgd3d11Samplers == d3d11Samplers);
        }

        {
            CachedResource* dbgUAVResources = nullptr;
            ID3D11UnorderedAccessView** dbgd3d11UAVs = nullptr;
            GetUAVArrays(dbgUAVResources, dbgd3d11UAVs);
            VERIFY_EXPR(dbgUAVResources == UAVResources && dbgd3d11UAVs == d3d11UAVs);
        }
#endif

        // Explicitly construct all objects
        for (Int32 cb = 0; cb < CBCount; ++cb)
            new(CBs+cb)CachedCB;

        for (Int32 srv = 0; srv < SRVCount; ++srv)
            new(SRVResources+srv)CachedResource;

        for (Int32 sam = 0; sam < SamplerCount; ++sam)
            new(Samplers+sam)CachedSampler;

        for (Int32 uav = 0; uav < UAVCount; ++uav)
            new(UAVResources+uav)CachedResource;
    }

    void ShaderResourceCacheD3D11::Destroy(IMemoryAllocator& MemAllocator)
    {
        VERIFY( IsInitialized(), "Resource cache is not initialized");
        VERIFY( m_pdbgMemoryAllocator == &MemAllocator, "The allocator does not match the one used to create resources");

        if( IsInitialized() ) 
        {
            CachedCB* CBs = nullptr;
            ID3D11Buffer** d3d11CBs = nullptr; 
            CachedResource* SRVResources = nullptr;
            ID3D11ShaderResourceView** d3d11SRVs = nullptr;
            CachedSampler* Samplers = nullptr;
            ID3D11SamplerState** d3d11Samplers = nullptr;
            CachedResource* UAVResources = nullptr;
            ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
            GetResourceArrays(CBs, d3d11CBs, SRVResources, d3d11SRVs, Samplers, d3d11Samplers, UAVResources, d3d11UAVs);

            // Explicitly destory all objects
            auto CBCount = GetCBCount();
            for (size_t cb = 0; cb < CBCount; ++cb)
                CBs[cb].~CachedCB();
            auto SRVCount = GetSRVCount();
            for (size_t srv = 0; srv < SRVCount; ++srv)
                SRVResources[srv].~CachedResource();
            auto SamplerCount = GetSamplerCount();
            for (size_t sam = 0; sam < SamplerCount; ++sam)
                Samplers[sam].~CachedSampler();
            auto UAVCount = GetUAVCount();
            for (size_t uav = 0; uav < UAVCount; ++uav)
                UAVResources[uav].~CachedResource();
            m_ResourceCounts = InvalidResourceCounts;

            if(m_pResourceData != nullptr)
                MemAllocator.Free(m_pResourceData);
        }
    }

    ShaderResourceCacheD3D11::~ShaderResourceCacheD3D11()
    {
        VERIFY( !IsInitialized(), "Shader resource cache memory must be released with ShaderResourceCacheD3D11::Destroy()" );
    }

    void dbgVerifyResource(ShaderResourceCacheD3D11::CachedResource& Res, ID3D11View* pd3d11View, const char* ViewType)
    {
        if (pd3d11View != nullptr)
        {
            VERIFY(Res.pView != nullptr, "Resource view is not initialized");
            VERIFY(Res.pBuffer==nullptr && Res.pTexture!=nullptr || Res.pBuffer!=nullptr && Res.pTexture==nullptr, "Texture and buffer resources are mutually exclusive");
            VERIFY(Res.pd3d11Resource!=nullptr, "D3D11 resource is missing");

            CComPtr<ID3D11Resource> pd3d11ActualResource;
            pd3d11View->GetResource(&pd3d11ActualResource);
            VERIFY(pd3d11ActualResource == Res.pd3d11Resource, "Inconsistent D3D11 resource");
            if (Res.pBuffer)
            {
                VERIFY(pd3d11ActualResource == Res.pBuffer->GetD3D11Buffer(), "Inconsistent buffer ", ViewType);
                if (Res.pView)
                {
                    RefCntAutoPtr<IBufferViewD3D11> pBufView(Res.pView, IID_BufferViewD3D11);
                    VERIFY(pBufView != nullptr, "Provided resource view is not D3D11 buffer view");
                    if(pBufView)
                        VERIFY(pBufView->GetBuffer() == Res.pBuffer, "Provided resource view is not a view of the buffer");
                }
            }
            else if(Res.pTexture)
            {
                VERIFY(pd3d11ActualResource == Res.pTexture->GetD3D11Texture(), "Inconsistent texture ", ViewType);
                if (Res.pView)
                {
                    RefCntAutoPtr<ITextureViewD3D11> pTexView(Res.pView, IID_TextureViewD3D11);
                    VERIFY(pTexView != nullptr, "Provided resource view is not D3D11 texture view");
                    if(pTexView)
                        VERIFY(pTexView->GetTexture() == Res.pTexture, "Provided resource view is not a view of the texture");
                }
            }
        }
        else
        {
            VERIFY(Res.pView==nullptr, "Resource view is unexpected");
            VERIFY(Res.pBuffer==nullptr && Res.pTexture==nullptr, "Niether texture nor buffer resource is expected");
            VERIFY(Res.pd3d11Resource==nullptr, "Unexepected D3D11 resource");
        }        
    }

    void ShaderResourceCacheD3D11::dbgVerifyCacheConsistency()
    {
        VERIFY(IsInitialized(), "Cache is not initialized");

        CachedCB* CBs = nullptr;
        ID3D11Buffer** d3d11CBs = nullptr; 
        CachedResource* SRVResources = nullptr;
        ID3D11ShaderResourceView** d3d11SRVs = nullptr;
        CachedSampler* Samplers = nullptr;
        ID3D11SamplerState** d3d11Samplers = nullptr;
        CachedResource* UAVResources = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
        GetResourceArrays(CBs, d3d11CBs, SRVResources, d3d11SRVs, Samplers, d3d11Samplers, UAVResources, d3d11UAVs);

        auto CBCount = GetCBCount();
        for (size_t cb = 0; cb < CBCount; ++cb)
        {
            auto &pBuff = CBs[cb].pBuff;
            auto *pd3d11Buff = d3d11CBs[cb];
            VERIFY(pBuff==nullptr && pd3d11Buff==nullptr || pBuff!=nullptr && pd3d11Buff!=nullptr, "CB resource and d3d11 buffer must be set/unset atomically");
            if(pBuff != nullptr && pd3d11Buff != nullptr )
            {
                VERIFY(pd3d11Buff == pBuff->GetD3D11Buffer(), "Inconsistent D3D11 buffer");
            }
        }

        auto  SRVCount = GetSRVCount();
        for (size_t srv = 0; srv < SRVCount; ++srv)
        {
            auto &Res = SRVResources[srv];
            auto *pd3d11SRV = d3d11SRVs[srv];
            dbgVerifyResource(Res, pd3d11SRV, "SRV");
        }

        auto UAVCount = GetUAVCount();
        for (size_t uav = 0; uav < UAVCount; ++uav)
        {
            auto &Res = UAVResources[uav];
            auto *pd3d11UAV = d3d11UAVs[uav];
            dbgVerifyResource(Res, pd3d11UAV, "UAV");
        }

        auto SamplerCount = GetSamplerCount();
        for (size_t sam = 0; sam < SamplerCount; ++sam)
        {
            auto &pSampler = Samplers[sam].pSampler;
            auto *pd3d11Sampler = d3d11Samplers[sam];
            VERIFY(pSampler==nullptr && pd3d11Sampler==nullptr || pSampler!=nullptr && pd3d11Sampler!=nullptr, "CB resource and d3d11 buffer must be set/unset atomically");
            if(pSampler!=nullptr && pd3d11Sampler!=nullptr)
            {
                VERIFY(pd3d11Sampler==pSampler->GetD3D11SamplerState(), "Inconsistent D3D11 sampler");
            }
        }
    }
}
