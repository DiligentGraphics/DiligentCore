/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "MemoryAllocator.h"
#include "ShaderResourceCacheCommon.hpp"
#include "TextureBaseD3D11.hpp"
#include "BufferD3D11Impl.hpp"
#include "SamplerD3D11Impl.hpp"
#include "PipelineResourceAttribsD3D11.hpp"

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
    explicit ShaderResourceCacheD3D11(ResourceCacheContentType ContentType) noexcept :
        m_ContentType{ContentType}
    {}

    ~ShaderResourceCacheD3D11();

    // clang-format off
    ShaderResourceCacheD3D11             (const ShaderResourceCacheD3D11&) = delete;
    ShaderResourceCacheD3D11& operator = (const ShaderResourceCacheD3D11&) = delete;
    ShaderResourceCacheD3D11             (ShaderResourceCacheD3D11&&)      = delete;
    ShaderResourceCacheD3D11& operator = (ShaderResourceCacheD3D11&&)      = delete;
    // clang-format on

    enum class StateTransitionMode
    {
        Transition,
        Verify
    };
    // Transitions all resources in the cache
    void TransitionResourceStates(DeviceContextD3D11Impl& Ctx, StateTransitionMode Mode);


    static constexpr int NumShaderTypes = BindPointsD3D11::NumShaderTypes;

    struct TCommittedResources
    {
        // clang-format off

        /// An array of D3D11 constant buffers committed to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do
        /// not need to keep strong references.
        ID3D11Buffer*              D3D11CBs     [NumShaderTypes][D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    
        /// An array of D3D11 shader resource views committed to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do 
        /// not need to keep strong references.
        ID3D11ShaderResourceView*  D3D11SRVs    [NumShaderTypes][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    
        /// An array of D3D11 samplers committed to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do 
        /// not need to keep strong references.
        ID3D11SamplerState*        D3D11Samplers[NumShaderTypes][D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    
        /// An array of D3D11 UAVs committed to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do 
        /// not need to keep strong references.
        ID3D11UnorderedAccessView* D3D11UAVs    [NumShaderTypes][D3D11_PS_CS_UAV_REGISTER_COUNT] = {};

        /// An array of D3D11 resources commited as SRV to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do 
        /// not need to keep strong references.
        ID3D11Resource*  D3D11SRVResources      [NumShaderTypes][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};

        /// An array of D3D11 resources commited as UAV to D3D11 device context,
        /// for each shader type. The context addref's all bound resources, so we do 
        /// not need to keep strong references.
        ID3D11Resource*  D3D11UAVResources      [NumShaderTypes][D3D11_PS_CS_UAV_REGISTER_COUNT] = {};

        Uint8 NumCBs     [NumShaderTypes] = {};
        Uint8 NumSRVs    [NumShaderTypes] = {};
        Uint8 NumSamplers[NumShaderTypes] = {};
        Uint8 NumUAVs    [NumShaderTypes] = {};

        // clang-format on

        void Clear();
    };

    struct MinMaxSlot
    {
        UINT MinSlot = UINT_MAX;
        UINT MaxSlot = 0;
    };
    using TMinMaxSlotPerStage = std::array<std::array<MinMaxSlot, DESCRIPTOR_RANGE_COUNT>, NumShaderTypes>;
    using TResourceCount      = std::array<Uint8, DESCRIPTOR_RANGE_COUNT>;
    using TBindingsPerStage   = std::array<TResourceCount, NumShaderTypes>;

    void BindResources(DeviceContextD3D11Impl& Ctx, const TBindingsPerStage& BaseBindings, TMinMaxSlotPerStage& MinMaxSlot, TCommittedResources& CommittedRes, SHADER_TYPE ActiveStages) const;


    /// Describes a resource associated with a cached constant buffer
    struct CachedCB
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferD3D11Impl> pBuff;

    private:
        friend class ShaderResourceCacheD3D11;
        __forceinline void Set(RefCntAutoPtr<BufferD3D11Impl>&& _pBuff)
        {
            pBuff = std::move(_pBuff);
        }
    };

    /// Describes a resource associated with a cached sampler
    struct CachedSampler
    {
        /// Strong reference to the sampler
        RefCntAutoPtr<class SamplerD3D11Impl> pSampler;

    private:
        friend class ShaderResourceCacheD3D11;
        __forceinline void Set(SamplerD3D11Impl* pSam)
        {
            pSampler = pSam;
        }
    };

    /// Describes a resource associated with a cached SRV or a UAV
    struct CachedResource
    {
        /// Wee keep strong reference to the view instead of the reference
        /// to the texture or buffer because this is more efficient from
        /// performance point of view: this avoids one pair of
        /// AddStrongRef()/ReleaseStrongRef(). The view holds strong reference
        /// to the texture or the buffer, so it makes no difference.
        RefCntAutoPtr<IDeviceObject> pView;

        TextureBaseD3D11* pTexture = nullptr;
        BufferD3D11Impl*  pBuffer  = nullptr;

        // There is no need to keep strong reference to D3D11 resource as
        // it is already kept by either pTexture or pBuffer
        ID3D11Resource* pd3d11Resource = nullptr;

        CachedResource() noexcept {}

    private:
        friend class ShaderResourceCacheD3D11;
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

    static size_t GetRequriedMemorySize(const TResourceCount& ResCount);

    void Initialize(const TResourceCount& ResCount, IMemoryAllocator& MemAllocator);

    __forceinline void SetCB(Uint32 CacheOffset, BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferD3D11Impl>&& pBuffD3D11Impl)
    {
        auto* pd3d11Buff = pBuffD3D11Impl ? pBuffD3D11Impl->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        SetD3D11ResourceInternal<CachedCB>(CacheOffset, GetCBCount(), BindPoints, &ShaderResourceCacheD3D11::GetCBArrays, std::move(pBuffD3D11Impl), pd3d11Buff);
    }

    __forceinline void SetTexSRV(Uint32 CacheOffset, BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl>&& pTexView)
    {
        auto* pd3d11SRV = pTexView ? static_cast<ID3D11ShaderResourceView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(CacheOffset, GetSRVCount(), BindPoints, &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pTexView), pd3d11SRV);
    }

    __forceinline void SetBufSRV(Uint32 CacheOffset, BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl>&& pBuffView)
    {
        auto* pd3d11SRV = pBuffView ? static_cast<ID3D11ShaderResourceView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(CacheOffset, GetSRVCount(), BindPoints, &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pBuffView), pd3d11SRV);
    }

    __forceinline void SetTexUAV(Uint32 CacheOffset, BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl>&& pTexView)
    {
        auto* pd3d11UAV = pTexView ? static_cast<ID3D11UnorderedAccessView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(CacheOffset, GetUAVCount(), BindPoints, &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pTexView), pd3d11UAV);
    }

    __forceinline void SetBufUAV(Uint32 CacheOffset, BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl>&& pBuffView)
    {
        auto* pd3d11UAV = pBuffView ? static_cast<ID3D11UnorderedAccessView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(CacheOffset, GetUAVCount(), BindPoints, &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pBuffView), pd3d11UAV);
    }

    __forceinline void SetSampler(Uint32 CacheOffset, BindPointsD3D11 BindPoints, SamplerD3D11Impl* pSampler)
    {
        auto* pd3d11Sampler = pSampler ? pSampler->SamplerD3D11Impl::GetD3D11SamplerState() : nullptr;
        SetD3D11ResourceInternal<CachedSampler>(CacheOffset, GetSamplerCount(), BindPoints, &ShaderResourceCacheD3D11::GetSamplerArrays, pSampler, pd3d11Sampler);
    }


    __forceinline CachedCB const& GetCB(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetCBCount(), "CB slot is out of range");
        ShaderResourceCacheD3D11::CachedCB* CBs;
        ID3D11Buffer**                      pd3d11CBs;
        BindPointsD3D11*                    bindPoints;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetCBArrays(CBs, pd3d11CBs, bindPoints);
        return CBs[CacheOffset];
    }

    __forceinline CachedResource const& GetSRV(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetSRVCount(), "SRV slot is out of range");
        ShaderResourceCacheD3D11::CachedResource* SRVResources;
        ID3D11ShaderResourceView**                pd3d11SRVs;
        BindPointsD3D11*                          bindPoints;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetSRVArrays(SRVResources, pd3d11SRVs, bindPoints);
        return SRVResources[CacheOffset];
    }

    __forceinline CachedResource const& GetUAV(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetUAVCount(), "UAV slot is out of range");
        ShaderResourceCacheD3D11::CachedResource* UAVResources;
        ID3D11UnorderedAccessView**               pd3d11UAVs;
        BindPointsD3D11*                          bindPoints;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetUAVArrays(UAVResources, pd3d11UAVs, bindPoints);
        return UAVResources[CacheOffset];
    }

    __forceinline CachedSampler const& GetSampler(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetSamplerCount(), "Sampler slot is out of range");
        ShaderResourceCacheD3D11::CachedSampler* Samplers;
        ID3D11SamplerState**                     pd3d11Samplers;
        BindPointsD3D11*                         bindPoints;
        const_cast<ShaderResourceCacheD3D11*>(this)->GetSamplerArrays(Samplers, pd3d11Samplers, bindPoints);
        return Samplers[CacheOffset];
    }


    __forceinline bool IsCBBound(Uint32 CacheOffset) const
    {
        CachedCB const*        CBs;
        ID3D11Buffer* const*   d3d11CBs;
        BindPointsD3D11 const* bindPoints;
        GetConstCBArrays(CBs, d3d11CBs, bindPoints);
        if (CacheOffset < GetCBCount() && d3d11CBs[CacheOffset] != nullptr)
        {
            VERIFY(CBs[CacheOffset].pBuff != nullptr, "No relevant buffer resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsSRVBound(Uint32 CacheOffset, bool dbgIsTextureView) const
    {
        CachedResource const*            SRVResources;
        ID3D11ShaderResourceView* const* d3d11SRVs;
        BindPointsD3D11 const*           bindPoints;
        GetConstSRVArrays(SRVResources, d3d11SRVs, bindPoints);
        if (CacheOffset < GetSRVCount() && d3d11SRVs[CacheOffset] != nullptr)
        {
            VERIFY((dbgIsTextureView && SRVResources[CacheOffset].pTexture != nullptr) || (!dbgIsTextureView && SRVResources[CacheOffset].pBuffer != nullptr),
                   "No relevant resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsUAVBound(Uint32 CacheOffset, bool dbgIsTextureView) const
    {
        CachedResource const*             UAVResources;
        ID3D11UnorderedAccessView* const* d3d11UAVs;
        BindPointsD3D11 const*            bindPoints;
        GetConstUAVArrays(UAVResources, d3d11UAVs, bindPoints);
        if (CacheOffset < GetUAVCount() && d3d11UAVs[CacheOffset] != nullptr)
        {
            VERIFY((dbgIsTextureView && UAVResources[CacheOffset].pTexture != nullptr) || (!dbgIsTextureView && UAVResources[CacheOffset].pBuffer != nullptr),
                   "No relevant resource");
            return true;
        }
        return false;
    }

    __forceinline bool IsSamplerBound(Uint32 CacheOffset) const
    {
        CachedSampler const*       Samplers;
        ID3D11SamplerState* const* d3d11Samplers;
        BindPointsD3D11 const*     bindPoints;
        GetConstSamplerArrays(Samplers, d3d11Samplers, bindPoints);
        if (CacheOffset < GetSamplerCount() && d3d11Samplers[CacheOffset] != nullptr)
        {
            VERIFY(Samplers[CacheOffset].pSampler != nullptr, "No relevant sampler");
            return true;
        }
        return false;
    }

#ifdef DILIGENT_DEVELOPMENT
    void DvpVerifyCacheConsistency();
#endif

    // clang-format off
    __forceinline Uint32 GetCBCount() const      { return m_CBCount; }
    __forceinline Uint32 GetSRVCount() const     { return m_SRVCount; }
    __forceinline Uint32 GetSamplerCount() const { return m_SamplerCount; }
    __forceinline Uint32 GetUAVCount() const     { return m_UAVCount; }
    // clang-format on

    __forceinline void GetCBArrays(CachedCB*& CBs, ID3D11Buffer**& pd3d11CBs, BindPointsD3D11*& pBindPoints)
    {
        VERIFY(alignof(CachedCB) == alignof(ID3D11Buffer*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        CBs         = reinterpret_cast<CachedCB*>(m_pResourceData + m_CBOffset);
        pd3d11CBs   = reinterpret_cast<ID3D11Buffer**>(CBs + GetCBCount());
        pBindPoints = reinterpret_cast<BindPointsD3D11*>(pd3d11CBs + GetCBCount());
    }

    __forceinline void GetSRVArrays(CachedResource*& SRVResources, ID3D11ShaderResourceView**& d3d11SRVs, BindPointsD3D11*& pBindPoints)
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11ShaderResourceView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        SRVResources = reinterpret_cast<CachedResource*>(m_pResourceData + m_SRVOffset);
        d3d11SRVs    = reinterpret_cast<ID3D11ShaderResourceView**>(SRVResources + GetSRVCount());
        pBindPoints  = reinterpret_cast<BindPointsD3D11*>(d3d11SRVs + GetSRVCount());
    }

    __forceinline void GetSamplerArrays(CachedSampler*& Samplers, ID3D11SamplerState**& pd3d11Samplers, BindPointsD3D11*& pBindPoints)
    {
        VERIFY(alignof(CachedSampler) == alignof(ID3D11SamplerState*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        Samplers       = reinterpret_cast<CachedSampler*>(m_pResourceData + m_SamplerOffset);
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState**>(Samplers + GetSamplerCount());
        pBindPoints    = reinterpret_cast<BindPointsD3D11*>(pd3d11Samplers + GetSamplerCount());
    }

    __forceinline void GetUAVArrays(CachedResource*& UAVResources, ID3D11UnorderedAccessView**& pd3d11UAVs, BindPointsD3D11*& pBindPoints)
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11UnorderedAccessView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        UAVResources = reinterpret_cast<CachedResource*>(m_pResourceData + m_UAVOffset);
        pd3d11UAVs   = reinterpret_cast<ID3D11UnorderedAccessView**>(UAVResources + GetUAVCount());
        pBindPoints  = reinterpret_cast<BindPointsD3D11*>(pd3d11UAVs + GetUAVCount());
    }

    __forceinline void GetConstCBArrays(CachedCB const*& CBs, ID3D11Buffer* const*& pd3d11CBs, BindPointsD3D11 const*& pBindPoints) const
    {
        VERIFY(alignof(CachedCB) == alignof(ID3D11Buffer*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        CBs         = reinterpret_cast<CachedCB const*>(m_pResourceData + m_CBOffset);
        pd3d11CBs   = reinterpret_cast<ID3D11Buffer* const*>(CBs + GetCBCount());
        pBindPoints = reinterpret_cast<BindPointsD3D11 const*>(pd3d11CBs + GetCBCount());
    }

    __forceinline void GetConstSRVArrays(CachedResource const*& SRVResources, ID3D11ShaderResourceView* const*& d3d11SRVs, BindPointsD3D11 const*& pBindPoints) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11ShaderResourceView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        SRVResources = reinterpret_cast<CachedResource const*>(m_pResourceData + m_SRVOffset);
        d3d11SRVs    = reinterpret_cast<ID3D11ShaderResourceView* const*>(SRVResources + GetSRVCount());
        pBindPoints  = reinterpret_cast<BindPointsD3D11 const*>(d3d11SRVs + GetSRVCount());
    }

    __forceinline void GetConstSamplerArrays(CachedSampler const*& Samplers, ID3D11SamplerState* const*& pd3d11Samplers, BindPointsD3D11 const*& pBindPoints) const
    {
        VERIFY(alignof(CachedSampler) == alignof(ID3D11SamplerState*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        Samplers       = reinterpret_cast<CachedSampler const*>(m_pResourceData + m_SamplerOffset);
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState* const*>(Samplers + GetSamplerCount());
        pBindPoints    = reinterpret_cast<BindPointsD3D11 const*>(pd3d11Samplers + GetSamplerCount());
    }

    __forceinline void GetConstUAVArrays(CachedResource const*& UAVResources, ID3D11UnorderedAccessView* const*& pd3d11UAVs, BindPointsD3D11 const*& pBindPoints) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11UnorderedAccessView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        UAVResources = reinterpret_cast<CachedResource const*>(m_pResourceData + m_UAVOffset);
        pd3d11UAVs   = reinterpret_cast<ID3D11UnorderedAccessView* const*>(UAVResources + GetUAVCount());
        pBindPoints  = reinterpret_cast<BindPointsD3D11 const*>(pd3d11UAVs + GetUAVCount());
    }

    __forceinline bool IsInitialized() const
    {
        return m_UAVOffset != InvalidResourceOffset;
    }

    ResourceCacheContentType GetContentType() const { return m_ContentType; }

private:
    template <typename TCachedResourceType, typename TGetResourceArraysFunc, typename TSrcResourceType, typename TD3D11ResourceType>
    __forceinline void SetD3D11ResourceInternal(Uint32 CacheOffset, Uint32 Size, BindPointsD3D11 BindPoints, TGetResourceArraysFunc GetArrays, TSrcResourceType&& pResource, TD3D11ResourceType* pd3d11Resource)
    {
        VERIFY(CacheOffset < Size, "Resource cache is not big enough");
        VERIFY(pResource != nullptr && pd3d11Resource != nullptr || pResource == nullptr && pd3d11Resource == nullptr,
               "Resource and D3D11 resource must be set/unset atomically");
        TCachedResourceType* Resources;
        TD3D11ResourceType** d3d11ResArr;
        BindPointsD3D11*     bindPoints;
        (this->*GetArrays)(Resources, d3d11ResArr, bindPoints);
        Resources[CacheOffset].Set(std::forward<TSrcResourceType>(pResource));
        bindPoints[CacheOffset]  = BindPoints;
        d3d11ResArr[CacheOffset] = pd3d11Resource;
    }

    template <typename THandleResource>
    void ProcessResources(THandleResource HandleResource);

    // Transitions resource to the shader resource state required by Type member.
    __forceinline void TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedCB* CBs, ID3D11Buffer** pd3d11CBs);
    __forceinline void TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedResource* SRVResources, ID3D11ShaderResourceView** d3d11SRVs);
    __forceinline void TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedSampler* Samplers, ID3D11SamplerState** pd3d11Samplers);
    __forceinline void TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedResource* UAVResources, ID3D11UnorderedAccessView** pd3d11UAVs);

#ifdef DILIGENT_DEVELOPMENT
    // Verifies that resource is in correct shader resource state required by Type member.
    void DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedCB* CBs, ID3D11Buffer**);
    void DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedResource* SRVResources, ID3D11ShaderResourceView**);
    void DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedSampler* Samplers, ID3D11SamplerState**);
    void DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedResource* UAVResources, ID3D11UnorderedAccessView**);
#endif

private:
    using OffsetType = Uint16;

    static constexpr size_t MaxAlignment = std::max(std::max(std::max(alignof(CachedCB), alignof(CachedResource)),
                                                             std::max(alignof(CachedSampler), alignof(BindPointsD3D11))),
                                                    std::max(std::max(alignof(ID3D11Buffer*), alignof(ID3D11ShaderResourceView*)),
                                                             std::max(alignof(ID3D11SamplerState*), alignof(ID3D11UnorderedAccessView*))));

    static constexpr OffsetType InvalidResourceOffset = std::numeric_limits<OffsetType>::max();

    static constexpr OffsetType m_CBOffset      = 0;
    OffsetType                  m_SRVOffset     = InvalidResourceOffset;
    OffsetType                  m_SamplerOffset = InvalidResourceOffset;
    OffsetType                  m_UAVOffset     = InvalidResourceOffset;

    static constexpr Uint32 _CBCountBits   = 7;
    static constexpr Uint32 _SVCountBits   = 10;
    static constexpr Uint32 _SampCountBits = 7;
    static constexpr Uint32 _UAVCountBits  = 4;

    // clang-format off
    static_assert((1U << _CBCountBits)   >= (D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * NumShaderTypes), "Number of constant buffers is too small");
    static_assert((1U << _SVCountBits)   >= (D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT      * NumShaderTypes), "Number of shader resources is too small");
    static_assert((1U << _SampCountBits) >= (D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT             * NumShaderTypes), "Number of samplers is too small");
    static_assert((1U << _UAVCountBits)  >= D3D11_PS_CS_UAV_REGISTER_COUNT,                                       "Number of UAVs is too small");

    Uint32 m_CBCount      : _CBCountBits;
    Uint32 m_SRVCount     : _SVCountBits;
    Uint32 m_SamplerCount : _SampCountBits;
    Uint32 m_UAVCount     : _UAVCountBits;
    // clang-format on

    // Indicates what types of resources are stored in the cache
    const ResourceCacheContentType m_ContentType;

    Uint8*            m_pResourceData = nullptr;
    IMemoryAllocator* m_pAllocator    = nullptr;
};

} // namespace Diligent
