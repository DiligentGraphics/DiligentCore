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

    static constexpr int NumShaderTypes = BindPointsD3D11::NumShaderTypes;
    using TBindingsPerStage             = std::array<std::array<Uint8, NumShaderTypes>, D3D11_RESOURCE_RANGE_COUNT>;

    static size_t GetRequriedMemorySize(const TBindingsPerStage& ResCount);

    void Initialize(const TBindingsPerStage& ResCount, IMemoryAllocator& MemAllocator);

    __forceinline void SetCB(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferD3D11Impl> pBuffD3D11Impl)
    {
        auto* pd3d11Buff = pBuffD3D11Impl ? pBuffD3D11Impl->BufferD3D11Impl::GetD3D11Buffer() : nullptr;
        SetD3D11ResourceInternal<CachedCB>(BindPoints, &ShaderResourceCacheD3D11::GetCBCount, &ShaderResourceCacheD3D11::GetCBArrays, std::move(pBuffD3D11Impl), pd3d11Buff);
    }

    __forceinline void SetTexSRV(BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11SRV = pTexView ? static_cast<ID3D11ShaderResourceView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, &ShaderResourceCacheD3D11::GetSRVCount, &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pTexView), pd3d11SRV);
    }

    __forceinline void SetBufSRV(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11SRV = pBuffView ? static_cast<ID3D11ShaderResourceView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, &ShaderResourceCacheD3D11::GetSRVCount, &ShaderResourceCacheD3D11::GetSRVArrays, std::move(pBuffView), pd3d11SRV);
    }

    __forceinline void SetTexUAV(BindPointsD3D11 BindPoints, RefCntAutoPtr<TextureViewD3D11Impl> pTexView)
    {
        auto* pd3d11UAV = pTexView ? static_cast<ID3D11UnorderedAccessView*>(pTexView->TextureViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, &ShaderResourceCacheD3D11::GetUAVCount, &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pTexView), pd3d11UAV);
    }

    __forceinline void SetBufUAV(BindPointsD3D11 BindPoints, RefCntAutoPtr<BufferViewD3D11Impl> pBuffView)
    {
        auto* pd3d11UAV = pBuffView ? static_cast<ID3D11UnorderedAccessView*>(pBuffView->BufferViewD3D11Impl::GetD3D11View()) : nullptr;
        SetD3D11ResourceInternal<CachedResource>(BindPoints, &ShaderResourceCacheD3D11::GetUAVCount, &ShaderResourceCacheD3D11::GetUAVArrays, std::move(pBuffView), pd3d11UAV);
    }

    __forceinline void SetSampler(BindPointsD3D11 BindPoints, SamplerD3D11Impl* pSampler)
    {
        auto* pd3d11Sampler = pSampler ? pSampler->SamplerD3D11Impl::GetD3D11SamplerState() : nullptr;
        SetD3D11ResourceInternal<CachedSampler>(BindPoints, &ShaderResourceCacheD3D11::GetSamplerCount, &ShaderResourceCacheD3D11::GetSamplerArrays, pSampler, pd3d11Sampler);
    }


    __forceinline CachedCB const& GetCB(BindPointsD3D11 BindPoints) const
    {
        const Uint32 ShaderInd = PlatformMisc::GetLSB(BindPoints.GetActiveBits());
        VERIFY(BindPoints[ShaderInd] < GetCBCount(ShaderInd), "CB slot is out of range");
        ShaderResourceCacheD3D11::CachedCB const* CBs;
        ID3D11Buffer* const*                      pd3d11CBs;
        GetConstCBArrays(ShaderInd, CBs, pd3d11CBs);
        return CBs[BindPoints[ShaderInd]];
    }

    __forceinline CachedResource const& GetSRV(BindPointsD3D11 BindPoints) const
    {
        const Uint32 ShaderInd = PlatformMisc::GetLSB(BindPoints.GetActiveBits());
        VERIFY(BindPoints[ShaderInd] < GetSRVCount(ShaderInd), "SRV slot is out of range");
        ShaderResourceCacheD3D11::CachedResource const* SRVResources;
        ID3D11ShaderResourceView* const*                pd3d11SRVs;
        GetConstSRVArrays(ShaderInd, SRVResources, pd3d11SRVs);
        return SRVResources[BindPoints[ShaderInd]];
    }

    __forceinline CachedResource const& GetUAV(BindPointsD3D11 BindPoints) const
    {
        const Uint32 ShaderInd = PlatformMisc::GetLSB(BindPoints.GetActiveBits());
        VERIFY(BindPoints[ShaderInd] < GetUAVCount(ShaderInd), "UAV slot is out of range");
        ShaderResourceCacheD3D11::CachedResource const* UAVResources;
        ID3D11UnorderedAccessView* const*               pd3d11UAVs;
        GetConstUAVArrays(ShaderInd, UAVResources, pd3d11UAVs);
        return UAVResources[BindPoints[ShaderInd]];
    }

    __forceinline CachedSampler const& GetSampler(BindPointsD3D11 BindPoints) const
    {
        const Uint32 ShaderInd = PlatformMisc::GetLSB(BindPoints.GetActiveBits());
        VERIFY(BindPoints[ShaderInd] < GetSamplerCount(ShaderInd), "Sampler slot is out of range");
        ShaderResourceCacheD3D11::CachedSampler const* Samplers;
        ID3D11SamplerState* const*                     pd3d11Samplers;
        GetConstSamplerArrays(ShaderInd, Samplers, pd3d11Samplers);
        return Samplers[BindPoints[ShaderInd]];
    }


    __forceinline bool CopyCB(const ShaderResourceCacheD3D11& SrcCache, BindPointsD3D11 BindPoints)
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            CachedCB const*      pSrcCBs;
            ID3D11Buffer* const* pSrcd3d11CBs;
            SrcCache.GetConstCBArrays(ShaderInd, pSrcCBs, pSrcd3d11CBs);

            CachedCB*      pCBs;
            ID3D11Buffer** pd3d11CBs;
            GetCBArrays(ShaderInd, pCBs, pd3d11CBs);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetCBCount(ShaderInd), "Index is out of range");
            if (pSrcCBs[CacheOffset].pBuff == nullptr)
                IsBound = false;

            pCBs[CacheOffset]      = pSrcCBs[CacheOffset];
            pd3d11CBs[CacheOffset] = pSrcd3d11CBs[CacheOffset];
        }
        return IsBound;
    }

    __forceinline bool CopySRV(const ShaderResourceCacheD3D11& SrcCache, BindPointsD3D11 BindPoints)
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            CachedResource const*            pSrcSRVResources;
            ID3D11ShaderResourceView* const* pSrcd3d11SRVs;
            SrcCache.GetConstSRVArrays(ShaderInd, pSrcSRVResources, pSrcd3d11SRVs);

            CachedResource*            pSRVResources;
            ID3D11ShaderResourceView** pd3d11SRVs;
            GetSRVArrays(ShaderInd, pSRVResources, pd3d11SRVs);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetSRVCount(ShaderInd), "Index is out of range");
            if (pSrcSRVResources[CacheOffset].pBuffer == nullptr && pSrcSRVResources[CacheOffset].pTexture == nullptr)
                IsBound = false;

            pSRVResources[CacheOffset] = pSrcSRVResources[CacheOffset];
            pd3d11SRVs[CacheOffset]    = pSrcd3d11SRVs[CacheOffset];
        }
        return IsBound;
    }

    __forceinline bool CopyUAV(const ShaderResourceCacheD3D11& SrcCache, BindPointsD3D11 BindPoints)
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            CachedResource const*             pSrcUAVResources;
            ID3D11UnorderedAccessView* const* pSrcd3d11UAVs;
            SrcCache.GetConstUAVArrays(ShaderInd, pSrcUAVResources, pSrcd3d11UAVs);

            CachedResource*             pUAVResources;
            ID3D11UnorderedAccessView** pd3d11UAVs;
            GetUAVArrays(ShaderInd, pUAVResources, pd3d11UAVs);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetUAVCount(ShaderInd), "Index is out of range");
            if (pSrcUAVResources[CacheOffset].pBuffer == nullptr && pSrcUAVResources[CacheOffset].pTexture == nullptr)
                IsBound = false;

            pUAVResources[CacheOffset] = pSrcUAVResources[CacheOffset];
            pd3d11UAVs[CacheOffset]    = pSrcd3d11UAVs[CacheOffset];
        }
        return IsBound;
    }

    __forceinline bool CopySampler(const ShaderResourceCacheD3D11& SrcCache, BindPointsD3D11 BindPoints)
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            CachedSampler const*       pSrcSamplers;
            ID3D11SamplerState* const* pSrcd3d11Samplers;
            SrcCache.GetConstSamplerArrays(ShaderInd, pSrcSamplers, pSrcd3d11Samplers);

            CachedSampler*       pSamplers;
            ID3D11SamplerState** pd3d11Samplers;
            GetSamplerArrays(ShaderInd, pSamplers, pd3d11Samplers);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            VERIFY(CacheOffset < GetSamplerCount(ShaderInd), "Index is out of range");
            if (pSrcSamplers[CacheOffset].pSampler == nullptr)
                IsBound = false;

            pSamplers[CacheOffset]      = pSrcSamplers[CacheOffset];
            pd3d11Samplers[CacheOffset] = pSrcd3d11Samplers[CacheOffset];
        }
        return IsBound;
    }


    __forceinline bool IsCBBound(BindPointsD3D11 BindPoints) const
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);
            const Uint32 CacheOffset = BindPoints[ShaderInd];

            CachedCB const*      CBs;
            ID3D11Buffer* const* d3d11CBs;
            GetConstCBArrays(ShaderInd, CBs, d3d11CBs);
            if (CacheOffset < GetCBCount(ShaderInd) && d3d11CBs[CacheOffset] != nullptr)
            {
                VERIFY(CBs[CacheOffset].pBuff != nullptr, "No relevant buffer resource");
                continue;
            }
            IsBound = false;
        }
        return IsBound;
    }

    __forceinline bool IsSRVBound(BindPointsD3D11 BindPoints, bool dbgIsTextureView) const
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);
            const Uint32 CacheOffset = BindPoints[ShaderInd];

            CachedResource const*            SRVResources;
            ID3D11ShaderResourceView* const* d3d11SRVs;
            GetConstSRVArrays(ShaderInd, SRVResources, d3d11SRVs);
            if (CacheOffset < GetSRVCount(ShaderInd) && d3d11SRVs[CacheOffset] != nullptr)
            {
                VERIFY((dbgIsTextureView && SRVResources[CacheOffset].pTexture != nullptr) || (!dbgIsTextureView && SRVResources[CacheOffset].pBuffer != nullptr),
                       "No relevant resource");
                continue;
            }
            IsBound = false;
        }
        return IsBound;
    }

    __forceinline bool IsUAVBound(BindPointsD3D11 BindPoints, bool dbgIsTextureView) const
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);
            const Uint32 CacheOffset = BindPoints[ShaderInd];

            CachedResource const*             UAVResources;
            ID3D11UnorderedAccessView* const* d3d11UAVs;
            GetConstUAVArrays(ShaderInd, UAVResources, d3d11UAVs);
            if (CacheOffset < GetUAVCount(ShaderInd) && d3d11UAVs[CacheOffset] != nullptr)
            {
                VERIFY((dbgIsTextureView && UAVResources[CacheOffset].pTexture != nullptr) || (!dbgIsTextureView && UAVResources[CacheOffset].pBuffer != nullptr),
                       "No relevant resource");
                continue;
            }
            IsBound = false;
        }
        return IsBound;
    }

    __forceinline bool IsSamplerBound(BindPointsD3D11 BindPoints) const
    {
        bool IsBound = true;
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);
            const Uint32 CacheOffset = BindPoints[ShaderInd];

            CachedSampler const*       Samplers;
            ID3D11SamplerState* const* d3d11Samplers;
            GetConstSamplerArrays(ShaderInd, Samplers, d3d11Samplers);
            if (CacheOffset < GetSamplerCount(ShaderInd) && d3d11Samplers[CacheOffset] != nullptr)
            {
                VERIFY(Samplers[CacheOffset].pSampler != nullptr, "No relevant sampler");
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

    __forceinline void GetCBArrays(Uint32 ShaderInd, CachedCB*& CBs, ID3D11Buffer**& pd3d11CBs) const
    {
        VERIFY(alignof(CachedCB) == alignof(ID3D11Buffer*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        CBs       = reinterpret_cast<CachedCB*>(m_pResourceData.get() + m_Offsets[CBOffset + ShaderInd]);
        pd3d11CBs = reinterpret_cast<ID3D11Buffer**>(CBs + GetCBCount(ShaderInd));
    }

    __forceinline void GetSRVArrays(Uint32 ShaderInd, CachedResource*& SRVResources, ID3D11ShaderResourceView**& d3d11SRVs) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11ShaderResourceView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        SRVResources = reinterpret_cast<CachedResource*>(m_pResourceData.get() + m_Offsets[SRVOffset + ShaderInd]);
        d3d11SRVs    = reinterpret_cast<ID3D11ShaderResourceView**>(SRVResources + GetSRVCount(ShaderInd));
    }

    __forceinline void GetSamplerArrays(Uint32 ShaderInd, CachedSampler*& Samplers, ID3D11SamplerState**& pd3d11Samplers) const
    {
        VERIFY(alignof(CachedSampler) == alignof(ID3D11SamplerState*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        Samplers       = reinterpret_cast<CachedSampler*>(m_pResourceData.get() + m_Offsets[SampOffset + ShaderInd]);
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState**>(Samplers + GetSamplerCount(ShaderInd));
    }

    __forceinline void GetUAVArrays(Uint32 ShaderInd, CachedResource*& UAVResources, ID3D11UnorderedAccessView**& pd3d11UAVs) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11UnorderedAccessView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        UAVResources = reinterpret_cast<CachedResource*>(m_pResourceData.get() + m_Offsets[UAVOffset + ShaderInd]);
        pd3d11UAVs   = reinterpret_cast<ID3D11UnorderedAccessView**>(UAVResources + GetUAVCount(ShaderInd));
    }

    __forceinline void GetConstCBArrays(Uint32 ShaderInd, CachedCB const*& CBs, ID3D11Buffer* const*& pd3d11CBs) const
    {
        VERIFY(alignof(CachedCB) == alignof(ID3D11Buffer*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        CBs       = reinterpret_cast<CachedCB const*>(m_pResourceData.get() + m_Offsets[CBOffset + ShaderInd]);
        pd3d11CBs = reinterpret_cast<ID3D11Buffer* const*>(CBs + GetCBCount(ShaderInd));
    }

    __forceinline void GetConstSRVArrays(Uint32 ShaderInd, CachedResource const*& SRVResources, ID3D11ShaderResourceView* const*& d3d11SRVs) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11ShaderResourceView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        SRVResources = reinterpret_cast<CachedResource const*>(m_pResourceData.get() + m_Offsets[SRVOffset + ShaderInd]);
        d3d11SRVs    = reinterpret_cast<ID3D11ShaderResourceView* const*>(SRVResources + GetSRVCount(ShaderInd));
    }

    __forceinline void GetConstSamplerArrays(Uint32 ShaderInd, CachedSampler const*& Samplers, ID3D11SamplerState* const*& pd3d11Samplers) const
    {
        VERIFY(alignof(CachedSampler) == alignof(ID3D11SamplerState*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        Samplers       = reinterpret_cast<CachedSampler const*>(m_pResourceData.get() + m_Offsets[SampOffset + ShaderInd]);
        pd3d11Samplers = reinterpret_cast<ID3D11SamplerState* const*>(Samplers + GetSamplerCount(ShaderInd));
    }

    __forceinline void GetConstUAVArrays(Uint32 ShaderInd, CachedResource const*& UAVResources, ID3D11UnorderedAccessView* const*& pd3d11UAVs) const
    {
        VERIFY(alignof(CachedResource) == alignof(ID3D11UnorderedAccessView*), "Alignment mismatch, pointer to D3D11 resource may not be properly aligned");
        UAVResources = reinterpret_cast<CachedResource const*>(m_pResourceData.get() + m_Offsets[UAVOffset + ShaderInd]);
        pd3d11UAVs   = reinterpret_cast<ID3D11UnorderedAccessView* const*>(UAVResources + GetUAVCount(ShaderInd));
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

    MinMaxSlot BindCBs(Uint32        ShaderInd,
                       ID3D11Buffer* CommittedD3D11CBs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT],
                       Uint8&        Binding) const;

    MinMaxSlot BindSRVs(Uint32                    ShaderInd,
                        ID3D11ShaderResourceView* CommittedD3D11SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
                        ID3D11Resource*           CommittedD3D11SRVResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
                        Uint8&                    Binding) const;

    MinMaxSlot BindSamplers(Uint32              ShaderInd,
                            ID3D11SamplerState* CommittedD3D11Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT],
                            Uint8&              Binding) const;

    MinMaxSlot BindUAVs(Uint32                     ShaderInd,
                        ID3D11UnorderedAccessView* CommittedD3D11UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT],
                        ID3D11Resource*            CommittedD3D11UAVResources[D3D11_PS_CS_UAV_REGISTER_COUNT],
                        Uint8&                     Binding) const;

private:
    template <typename TCachedResourceType, typename TGetResourceCount, typename TGetResourceArraysFunc, typename TSrcResourceType, typename TD3D11ResourceType>
    __forceinline void SetD3D11ResourceInternal(BindPointsD3D11 BindPoints, TGetResourceCount GetCount, TGetResourceArraysFunc GetArrays, TSrcResourceType pResource, TD3D11ResourceType* pd3d11Resource)
    {
        VERIFY(pResource != nullptr && pd3d11Resource != nullptr || pResource == nullptr && pd3d11Resource == nullptr,
               "Resource and D3D11 resource must be set/unset atomically");
        for (Uint32 ActiveBits = BindPoints.GetActiveBits(); ActiveBits != 0;)
        {
            const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
            ActiveBits &= ~(1u << ShaderInd);

            const Uint32 CacheOffset = BindPoints[ShaderInd];
            const Uint32 ResCount    = (this->*GetCount)(ShaderInd);
            VERIFY(CacheOffset < ResCount, "Index is out of range");

            TCachedResourceType* Resources;
            TD3D11ResourceType** d3d11ResArr;
            (this->*GetArrays)(ShaderInd, Resources, d3d11ResArr);
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

// Instantiate templates
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(DeviceContextD3D11Impl& Ctx);
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Verify>(DeviceContextD3D11Impl& Ctx);

} // namespace Diligent
