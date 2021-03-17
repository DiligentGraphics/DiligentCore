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

#include <array>
#include <memory>

#include "MemoryAllocator.h"
#include "ShaderResourceCacheCommon.hpp"
#include "PipelineResourceAttribsD3D11.hpp"

#include "TextureBaseD3D11.hpp"
#include "TextureViewD3D11Impl.hpp"
#include "BufferD3D11Impl.hpp"
#include "BufferViewD3D11Impl.hpp"
#include "SamplerD3D11Impl.hpp"

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
    template <StateTransitionMode Mode>
    void TransitionResourceStates(DeviceContextD3D11Impl& Ctx);

    /// Describes a resource associated with a cached constant buffer
    struct CachedCB
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferD3D11Impl> pBuff;

        explicit operator bool() const
        {
            return pBuff;
        }

    private:
        friend class ShaderResourceCacheD3D11;
        __forceinline void Set(RefCntAutoPtr<BufferD3D11Impl> _pBuff)
        {
            pBuff = std::move(_pBuff);
        }
    };

    /// Describes a resource associated with a cached sampler
    struct CachedSampler
    {
        /// Strong reference to the sampler
        RefCntAutoPtr<SamplerD3D11Impl> pSampler;

        explicit operator bool() const
        {
            return pSampler;
        }

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

        explicit operator bool() const
        {
            VERIFY_EXPR((pView && pd3d11Resource != nullptr) || (!pView && pd3d11Resource == nullptr));
            VERIFY_EXPR(pTexture == nullptr || pBuffer == nullptr);
            VERIFY_EXPR((pView && (pTexture != nullptr || pBuffer != nullptr)) || (!pView && (pTexture == nullptr && pBuffer == nullptr)));
            return pView;
        }

    private:
        friend class ShaderResourceCacheD3D11;
        __forceinline void Set(RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
        {
            pBuffer = nullptr;
            // Avoid unnecessary virtual function calls
            pTexture       = pTexView ? pTexView->GetTexture<TextureBaseD3D11>() : nullptr;
            pView          = std::move(pTexView);
            pd3d11Resource = pTexture ? pTexture->TextureBaseD3D11::GetD3D11Texture() : nullptr;
        }

        __forceinline void Set(RefCntAutoPtr<BufferViewD3D11Impl> pBufView)
        {
            pTexture = nullptr;
            // Avoid unnecessary virtual function calls
            pBuffer        = pBufView ? pBufView->GetBuffer<BufferD3D11Impl>() : nullptr;
            pView          = std::move(pBufView);
            pd3d11Resource = pBuffer ? pBuffer->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        }
    };

    template <typename D3D11ResourceType>
    struct CachedResourceTraits;

    static constexpr int NumShaderTypes = BindPointsD3D11::NumShaderTypes;
    using TBindingsPerStage             = std::array<std::array<Uint8, NumShaderTypes>, D3D11_RESOURCE_RANGE_COUNT>;

    static size_t GetRequriedMemorySize(const TBindingsPerStage& ResCount);

    void Initialize(const TBindingsPerStage& ResCount, IMemoryAllocator& MemAllocator);

    __forceinline void SetCB(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferD3D11Impl> pBuffD3D11Impl)
    {
        auto* pd3d11Buff = pBuffD3D11Impl ? pBuffD3D11Impl->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        SetD3D11ResourceInternal<CachedCB>(BindPoints, std::move(pBuffD3D11Impl), pd3d11Buff);
    }

    __forceinline void SetTexSRV(BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11SRV = pTexView ? static_cast<ID3D11ShaderResourceView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, std::move(pTexView), pd3d11SRV);
    }

    __forceinline void SetBufSRV(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11SRV = pBuffView ? static_cast<ID3D11ShaderResourceView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, std::move(pBuffView), pd3d11SRV);
    }

    __forceinline void SetTexUAV(BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11UAV = pTexView ? static_cast<ID3D11UnorderedAccessView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, std::move(pTexView), pd3d11UAV);
    }

    __forceinline void SetBufUAV(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11UAV = pBuffView ? static_cast<ID3D11UnorderedAccessView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, std::move(pBuffView), pd3d11UAV);
    }

    __forceinline void SetSampler(BindPointsD3D11 BindPoints, SamplerD3D11Impl* pSampler)
    {
        auto* pd3d11Sampler = pSampler ? pSampler->SamplerD3D11Impl::GetD3D11SamplerState() : nullptr;
        SetD3D11ResourceInternal<CachedSampler>(BindPoints, pSampler, pd3d11Sampler);
    }


    template <typename D3D11ResourceType>
    __forceinline const typename CachedResourceTraits<D3D11ResourceType>::CachedResourceType& GetResource(BindPointsD3D11 BindPoints) const
    {
        const Uint32 ShaderInd = PlatformMisc::GetLSB(BindPoints.GetActiveBits());
        VERIFY(BindPoints[ShaderInd] < GetResourceCount<D3D11ResourceType>(ShaderInd), "Resource slot is out of range");
        CachedResourceTraits<D3D11ResourceType>::CachedResourceType const* CachedResources;
        D3D11ResourceType* const*                                          pd3d11Resources;
        GetConstResourceArrays(ShaderInd, CachedResources, pd3d11Resources);
        return CachedResources[BindPoints[ShaderInd]];
    }

    template <typename D3D11ResourceType>
    bool CopyResource(const ShaderResourceCacheD3D11& SrcCache, BindPointsD3D11 BindPoints)
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            CachedResourceTraits<D3D11ResourceType>::CachedResourceType const* pSrcCachedResources;
            D3D11ResourceType* const*                                          pd3d11SrcResources;
            SrcCache.GetConstResourceArrays(ShaderInd, pSrcCachedResources, pd3d11SrcResources);

            CachedResourceTraits<D3D11ResourceType>::CachedResourceType* pDstCachedResources;
            D3D11ResourceType**                                          pd3d11DstResources;
            GetResourceArrays(ShaderInd, pDstCachedResources, pd3d11DstResources);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetResourceCount<D3D11ResourceType>(ShaderInd), "Index is out of range");
            if (!pSrcCachedResources[CacheOffset])
                IsBound = false;

            pDstCachedResources[CacheOffset] = pSrcCachedResources[CacheOffset];
            pd3d11DstResources[CacheOffset]  = pd3d11SrcResources[CacheOffset];
        }
        return IsBound;
    }

    template <typename D3D11ResourceType>
    __forceinline bool IsResourceBound(BindPointsD3D11 BindPoints) const
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);
            const Uint32 CacheOffset = BindPoints[ShaderInd];

            CachedResourceTraits<D3D11ResourceType>::CachedResourceType const* CachedResources;
            D3D11ResourceType* const*                                          pd3d11Resources;
            GetConstResourceArrays(ShaderInd, CachedResources, pd3d11Resources);
            if (CacheOffset < GetResourceCount<D3D11ResourceType>(ShaderInd) && CachedResources[CacheOffset])
            {
                continue;
            }
            IsBound = false;
        }
        return IsBound;
    }

