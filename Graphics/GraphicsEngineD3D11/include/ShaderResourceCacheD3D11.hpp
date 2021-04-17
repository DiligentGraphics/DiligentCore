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
#include <utility>

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

/// The class implements a cache that holds resources bound to all shader stages.
// All resources are stored in the continuous memory using the following layout:
//
//   |         CachedCB         |      ID3D11Buffer*     ||       CachedResource     | ID3D11ShaderResourceView* ||         CachedSampler        |      ID3D11SamplerState*    ||      CachedResource     | ID3D11UnorderedAccessView*||
//   |--------------------------|------------------------||--------------------------|---------------------------||------------------------------|-----------------------------||-------------------------|---------------------------||
//   |  0 | 1 | ... | CBCount-1 | 0 | 1 | ...| CBCount-1 || 0 | 1 | ... | SRVCount-1 | 0 | 1 |  ... | SRVCount-1 || 0 | 1 | ... | SamplerCount-1 | 0 | 1 | ...| SamplerCount-1 ||0 | 1 | ... | UAVCount-1 | 0 | 1 | ...  | UAVCount-1 ||
//    --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
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

    /// Describes a resource associated with a cached constant buffer
    struct CachedCB
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferD3D11Impl> pBuff;

        explicit operator bool() const
        {
            return pBuff;
        }

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

        __forceinline void Set(SamplerD3D11Impl* pSam)
        {
            pSampler = pSam;
        }
    };

    /// Describes a resource associated with a cached SRV or a UAV
    struct CachedResource
    {
        /// We keep strong reference to the view instead of the reference
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

    template <D3D11_RESOURCE_RANGE>
    struct CachedResourceTraits;

    static size_t GetRequiredMemorySize(const D3D11ShaderResourceCounters& ResCount);

    void Initialize(const D3D11ShaderResourceCounters& ResCount, IMemoryAllocator& MemAllocator);

    __forceinline void SetCB(const D3D11ResourceBindPoints& BindPoints, RefCntAutoPtr<BufferD3D11Impl> pBuffD3D11Impl)
    {
        auto* pd3d11Buff = pBuffD3D11Impl ? pBuffD3D11Impl->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_CBV>(BindPoints, std::move(pBuffD3D11Impl), pd3d11Buff);
    }

    __forceinline void SetTexSRV(const D3D11ResourceBindPoints& BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11SRV = pTexView ? static_cast<ID3D11ShaderResourceView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_SRV>(BindPoints, std::move(pTexView), pd3d11SRV);
    }

    __forceinline void SetBufSRV(const D3D11ResourceBindPoints& BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11SRV = pBuffView ? static_cast<ID3D11ShaderResourceView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_SRV>(BindPoints, std::move(pBuffView), pd3d11SRV);
    }

    __forceinline void SetTexUAV(const D3D11ResourceBindPoints& BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11UAV = pTexView ? static_cast<ID3D11UnorderedAccessView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_UAV>(BindPoints, std::move(pTexView), pd3d11UAV);
    }

    __forceinline void SetBufUAV(const D3D11ResourceBindPoints& BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11UAV = pBuffView ? static_cast<ID3D11UnorderedAccessView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_UAV>(BindPoints, std::move(pBuffView), pd3d11UAV);
    }

    __forceinline void SetSampler(const D3D11ResourceBindPoints& BindPoints, SamplerD3D11Impl* pSampler)
    {
        auto* pd3d11Sampler = pSampler ? pSampler->SamplerD3D11Impl::GetD3D11SamplerState() : nullptr;
        SetD3D11ResourceInternal<D3D11_RESOURCE_RANGE_SAMPLER>(BindPoints, pSampler, pd3d11Sampler);
    }


    template <D3D11_RESOURCE_RANGE ResRange>
    __forceinline const typename CachedResourceTraits<ResRange>::CachedResourceType& GetResource(const D3D11ResourceBindPoints& BindPoints) const
    {
        VERIFY(BindPoints.GetActiveStages() != SHADER_TYPE_UNKNOWN, "No active shader stage");
        const auto ShaderInd = GetFirstShaderStageIndex(BindPoints.GetActiveStages());
        const auto Offset    = BindPoints[ShaderInd];
        VERIFY(Offset < GetResourceCount<ResRange>(ShaderInd), "Resource slot is out of range");
        const auto ResArrays = GetConstResourceArrays<ResRange>(ShaderInd);
        return ResArrays.first[BindPoints[ShaderInd]];
    }

    template <D3D11_RESOURCE_RANGE ResRange>
    bool CopyResource(const ShaderResourceCacheD3D11& SrcCache, const D3D11ResourceBindPoints& BindPoints)
    {
        bool IsBound = true;
        for (auto ActiveStages = BindPoints.GetActiveStages(); ActiveStages != SHADER_TYPE_UNKNOWN;)
        {
            const auto ShaderInd = ExtractFirstShaderStageIndex(ActiveStages);

            auto SrcResArrays = SrcCache.GetConstResourceArrays<ResRange>(ShaderInd);
            auto DstResArrays = GetResourceArrays<ResRange>(ShaderInd);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetResourceCount<ResRange>(ShaderInd), "Index is out of range");
            if (!SrcResArrays.first[CacheOffset])
                IsBound = false;

            DstResArrays.first[CacheOffset]  = SrcResArrays.first[CacheOffset];
            DstResArrays.second[CacheOffset] = SrcResArrays.second[CacheOffset];
        }
        VERIFY_EXPR(IsBound == IsResourceBound<ResRange>(BindPoints));
        return IsBound;
    }

    template <D3D11_RESOURCE_RANGE ResRange>
    __forceinline bool IsResourceBound(const D3D11ResourceBindPoints& BindPoints) const
    {
        if (BindPoints.IsEmpty())
            return false;

        auto       ActiveStages   = BindPoints.GetActiveStages();
        const auto FirstShaderInd = ExtractFirstShaderStageIndex(ActiveStages);
        const bool IsBound        = IsResourceBound<ResRange>(FirstShaderInd, BindPoints[FirstShaderInd]);

#ifdef DILIGENT_DEBUG
        while (ActiveStages != SHADER_TYPE_UNKNOWN)
        {
            const Uint32 ShaderInd = ExtractFirstShaderStageIndex(ActiveStages);
            VERIFY_EXPR(IsBound == IsResourceBound<ResRange>(ShaderInd, BindPoints[ShaderInd]));
        }
#endif

        return IsBound;
    }

    // clang-format off
    __forceinline Uint32 GetCBCount     (Uint32 ShaderInd) const { return (m_Offsets[FirstCBOffsetIdx  + ShaderInd + 1] - m_Offsets[FirstCBOffsetIdx  + ShaderInd]) / (sizeof(CachedCB)       + sizeof(ID3D11Buffer*));             }
    __forceinline Uint32 GetSRVCount    (Uint32 ShaderInd) const { return (m_Offsets[FirstSRVOffsetIdx + ShaderInd + 1] - m_Offsets[FirstSRVOffsetIdx + ShaderInd]) / (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*)); }
    __forceinline Uint32 GetSamplerCount(Uint32 ShaderInd) const { return (m_Offsets[FirstSamOffsetIdx + ShaderInd + 1] - m_Offsets[FirstSamOffsetIdx + ShaderInd]) / (sizeof(CachedSampler)  + sizeof(ID3D11SamplerState*));       }
    __forceinline Uint32 GetUAVCount    (Uint32 ShaderInd) const { return (m_Offsets[FirstUAVOffsetIdx + ShaderInd + 1] - m_Offsets[FirstUAVOffsetIdx + ShaderInd]) / (sizeof(CachedResource) + sizeof(ID3D11UnorderedAccessView*));}
    // clang-format on

    template <D3D11_RESOURCE_RANGE>
    __forceinline Uint32 GetResourceCount(Uint32 ShaderInd) const;

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

    template <D3D11_RESOURCE_RANGE Range>
    inline MinMaxSlot BindResources(Uint32                                                   ShaderInd,
                                    typename CachedResourceTraits<Range>::D3D11ResourceType* CommittedD3D11Resources[],
                                    const D3D11ShaderResourceCounters&                       BaseBindings) const;

    template <D3D11_RESOURCE_RANGE Range>
    inline MinMaxSlot BindResourceViews(Uint32                                                   ShaderInd,
                                        typename CachedResourceTraits<Range>::D3D11ResourceType* CommittedD3D11Views[],
                                        ID3D11Resource*                                          CommittedD3D11Resources[],
                                        const D3D11ShaderResourceCounters&                       BaseBindings) const;

    enum class StateTransitionMode
    {
        Transition,
        Verify
    };
    // Transitions all resources in the cache
    template <StateTransitionMode Mode>
    void TransitionResourceStates(DeviceContextD3D11Impl& Ctx);

