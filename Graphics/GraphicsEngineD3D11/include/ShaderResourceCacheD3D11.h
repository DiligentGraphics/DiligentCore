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

#pragma once

/// \file
/// Declaration of Diligent::ShaderResourceCacheD3D11 class

#include "TextureBaseD3D11.h"
#include "BufferD3D11Impl.h"
#include "SamplerD3D11Impl.h"

namespace Diligent
{

/// The class implements a cache that holds resources bound to a specific shader stage
// All resources are stored in the continuous memory using the following layout:
//
//   |         CachedCB         |      ID3D11Buffer*     ||       CachedResource     | ID3D11ShaderResourceView* ||         CachedSampler        |      ID3D11SamplerState*    ||      CachedResource     | ID3D11UnorderedAccessView*||
//   |---------------------------------------------------||--------------------------|---------------------------||------------------------------|-----------------------------||-------------------------|---------------------------||
//   |  0 | 1 | ... | CBCount-1 | 0 | 1 | ...| CBCount-1 || 0 | 1 | ... | SRVCount-1 | 0 | 1 |  ... | SRVCount-1 || 0 | 1 | ... | SamplerCount-1 | 0 | 1 | ...| SamplerCount-1 ||0 | 1 | ... | UAVCount-1 | 0 | 1 | ...  | UAVCount-1 ||
//    --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//
// http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
class ShaderResourceCacheD3D11
{
public:
    ShaderResourceCacheD3D11()
    {}

    ~ShaderResourceCacheD3D11();


    ShaderResourceCacheD3D11(const ShaderResourceCacheD3D11&) = delete;
    ShaderResourceCacheD3D11& operator = (const ShaderResourceCacheD3D11&) = delete;
    ShaderResourceCacheD3D11(ShaderResourceCacheD3D11&&) = delete;
    ShaderResourceCacheD3D11& operator = (ShaderResourceCacheD3D11&&) = delete;

    /// Describes resources associated with the cached constant buffer
    struct CachedCB
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferD3D11Impl> pBuff;
        __forceinline void Set(RefCntAutoPtr<BufferD3D11Impl> &&_pBuff)
        {
            pBuff = std::move(_pBuff);
        }
    };

    /// Describes resources associated with the cached sampler
    struct CachedSampler
    {
        /// Strong reference to the sampler
        RefCntAutoPtr<class SamplerD3D11Impl> pSampler;
        __forceinline void Set(SamplerD3D11Impl *pSam)
        {
            pSampler = pSam;
        }
    };

    /// Describes resources associated with the cached SRV or UAV
    struct CachedResource
    {
        /// Wee keep strong reference to the view instead of the reference
        /// to the texture or buffer because this is more efficient from
        /// performance point of view: this avoids one pair of 
        /// AddStrongRef()/ReleaseStrongRef(). The view holds strong reference 
        /// to the texture or the buffer, so it makes no difference.
        RefCntAutoPtr<IDeviceObject> pView;
        TextureBaseD3D11* pTexture;
        BufferD3D11Impl* pBuffer;
        // There is no need to keep strong reference to D3D11 resource as
        // it is already kept by either pTexture or pBuffer
        ID3D11Resource *pd3d11Resource;
        CachedResource() : pTexture(nullptr), pBuffer(nullptr), pd3d11Resource(nullptr)
        {}

        __forceinline void Set(RefCntAutoPtr<TextureViewD3D11Impl>&& pTexView)
        {
            pBuffer = nullptr;
            // Avoid unnecessary virtual function calls
            pTexture = pTexView ? ValidatedCast<TextureBaseD3D11>(pTexView->TextureViewD3D11Impl::GetTexture()) : nullptr;
            pView.Attach(pTexView.Detach());
            pd3d11Resource = pTexture ? pTexture->TextureBaseD3D11::GetD3D11Texture() : nullptr;
        }

        __forceinline void Set(RefCntAutoPtr<BufferViewD3D11Impl>&& pBufView)
        {
            pTexture = nullptr;
            // Avoid unnecessary virtual function calls
            pBuffer = pBufView ? ValidatedCast<BufferD3D11Impl>(pBufView->BufferViewD3D11Impl::GetBuffer()) : nullptr;
            pView.Attach(pBufView.Detach());
            pd3d11Resource = pBuffer ? pBuffer->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        }
    };