#ifdef DILIGENT_DEVELOPMENT
    void DvpVerifyCacheConsistency();
#endif

    // clang-format off
    __forceinline Uint32 GetCBCount     (Uint32 ShaderInd) const { return (m_Offsets[CBOffset   + ShaderInd + 1] - m_Offsets[CBOffset   + ShaderInd]) / (sizeof(CachedCB)       + sizeof(ID3D11Buffer*));              }
    __forceinline Uint32 GetSRVCount    (Uint32 ShaderInd) const { return (m_Offsets[SRVOffset  + ShaderInd + 1] - m_Offsets[SRVOffset  + ShaderInd]) / (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*));  }
    __forceinline Uint32 GetSamplerCount(Uint32 ShaderInd) const { return (m_Offsets[SampOffset + ShaderInd + 1] - m_Offsets[SampOffset + ShaderInd]) / (sizeof(CachedSampler)  + sizeof(ID3D11SamplerState*));        }
    __forceinline Uint32 GetUAVCount    (Uint32 ShaderInd) const { return (m_Offsets[UAVOffset  + ShaderInd + 1] - m_Offsets[UAVOffset  + ShaderInd]) / (sizeof(CachedResource) + sizeof(ID3D11UnorderedAccessView*)); }
    // clang-format on

    template <typename D3D11ResourceType>
    __forceinline Uint32 GetResourceCount(Uint32 ShaderInd) const;

    template <typename D3D11ResourceType>
    __forceinline void GetResourceArrays(
        Uint32                                                                 ShaderInd,
        typename CachedResourceTraits<D3D11ResourceType>::CachedResourceType*& pResources,
        D3D11ResourceType**&                                                   pd3d11Resources) const
    {
        static_assert(alignof(CachedResourceTraits<D3D11ResourceType>::CachedResourceType) == alignof(D3D11ResourceType*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");

        pResources      = reinterpret_cast<CachedResourceTraits<D3D11ResourceType>::CachedResourceType*>(m_pResourceData.get() + GetResourceDataOffset<D3D11ResourceType>(ShaderInd));
        pd3d11Resources = reinterpret_cast<D3D11ResourceType**>(pResources + GetResourceCount<D3D11ResourceType>(ShaderInd));
    }


    template <typename D3D11ResourceType>
    __forceinline void GetConstResourceArrays(
        Uint32                                                                       ShaderInd,
        typename CachedResourceTraits<D3D11ResourceType>::CachedResourceType const*& pResources,
        D3D11ResourceType* const*&                                                   pd3d11Resources) const
    {
        static_assert(alignof(CachedResourceTraits<D3D11ResourceType>::CachedResourceType) == alignof(D3D11ResourceType*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");

        pResources      = reinterpret_cast<CachedResourceTraits<D3D11ResourceType>::CachedResourceType const*>(m_pResourceData.get() + GetResourceDataOffset<D3D11ResourceType>(ShaderInd));
        pd3d11Resources = reinterpret_cast<D3D11ResourceType* const*>(pResources + GetResourceCount<D3D11ResourceType>(ShaderInd));
    }

    bool IsInitialized() const { return m_IsInitialized; }

    ResourceCacheContentType GetContentType() const { return m_ContentType; }

    struct MinMaxSlot
    {
        UINT MinSlot = UINT_MAX;
        UINT MaxSlot = 0;

        void Add(UINT Slot)
        {
            MinSlot = std::min(MinSlot, Slot);

            VERIFY_EXPR(Slot >= MaxSlot);
            MaxSlot = Slot;
        }

        explicit operator bool() const
        {
            return MinSlot <= MaxSlot;
        }
    };

    template <typename D3D11ResourceType>
    MinMaxSlot BindResources(Uint32             ShaderInd,
                             D3D11ResourceType* CommittedD3D11Resources[],
                             Uint8&             Binding) const;

    template <typename D3D11ResourceViewType>
    MinMaxSlot BindResourceViews(Uint32                 ShaderInd,
                                 D3D11ResourceViewType* CommittedD3D11Views[],
                                 ID3D11Resource*        CommittedD3D11Resources[],
                                 Uint8&                 Binding) const;

private:
    template <typename D3D11ResourceTpye>
    __forceinline Uint32 GetResourceDataOffset(Uint32 ShaderInd) const;

    template <typename TCachedResourceType, typename TSrcResourceType, typename TD3D11ResourceType>
    __forceinline void SetD3D11ResourceInternal(BindPointsD3D11 BindPoints, TSrcResourceType pResource, TD3D11ResourceType* pd3d11Resource)
    {
        VERIFY(pResource != nullptr && pd3d11Resource != nullptr || pResource == nullptr && pd3d11Resource == nullptr,
               "Resource and D3D11 resource must be set/unset atomically");
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            const Uint32 ResCount    = GetResourceCount<TD3D11ResourceType>(ShaderInd);
            VERIFY(CacheOffset < ResCount, "Index is out of range");

            TCachedResourceType* Resources;
            TD3D11ResourceType** d3d11ResArr;
            GetResourceArrays(ShaderInd, Resources, d3d11ResArr);
            Resources[CacheOffset].Set(pResource);
            d3d11ResArr[CacheOffset] = pd3d11Resource;
        }
    }

    // Transitions or verifies the resource state.
    template <StateTransitionMode Mode>
    void TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11Buffer* /*Selector*/) const;

    template <StateTransitionMode Mode>
    void TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11ShaderResourceView* /*Selector*/) const;

    template <StateTransitionMode Mode>
    void TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11SamplerState* /*Selector*/) const;

    template <StateTransitionMode Mode>
    void TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11UnorderedAccessView* /*Selector*/) const;