private:
    template <D3D11_RESOURCE_RANGE>
    __forceinline Uint32 GetResourceDataOffset(Uint32 ShaderInd) const;

    template <D3D11_RESOURCE_RANGE ResRange>
    __forceinline std::pair<typename CachedResourceTraits<ResRange>::CachedResourceType*,
                            typename CachedResourceTraits<ResRange>::D3D11ResourceType**>
    GetResourceArrays(Uint32 ShaderInd) const
    {
        using CachedResourceType = CachedResourceTraits<ResRange>::CachedResourceType;
        using D3D11ResourceType  = CachedResourceTraits<ResRange>::D3D11ResourceType;
        static_assert(alignof(CachedResourceType) == alignof(D3D11ResourceType*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");

        const auto  DataOffset      = GetResourceDataOffset<ResRange>(ShaderInd);
        const auto  ResCount        = GetResourceCount<ResRange>(ShaderInd);
        auto* const pResources      = reinterpret_cast<CachedResourceType*>(m_pResourceData.get() + DataOffset);
        auto* const pd3d11Resources = reinterpret_cast<D3D11ResourceType**>(pResources + ResCount);
        return std::make_pair(pResources, pd3d11Resources);
    }

    template <D3D11_RESOURCE_RANGE ResRange>
    __forceinline std::pair<typename CachedResourceTraits<ResRange>::CachedResourceType const*,
                            typename CachedResourceTraits<ResRange>::D3D11ResourceType* const*>
    GetConstResourceArrays(Uint32 ShaderInd) const
    {
        const auto ResArrays = GetResourceArrays<ResRange>(ShaderInd);
        return std::make_pair(ResArrays.first, ResArrays.second);
    }

    template <D3D11_RESOURCE_RANGE ResRange, typename TSrcResourceType, typename TD3D11ResourceType>
    __forceinline void SetD3D11ResourceInternal(const D3D11ResourceBindPoints& BindPoints, TSrcResourceType pResource, TD3D11ResourceType* pd3d11Resource)
    {
        VERIFY(pResource != nullptr && pd3d11Resource != nullptr || pResource == nullptr && pd3d11Resource == nullptr,
               "Resource and D3D11 resource must be set/unset atomically");
        for (auto ActiveStages = BindPoints.GetActiveStages(); ActiveStages != SHADER_TYPE_UNKNOWN;)
        {
            const Uint32 ShaderInd   = ExtractFirstShaderStageIndex(ActiveStages);
            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetResourceCount<ResRange>(ShaderInd), "Cache offset is out of range");

            auto ResArrays = GetResourceArrays<ResRange>(ShaderInd);
            ResArrays.first[CacheOffset].Set(pResource);
            ResArrays.second[CacheOffset] = pd3d11Resource;
        }
    }

    template <D3D11_RESOURCE_RANGE ResRange>
    __forceinline bool IsResourceBound(Uint32 ShaderInd, Uint32 Offset) const
    {
        const auto ResCount = GetResourceCount<ResRange>(ShaderInd);
        VERIFY(Offset < ResCount, "Offset is out of range");
        const auto ResArrays = GetConstResourceArrays<ResRange>(ShaderInd);
        return Offset < ResCount && ResArrays.first[Offset];
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

    static constexpr size_t MaxAlignment = std::max(std::max(std::max(alignof(CachedCB), alignof(CachedResource)), alignof(CachedSampler)), alignof(IUnknown*));

    static constexpr int NumShaderTypes = D3D11ResourceBindPoints::NumShaderTypes;

    static constexpr Uint32 FirstCBOffsetIdx  = 0;
    static constexpr Uint32 FirstSRVOffsetIdx = FirstCBOffsetIdx + NumShaderTypes;
    static constexpr Uint32 FirstSamOffsetIdx = FirstSRVOffsetIdx + NumShaderTypes;
    static constexpr Uint32 FirstUAVOffsetIdx = FirstSamOffsetIdx + NumShaderTypes;
    static constexpr Uint32 MaxOffsets        = FirstUAVOffsetIdx + NumShaderTypes + 1;

    std::array<OffsetType, MaxOffsets> m_Offsets = {};

    bool m_IsInitialized = false;

    // Indicates what types of resources are stored in the cache
    const ResourceCacheContentType m_ContentType;

    std::unique_ptr<Uint8, STDDeleter<Uint8, IMemoryAllocator>> m_pResourceData;
};

static constexpr size_t ResCacheSize = sizeof(ShaderResourceCacheD3D11);


template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_CBV>
{
    using CachedResourceType = CachedCB;
    using D3D11ResourceType  = ID3D11Buffer;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_SAMPLER>
{
    using CachedResourceType = CachedSampler;
    using D3D11ResourceType  = ID3D11SamplerState;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_SRV>
{
    using CachedResourceType = CachedResource;
    using D3D11ResourceType  = ID3D11ShaderResourceView;
};

template <>
struct ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_UAV>
{
    using CachedResourceType = CachedResource;
    using D3D11ResourceType  = ID3D11UnorderedAccessView;
};


template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<D3D11_RESOURCE_RANGE_CBV>(Uint32 ShaderInd) const
{
    return GetCBCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<D3D11_RESOURCE_RANGE_SRV>(Uint32 ShaderInd) const
{
    return GetSRVCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<D3D11_RESOURCE_RANGE_UAV>(Uint32 ShaderInd) const
{
    return GetUAVCount(ShaderInd);
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceCount<D3D11_RESOURCE_RANGE_SAMPLER>(Uint32 ShaderInd) const
{
    return GetSamplerCount(ShaderInd);
}


template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<D3D11_RESOURCE_RANGE_CBV>(Uint32 ShaderInd) const
{
    return m_Offsets[FirstCBOffsetIdx + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<D3D11_RESOURCE_RANGE_SRV>(Uint32 ShaderInd) const
{
    return m_Offsets[FirstSRVOffsetIdx + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<D3D11_RESOURCE_RANGE_SAMPLER>(Uint32 ShaderInd) const
{
    return m_Offsets[FirstSamOffsetIdx + ShaderInd];
}

template <>
__forceinline Uint32 ShaderResourceCacheD3D11::GetResourceDataOffset<D3D11_RESOURCE_RANGE_UAV>(Uint32 ShaderInd) const
{
    return m_Offsets[FirstUAVOffsetIdx + ShaderInd];
}

// Instantiate templates
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(DeviceContextD3D11Impl& Ctx);
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Verify>(DeviceContextD3D11Impl& Ctx);

template <D3D11_RESOURCE_RANGE Range>
inline ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindResources(
    Uint32                                                   ShaderInd,
    typename CachedResourceTraits<Range>::D3D11ResourceType* CommittedD3D11Resources[],
    const D3D11ShaderResourceCounters&                       BaseBindings) const
{
    const auto   ResCount    = GetResourceCount<Range>(ShaderInd);
    const auto   ResArrays   = GetConstResourceArrays<Range>(ShaderInd);
    const Uint32 BaseBinding = BaseBindings[Range][ShaderInd];

    MinMaxSlot Slots;
    for (Uint32 res = 0; res < ResCount; ++res)
    {
        const Uint32 Slot = BaseBinding + res;
        if (CommittedD3D11Resources[Slot] != ResArrays.second[res])
            Slots.Add(Slot);

        VERIFY_EXPR(ResArrays.second[res] != nullptr);
        CommittedD3D11Resources[Slot] = ResArrays.second[res];
    }

    return Slots;
}


template <D3D11_RESOURCE_RANGE Range>
inline ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindResourceViews(
    Uint32                                                   ShaderInd,
    typename CachedResourceTraits<Range>::D3D11ResourceType* CommittedD3D11Views[],
    ID3D11Resource*                                          CommittedD3D11Resources[],
    const D3D11ShaderResourceCounters&                       BaseBindings) const
{
    const auto   ResCount    = GetResourceCount<Range>(ShaderInd);
    const auto   ResArrays   = GetConstResourceArrays<Range>(ShaderInd);
    const Uint32 BaseBinding = BaseBindings[Range][ShaderInd];

    MinMaxSlot Slots;
    for (Uint32 res = 0; res < ResCount; ++res)
    {
        const Uint32 Slot = BaseBinding + res;
        if (CommittedD3D11Views[Slot] != ResArrays.second[res])
            Slots.Add(Slot);

        VERIFY_EXPR(ResArrays.second[res] != nullptr);
        CommittedD3D11Resources[Slot] = ResArrays.first[res].pd3d11Resource;
        CommittedD3D11Views[Slot]     = ResArrays.second[res];
    }

    return Slots;
}

} // namespace Diligent