    void Initialize(Int32 CBCount, Int32 SRVCount, Int32 SamplerCount, Int32 UAVCount, class IMemoryAllocator &MemAllocator);
    void Destroy(class IMemoryAllocator &MemAllocator);


    __forceinline void SetCB(Uint32 Slot, RefCntAutoPtr<BufferD3D11Impl> &&pBuffD3D11Impl)
    {
        auto *pd3d11Buff = pBuffD3D11Impl ? pBuffD3D11Impl->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        SetD3D11ResourceInternal<CachedCB>(Slot, GetCBCount(), &ShaderResourceCacheD3D11::GetCBArrays, std::move(pBuffD3D11Impl), pd3d11Buff);
    }

    __forceinline void SetTexSRV(Uint32 Slot, RefCntAutoPtr<TextureViewD3D11Impl>&& pTexView)
    {
        auto pd3d11SRV = pTexView ? static_cast<ID3D11ShaderResourceView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(Slot, GetSRVCount(), &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pTexView), pd3d11SRV);
    }

    __forceinline void SetBufSRV(Uint32 Slot, RefCntAutoPtr<BufferViewD3D11Impl> &&pBuffView)
    {
        auto pd3d11SRV = pBuffView ? static_cast<ID3D11ShaderResourceView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(Slot, GetSRVCount(), &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pBuffView), pd3d11SRV);
    }

    __forceinline void SetTexUAV(Uint32 Slot, RefCntAutoPtr<TextureViewD3D11Impl>&& pTexView)
    {
        auto pd3d11UAV = pTexView ? static_cast<ID3D11UnorderedAccessView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(Slot, GetUAVCount(), &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pTexView), pd3d11UAV);
    }

    __forceinline void SetBufUAV(Uint32 Slot, RefCntAutoPtr<BufferViewD3D11Impl> &&pBuffView)
    {
        auto pd3d11UAV = pBuffView ? static_cast<ID3D11UnorderedAccessView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(Slot, GetUAVCount(), &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pBuffView), pd3d11UAV);
    }

    __forceinline void SetSampler(Uint32 Slot, SamplerD3D11Impl *pSampler)
    {
        auto *pd3d11Sampler = pSampler ? pSampler->SamplerD3D11Impl::GetD3D11SamplerState() : nullptr;
        SetD3D11ResourceInternal<CachedSampler>(Slot, GetSamplerCount(), &ShaderResourceCacheD3D11::GetSamplerArrays, pSampler, pd3d11Sampler);
    }



    __forceinline CachedCB& GetCB(Uint32 Slot)
    {
        VERIFY(Slot < GetCBCount(), "CB slot is out of range" );
        ShaderResourceCacheD3D11::CachedCB *CBs;
        ID3D11Buffer **pd3d11CBs;
        GetCBArrays(CBs, pd3d11CBs);
        return CBs[Slot];
    }

    __forceinline CachedResource& GetSRV(Uint32 Slot)
    {
        VERIFY(Slot < GetSRVCount(), "SRV slot is out of range" );
        ShaderResourceCacheD3D11::CachedResource *SRVResources;
        ID3D11ShaderResourceView **pd3d11SRVs;
        GetSRVArrays(SRVResources, pd3d11SRVs);
        return SRVResources[Slot];
    }

    __forceinline CachedResource& GetUAV(Uint32 Slot)
    {
        VERIFY(Slot < GetUAVCount(), "UAV slot is out of range" );
        ShaderResourceCacheD3D11::CachedResource *UAVResources;
        ID3D11UnorderedAccessView **pd3d11UAVs;
        GetUAVArrays(UAVResources, pd3d11UAVs);
        return UAVResources[Slot];
    }
    
    __forceinline CachedSampler& GetSampler(Uint32 Slot)
    {
        VERIFY(Slot < GetSamplerCount(), "Sampler slot is out of range" );
        ShaderResourceCacheD3D11::CachedSampler *Samplers;
        ID3D11SamplerState **pd3d11Samplers;
        GetSamplerArrays(Samplers, pd3d11Samplers);
        return Samplers[Slot];
    }


    __forceinline bool IsCBBound(Uint32 Slot)const
    {
        CachedCB* CBs = nullptr;
        ID3D11Buffer** d3d11CBs = nullptr;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetCBArrays(CBs, d3d11CBs);
        if( Slot < GetCBCount() && d3d11CBs[Slot] != nullptr )
        {
            VERIFY(CBs[Slot].pBuff != nullptr, "No relevant buffer resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsSRVBound(Uint32 Slot, bool dbgIsTextureView)const
    {
        CachedResource* SRVResources = nullptr;
        ID3D11ShaderResourceView** d3d11SRVs = nullptr;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetSRVArrays(SRVResources, d3d11SRVs);
        if( Slot < GetSRVCount() && d3d11SRVs[Slot] != nullptr )
        {
            VERIFY( ( dbgIsTextureView && SRVResources[Slot].pTexture != nullptr) || 
                    (!dbgIsTextureView && SRVResources[Slot].pBuffer  != nullptr), "No relevant resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsUAVBound(Uint32 Slot, bool dbgIsTextureView)const
    {
        CachedResource* UAVResources = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs = nullptr;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetUAVArrays(UAVResources, d3d11UAVs);
        if( Slot < GetUAVCount() && d3d11UAVs[Slot] != nullptr )
        {
            VERIFY( ( dbgIsTextureView && UAVResources[Slot].pTexture != nullptr) || 
                    (!dbgIsTextureView && UAVResources[Slot].pBuffer  != nullptr), "No relevant resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsSamplerBound(Uint32 Slot)const
    {
        CachedSampler* Samplers = nullptr;
        ID3D11SamplerState** d3d11Samplers = nullptr;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetSamplerArrays(Samplers, d3d11Samplers);
        if( Slot < GetSamplerCount() && d3d11Samplers[Slot] != nullptr )
        {
            VERIFY(Samplers[Slot].pSampler != nullptr, "No relevant sampler");
            return true;
        }
        return false;
    }

    void dbgVerifyCacheConsistency();

    
    static __forceinline Uint32 PackResourceCounts(Uint32 CBCount, Uint32 SRVCount, Uint32 SamplerCount, Uint32 UAVCount)
    {
        VERIFY_EXPR(CBCount      <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT ); 
        VERIFY_EXPR(SRVCount     <= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
        VERIFY_EXPR(SamplerCount <= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
        VERIFY_EXPR(UAVCount     <= D3D11_PS_CS_UAV_REGISTER_COUNT);
        return (CBCount & 0xFF) | ((SRVCount & 0xFF) << 8) | ((SamplerCount & 0xFF) << 16) | ((UAVCount & 0xFF) << 24);
    }

    static __forceinline Uint32 UnpackCBCount       (Uint32 PackedCounts) {return (PackedCounts >>   0) & 0xFF; }
    static __forceinline Uint32 UnpackSRVCount      (Uint32 PackedCounts) {return (PackedCounts >>   8) & 0xFF; }
    static __forceinline Uint32 UnpackSamplerCount  (Uint32 PackedCounts) {return (PackedCounts >>  16) & 0xFF; }
    static __forceinline Uint32 UnpackUAVCount      (Uint32 PackedCounts) {return (PackedCounts >>  24) & 0xFF; }

    __forceinline Uint32 GetCBCount()     const{ return UnpackCBCount     (m_ResourceCounts);}
    __forceinline Uint32 GetSRVCount()    const{ return UnpackSRVCount    (m_ResourceCounts);}
    __forceinline Uint32 GetSamplerCount()const{ return UnpackSamplerCount(m_ResourceCounts);}
    __forceinline Uint32 GetUAVCount()    const{ return UnpackUAVCount    (m_ResourceCounts);}
    __forceinline Uint32 GetPackedCounts()const{ return m_ResourceCounts;}

    __forceinline void GetResourceArrays(CachedCB*       &CBs,          ID3D11Buffer**              &pd3d11CBs, 
                                         CachedResource* &SRVResources, ID3D11ShaderResourceView**  &d3d11SRVs,
                                         CachedSampler*  &Samplers,     ID3D11SamplerState**        &pd3d11Samplers, 
                                         CachedResource* &UAVResources, ID3D11UnorderedAccessView** &pd3d11UAVs)
    {
        VERIFY_EXPR(IsInitialized());
        CBs =            reinterpret_cast<CachedCB*>( m_pResourceData );
        pd3d11CBs =      reinterpret_cast<ID3D11Buffer**>(        CBs + GetCBCount() );
        SRVResources =   reinterpret_cast<CachedResource*>( pd3d11CBs + GetCBCount() );
        d3d11SRVs =      reinterpret_cast<ID3D11ShaderResourceView**>( SRVResources + GetSRVCount() );
        Samplers =       reinterpret_cast<CachedSampler*>(             d3d11SRVs    + GetSRVCount() );
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState**>(  Samplers + GetSamplerCount() );
        UAVResources =   reinterpret_cast<CachedResource*>( pd3d11Samplers + GetSamplerCount() );
        pd3d11UAVs =     reinterpret_cast<ID3D11UnorderedAccessView**>( UAVResources + GetUAVCount() );
    }

    __forceinline void GetCBArrays(CachedCB* &CBs, ID3D11Buffer** &pd3d11CBs)
    {
        CBs = reinterpret_cast<CachedCB*>( m_pResourceData );
        pd3d11CBs = reinterpret_cast<ID3D11Buffer**>( CBs + GetCBCount() );
    }

    __forceinline void GetSRVArrays(CachedResource* &SRVResources, ID3D11ShaderResourceView** &d3d11SRVs)
    {
        SRVResources = reinterpret_cast<CachedResource*>( m_pResourceData + (sizeof(CachedCB) + sizeof(ID3D11Buffer*)) * GetCBCount() );
        d3d11SRVs = reinterpret_cast<ID3D11ShaderResourceView**>( SRVResources + GetSRVCount() );
    }

    __forceinline void GetSamplerArrays(CachedSampler* &Samplers, ID3D11SamplerState** &pd3d11Samplers)
    {
        Samplers = reinterpret_cast<CachedSampler*>( m_pResourceData + 
                    (sizeof(CachedCB)       + sizeof(ID3D11Buffer*))             * GetCBCount() + 
                    (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*)) * GetSRVCount() );
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState**>( Samplers + GetSamplerCount() );
    }

    __forceinline void GetUAVArrays(CachedResource* &UAVResources, ID3D11UnorderedAccessView** &pd3d11UAVs)
    {
        UAVResources = reinterpret_cast<CachedResource*>( m_pResourceData + 
                    (sizeof(CachedCB)       + sizeof(ID3D11Buffer*))             * GetCBCount() + 
                    (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*)) * GetSRVCount() +
                    (sizeof(CachedSampler)  + sizeof(ID3D11SamplerState*))       * GetSamplerCount() );
        pd3d11UAVs = reinterpret_cast<ID3D11UnorderedAccessView**>( UAVResources + GetUAVCount() );
    }

    __forceinline bool IsInitialized()const{return m_ResourceCounts != InvalidResourceCounts;}

private:

    template<typename TCachedResourceType, typename TGetResourceArraysFunc, typename TSrcResourceType, typename TD3D11ResourceType>
    __forceinline void SetD3D11ResourceInternal(Uint32 Slot, Uint32 Size,
                                                TGetResourceArraysFunc GetArrays,
                                                TSrcResourceType &&pResource, 
                                                TD3D11ResourceType *pd3d11Resource)
    {
        VERIFY(Slot < Size, "Resource cache is not big enough" );
        VERIFY(pResource != nullptr && pd3d11Resource != nullptr || pResource == nullptr && pd3d11Resource == nullptr, 
               "Resource and D3D11 resource must be set/unset atomically");
        TCachedResourceType* Resources;
        TD3D11ResourceType** d3d11ResArr;
        (this->*GetArrays)(Resources, d3d11ResArr);
        Resources[Slot].Set(std::forward<TSrcResourceType>(pResource));
        d3d11ResArr[Slot] = pd3d11Resource;
    }

    static constexpr Uint32 InvalidResourceCounts = 0xFFFFFFFF;
    // Resource limits in D3D11:
    // Max CB count:        14
    // Max SRV count:       128
    // Max Sampler count:   16
    // Max UAV count:       8
    Uint32 m_ResourceCounts = InvalidResourceCounts; // CBCount == (m_ResourceCounts >> 0) & 0xFF
                                                     // SRVCount == (m_ResourceCounts >> 8) & 0xFF
                                                     // SamplerCount == (m_ResourceCounts >> 16) & 0xFF
                                                     // UAVCount == (m_ResourceCounts >> 24) & 0xFF
    Uint8 *m_pResourceData = nullptr;

#ifdef _DEBUG
    IMemoryAllocator *m_pdbgMemoryAllocator = nullptr;
#endif
};

}