private:
    using OffsetType = Uint16;

    static constexpr size_t MaxAlignment = std::max(std::max(std::max(alignof(CachedCB), alignof(CachedResource)), alignof(CachedSampler)),
                                                    std::max(std::max(alignof(ID3D11Buffer*), alignof(ID3D11ShaderResourceView*)),
                                                             std::max(alignof(ID3D11SamplerState*), alignof(ID3D11UnorderedAccessView*))));

    static constexpr Uint32 CBOffset   = 0;
    static constexpr Uint32 SRVOffset  = CBOffset + NumShaderTypes;
    static constexpr Uint32 SampOffset = SRVOffset + NumShaderTypes;
    static constexpr Uint32 UAVOffset  = SampOffset + NumShaderTypes;
    static constexpr Uint32 MaxOffsets = UAVOffset + NumShaderTypes + 1;

    std::array<OffsetType, MaxOffsets> m_Offsets = {};

    bool m_IsInitialized = false;

    // Indicates what types of resources are stored in the cache
    const ResourceCacheContentType m_ContentType;

    std::unique_ptr<Uint8, STDDeleter<Uint8, IMemoryAllocator>> m_pResourceData;
};

static constexpr size_t ResCacheSize = sizeof(ShaderResourceCacheD3D11);


template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<ID3D11Buffer>
{
    using CachedResourceType = CachedCB;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<ID3D11SamplerState>
{
    using CachedResourceType = CachedSampler;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<ID3D11ShaderResourceView>
{
    using CachedResourceType = CachedResource;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<ID3D11UnorderedAccessView>
{
    using CachedResourceType = CachedResource;
};


template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<ID3D11Buffer>(Uint32 ShaderInd) const
{
    return GetCBCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<ID3D11ShaderResourceView>(Uint32 ShaderInd) const
{
    return GetSRVCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<ID3D11UnorderedAccessView>(Uint32 ShaderInd) const
{
    return GetUAVCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<ID3D11SamplerState>(Uint32 ShaderInd) const
{
    return GetSamplerCount(ShaderInd);
}


template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<ID3D11Buffer>(Uint32 ShaderInd) const
{
    return m_Offsets[CBOffset + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<ID3D11ShaderResourceView>(Uint32 ShaderInd) const
{
    return m_Offsets[SRVOffset + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<ID3D11SamplerState>(Uint32 ShaderInd) const
{
    return m_Offsets[SampOffset + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<ID3D11UnorderedAccessView>(Uint32 ShaderInd) const
{
    return m_Offsets[UAVOffset + ShaderInd];
}

// Instantiate templates
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(DeviceContextD3D11Impl& Ctx);
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Verify>(DeviceContextD3D11Impl& Ctx);


template <typename D3D11ResourceType>
ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindResources(
    Uint32             ShaderInd,
    D3D11ResourceType* CommittedD3D11Resources[],
    Uint8&             Binding) const
{
    CachedResourceTraits<D3D11ResourceType>::CachedResourceType const* CachedResources;
    D3D11ResourceType* const*                                          pd3d11Resources;
    GetConstResourceArrays(ShaderInd, CachedResources, pd3d11Resources);

    MinMaxSlot Slots;

    const auto ResCount = GetResourceCount<D3D11ResourceType>(ShaderInd);
    for (Uint32 res = 0; res < ResCount; ++res)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11Resources[Slot] != pd3d11Resources[res])
            Slots.Add(Slot);

        VERIFY_EXPR(pd3d11Resources[res] != nullptr);
        CommittedD3D11Resources[Slot] = pd3d11Resources[res];
    }

    return Slots;
}

template <typename D3D11ResourceViewType>
ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindResourceViews(
    Uint32                 ShaderInd,
    D3D11ResourceViewType* CommittedD3D11Views[],
    ID3D11Resource*        CommittedD3D11Resources[],
    Uint8&                 Binding) const
{
    CachedResourceTraits<D3D11ResourceViewType>::CachedResourceType const* CachedResources;
    D3D11ResourceViewType* const*                                          pd3d11Views;
    GetConstResourceArrays(ShaderInd, CachedResources, pd3d11Views);

    MinMaxSlot Slots;

    const auto ResCount = GetResourceCount<D3D11ResourceViewType>(ShaderInd);
    for (Uint32 res = 0; res < ResCount; ++res)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11Views[Slot] != pd3d11Views[res])
            Slots.Add(Slot);

        VERIFY_EXPR(pd3d11Views[res] != nullptr);
        CommittedD3D11Resources[Slot] = CachedResources[res].pd3d11Resource;
        CommittedD3D11Views[Slot]     = pd3d11Views[res];
    }

    return Slots;
}

} // namespace Diligent
