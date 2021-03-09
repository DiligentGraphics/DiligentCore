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

#include "pch.h"

#include "ShaderResourceCacheD3D11.hpp"
#include "TextureBaseD3D11.hpp"
#include "BufferD3D11Impl.hpp"
#include "SamplerD3D11Impl.hpp"
#include "DeviceContextD3D11Impl.hpp"
#include "MemoryAllocator.h"
#include "Align.hpp"

namespace Diligent
{
size_t ShaderResourceCacheD3D11::GetRequriedMemorySize(const TResourceCount& ResCount)
{
    // clang-format off
    auto   CBCount      = ResCount[DESCRIPTOR_RANGE_CBV];
    auto   SRVCount     = ResCount[DESCRIPTOR_RANGE_SRV];
    auto   SamplerCount = ResCount[DESCRIPTOR_RANGE_SAMPLER];
    auto   UAVCount     = ResCount[DESCRIPTOR_RANGE_UAV];
    size_t MemSize      = 0;
    MemSize = Align(MemSize + (sizeof(CachedCB)       + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11Buffer*))              * CBCount,      MaxAlignment);
    MemSize = Align(MemSize + (sizeof(CachedResource) + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11ShaderResourceView*))  * SRVCount,     MaxAlignment);
    MemSize = Align(MemSize + (sizeof(CachedSampler)  + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11SamplerState*))        * SamplerCount, MaxAlignment);
    MemSize = Align(MemSize + (sizeof(CachedResource) + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11UnorderedAccessView*)) * UAVCount,     MaxAlignment);
    // clang-format on
    VERIFY(MemSize < InvalidResourceOffset, "Memory size exeed the maximum allowed size.");
    return MemSize;
}

void ShaderResourceCacheD3D11::Initialize(const TResourceCount& ResCount, IMemoryAllocator& MemAllocator)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
    if (IsInitialized())
    {
        LOG_ERROR_MESSAGE("Resource cache is already intialized");
        return;
    }

    const Uint32 CBCount      = ResCount[DESCRIPTOR_RANGE_CBV];
    const Uint32 SRVCount     = ResCount[DESCRIPTOR_RANGE_SRV];
    const Uint32 SamplerCount = ResCount[DESCRIPTOR_RANGE_SAMPLER];
    const Uint32 UAVCount     = ResCount[DESCRIPTOR_RANGE_UAV];

    // clang-format off
    m_CBCount      = static_cast<decltype(m_CBCount     )>(CBCount);
    m_SRVCount     = static_cast<decltype(m_SRVCount    )>(SRVCount);
    m_SamplerCount = static_cast<decltype(m_SamplerCount)>(SamplerCount);
    m_UAVCount     = static_cast<decltype(m_UAVCount    )>(UAVCount);

    VERIFY(CBCount      == m_CBCount,      "Constant buffer count (", CBCount, ") exceeds maximum representable value");
    VERIFY(SRVCount     == m_SRVCount,     "Shader resources count (", SRVCount, ") exceeds maximum representable value");
    VERIFY(SamplerCount == m_SamplerCount, "Sampler count (", SamplerCount, ") exceeds maximum representable value");
    VERIFY(UAVCount     == m_UAVCount,     "UAVs count (", UAVCount, ") exceeds maximum representable value");

    // m_CBOffset  = 0
    m_SRVOffset       = static_cast<OffsetType>(Align(m_CBOffset      + (sizeof(CachedCB)       + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11Buffer*))              * CBCount,      MaxAlignment));
    m_SamplerOffset   = static_cast<OffsetType>(Align(m_SRVOffset     + (sizeof(CachedResource) + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11ShaderResourceView*))  * SRVCount,     MaxAlignment));
    m_UAVOffset       = static_cast<OffsetType>(Align(m_SamplerOffset + (sizeof(CachedSampler)  + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11SamplerState*))        * SamplerCount, MaxAlignment));
    size_t BufferSize = static_cast<OffsetType>(Align(m_UAVOffset     + (sizeof(CachedResource) + sizeof(TBindPointsAndActiveBits) + sizeof(ID3D11UnorderedAccessView*)) * UAVCount,     MaxAlignment));
    // clang-format on

    VERIFY_EXPR(m_pResourceData == nullptr);
    VERIFY_EXPR(BufferSize == GetRequriedMemorySize(ResCount));

    if (BufferSize > 0)
    {
        m_pResourceData = ALLOCATE(MemAllocator, "Shader resource cache data buffer", Uint8, BufferSize);
        memset(m_pResourceData, 0, BufferSize);
        m_pAllocator = &MemAllocator;
    }

    const TBindPointsAndActiveBits InvalidBindPoints = {0, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint};

    // Explicitly construct all objects
    if (CBCount != 0)
    {
        CachedCB*                 CBs        = nullptr;
        ID3D11Buffer**            d3d11CBs   = nullptr;
        TBindPointsAndActiveBits* bindPoints = nullptr;
        GetCBArrays(CBs, d3d11CBs, bindPoints);
        for (Uint32 cb = 0; cb < CBCount; ++cb)
        {
            new (CBs + cb) CachedCB{};
            bindPoints[cb] = InvalidBindPoints;
        }
    }

    if (SRVCount != 0)
    {
        CachedResource*            SRVResources = nullptr;
        ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
        TBindPointsAndActiveBits*  bindPoints   = nullptr;
        GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);
        for (Uint32 srv = 0; srv < SRVCount; ++srv)
        {
            new (SRVResources + srv) CachedResource{};
            bindPoints[srv] = InvalidBindPoints;
        }
    }

    if (SamplerCount != 0)
    {
        CachedSampler*            Samplers      = nullptr;
        ID3D11SamplerState**      d3d11Samplers = nullptr;
        TBindPointsAndActiveBits* bindPoints    = nullptr;
        GetSamplerArrays(Samplers, d3d11Samplers, bindPoints);
        for (Uint32 sam = 0; sam < SamplerCount; ++sam)
        {
            new (Samplers + sam) CachedSampler{};
            bindPoints[sam] = InvalidBindPoints;
        }
    }

    if (UAVCount != 0)
    {
        CachedResource*             UAVResources = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
        TBindPointsAndActiveBits*   bindPoints   = nullptr;
        GetUAVArrays(UAVResources, d3d11UAVs, bindPoints);
        for (Uint32 uav = 0; uav < UAVCount; ++uav)
        {
            new (UAVResources + uav) CachedResource{};
            bindPoints[uav] = InvalidBindPoints;
        }
    }
}

ShaderResourceCacheD3D11::~ShaderResourceCacheD3D11()
{
    if (IsInitialized())
    {
        // Explicitly destory all objects
        auto CBCount = GetCBCount();
        if (CBCount != 0)
        {
            CachedCB*                 CBs        = nullptr;
            ID3D11Buffer**            d3d11CBs   = nullptr;
            TBindPointsAndActiveBits* bindPoints = nullptr;
            GetCBArrays(CBs, d3d11CBs, bindPoints);
            for (size_t cb = 0; cb < CBCount; ++cb)
                CBs[cb].~CachedCB();
        }

        auto SRVCount = GetSRVCount();
        if (SRVCount != 0)
        {
            CachedResource*            SRVResources = nullptr;
            ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
            TBindPointsAndActiveBits*  bindPoints   = nullptr;
            GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);
            for (size_t srv = 0; srv < SRVCount; ++srv)
                SRVResources[srv].~CachedResource();
        }

        auto SamplerCount = GetSamplerCount();
        if (SamplerCount != 0)
        {
            CachedSampler*            Samplers      = nullptr;
            ID3D11SamplerState**      d3d11Samplers = nullptr;
            TBindPointsAndActiveBits* bindPoints    = nullptr;
            GetSamplerArrays(Samplers, d3d11Samplers, bindPoints);
            for (size_t sam = 0; sam < SamplerCount; ++sam)
                Samplers[sam].~CachedSampler();
        }

        auto UAVCount = GetUAVCount();
        if (UAVCount != 0)
        {
            CachedResource*             UAVResources = nullptr;
            ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
            TBindPointsAndActiveBits*   bindPoints   = nullptr;
            GetUAVArrays(UAVResources, d3d11UAVs, bindPoints);
            for (size_t uav = 0; uav < UAVCount; ++uav)
                UAVResources[uav].~CachedResource();
        }

        m_SRVOffset     = InvalidResourceOffset;
        m_SamplerOffset = InvalidResourceOffset;
        m_UAVOffset     = InvalidResourceOffset;
        m_CBCount       = 0;
        m_SRVCount      = 0;
        m_SamplerCount  = 0;
        m_UAVCount      = 0;

        if (m_pResourceData != nullptr)
            m_pAllocator->Free(m_pResourceData);

        m_pResourceData = nullptr;
        m_pAllocator    = nullptr;
    }
}

#ifdef DILIGENT_DEVELOPMENT
namespace
{
void DvpVerifyResource(ShaderResourceCacheD3D11::CachedResource& Res, ID3D11View* pd3d11View, const char* ViewType)
{
    if (pd3d11View != nullptr)
    {
        VERIFY(Res.pView != nullptr, "Resource view is not initialized");
        VERIFY(Res.pBuffer == nullptr && Res.pTexture != nullptr || Res.pBuffer != nullptr && Res.pTexture == nullptr,
               "Texture and buffer resources are mutually exclusive");
        VERIFY(Res.pd3d11Resource != nullptr, "D3D11 resource is missing");

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
                if (pBufView)
                    VERIFY(pBufView->GetBuffer() == Res.pBuffer, "Provided resource view is not a view of the buffer");
            }
        }
        else if (Res.pTexture)
        {
            VERIFY(pd3d11ActualResource == Res.pTexture->GetD3D11Texture(), "Inconsistent texture ", ViewType);
            if (Res.pView)
            {
                RefCntAutoPtr<ITextureViewD3D11> pTexView(Res.pView, IID_TextureViewD3D11);
                VERIFY(pTexView != nullptr, "Provided resource view is not D3D11 texture view");
                if (pTexView)
                    VERIFY(pTexView->GetTexture() == Res.pTexture, "Provided resource view is not a view of the texture");
            }
        }
    }
    else
    {
        VERIFY(Res.pView == nullptr, "Resource view is unexpected");
        VERIFY(Res.pBuffer == nullptr && Res.pTexture == nullptr, "Niether texture nor buffer resource is expected");
        VERIFY(Res.pd3d11Resource == nullptr, "Unexepected D3D11 resource");
    }
}
} // namespace

void ShaderResourceCacheD3D11::DvpVerifyCacheConsistency()
{
    VERIFY(IsInitialized(), "Cache is not initialized");

    CachedCB*                   CBs           = nullptr;
    ID3D11Buffer**              d3d11CBs      = nullptr;
    CachedResource*             SRVResources  = nullptr;
    ID3D11ShaderResourceView**  d3d11SRVs     = nullptr;
    CachedSampler*              Samplers      = nullptr;
    ID3D11SamplerState**        d3d11Samplers = nullptr;
    CachedResource*             UAVResources  = nullptr;
    ID3D11UnorderedAccessView** d3d11UAVs     = nullptr;
    TBindPointsAndActiveBits*   bindPoints    = nullptr;

    GetCBArrays(CBs, d3d11CBs, bindPoints);
    GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);
    GetSamplerArrays(Samplers, d3d11Samplers, bindPoints);
    GetUAVArrays(UAVResources, d3d11UAVs, bindPoints);

    auto CBCount = GetCBCount();
    for (size_t cb = 0; cb < CBCount; ++cb)
    {
        auto& pBuff      = CBs[cb].pBuff;
        auto* pd3d11Buff = d3d11CBs[cb];
        VERIFY(pBuff == nullptr && pd3d11Buff == nullptr || pBuff != nullptr && pd3d11Buff != nullptr, "CB resource and d3d11 buffer must be set/unset atomically");
        if (pBuff != nullptr && pd3d11Buff != nullptr)
        {
            VERIFY(pd3d11Buff == pBuff->GetD3D11Buffer(), "Inconsistent D3D11 buffer");
        }
    }

    auto SRVCount = GetSRVCount();
    for (size_t srv = 0; srv < SRVCount; ++srv)
    {
        auto& Res       = SRVResources[srv];
        auto* pd3d11SRV = d3d11SRVs[srv];
        DvpVerifyResource(Res, pd3d11SRV, "SRV");
    }

    auto UAVCount = GetUAVCount();
    for (size_t uav = 0; uav < UAVCount; ++uav)
    {
        auto& Res       = UAVResources[uav];
        auto* pd3d11UAV = d3d11UAVs[uav];
        DvpVerifyResource(Res, pd3d11UAV, "UAV");
    }

    auto SamplerCount = GetSamplerCount();
    for (size_t sam = 0; sam < SamplerCount; ++sam)
    {
        auto& pSampler      = Samplers[sam].pSampler;
        auto* pd3d11Sampler = d3d11Samplers[sam];
        VERIFY(pSampler == nullptr && pd3d11Sampler == nullptr || pSampler != nullptr && pd3d11Sampler != nullptr, "CB resource and d3d11 buffer must be set/unset atomically");
        if (pSampler != nullptr && pd3d11Sampler != nullptr)
        {
            VERIFY(pd3d11Sampler == pSampler->GetD3D11SamplerState(), "Inconsistent D3D11 sampler");
        }
    }
}
#endif

template <typename THandleResource>
void ShaderResourceCacheD3D11::ProcessResources(THandleResource HandleResources)
{
    {
        ShaderResourceCacheD3D11::CachedCB* CachedCBs;
        ID3D11Buffer**                      d3d11CBs;
        TBindPointsAndActiveBits*           bindPoints;
        GetCBArrays(CachedCBs, d3d11CBs, bindPoints);
        HandleResources(GetCBCount(), CachedCBs, d3d11CBs);
    }
    {
        ShaderResourceCacheD3D11::CachedResource* CachedSRVResources;
        ID3D11ShaderResourceView**                d3d11SRVs;
        TBindPointsAndActiveBits*                 bindPoints;
        GetSRVArrays(CachedSRVResources, d3d11SRVs, bindPoints);
        HandleResources(GetSRVCount(), CachedSRVResources, d3d11SRVs);
    }
    {
        ShaderResourceCacheD3D11::CachedSampler* CachedSamplers;
        ID3D11SamplerState**                     d3d11Samplers;
        TBindPointsAndActiveBits*                bindPoints;
        GetSamplerArrays(CachedSamplers, d3d11Samplers, bindPoints);
        HandleResources(GetSamplerCount(), CachedSamplers, d3d11Samplers);
    }
    {
        ShaderResourceCacheD3D11::CachedResource* CachedUAVResources;
        ID3D11UnorderedAccessView**               d3d11UAVs;
        TBindPointsAndActiveBits*                 bindPoints;
        GetUAVArrays(CachedUAVResources, d3d11UAVs, bindPoints);
        HandleResources(GetUAVCount(), CachedUAVResources, d3d11UAVs);
    }
}

void ShaderResourceCacheD3D11::TransitionResourceStates(DeviceContextD3D11Impl& Ctx, StateTransitionMode Mode)
{
    VERIFY_EXPR(IsInitialized());

    switch (Mode)
    {
        case StateTransitionMode::Transition:
        {
            ProcessResources([&](Uint32 Count, auto* Resources, auto* Views) { TransitionResource(Ctx, Count, Resources, Views); });
            break;
        }
        case StateTransitionMode::Verify:
        {
#ifdef DILIGENT_DEVELOPMENT
            ProcessResources([&](Uint32 Count, auto* Resources, auto* Views) { DvpVerifyResourceState(Ctx, Count, Resources, Views); });
#endif
            break;
        }
        default:
            UNEXPECTED("Unexpected mode");
    }
}

void ShaderResourceCacheD3D11::TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedCB* CBs, ID3D11Buffer** pd3d11CBs)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        if (auto* pBuffer = CBs[i].pBuff.RawPtr<BufferD3D11Impl>())
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
            { /*
                if (pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    // Even though we have unbound resources from UAV, we only checked shader
                    // stages active in current PSO, so we still may need to unbind the resource
                    // from UAV (for instance, unbind resource from CS UAV when running draw command).
                    Ctx.UnbindResourceFromUAV(pBuffer, pd3d11CBs[i]);
                    pBuffer->ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
                }
                pBuffer->AddState(RESOURCE_STATE_CONSTANT_BUFFER);*/
                Ctx.TransitionResource(pBuffer, RESOURCE_STATE_CONSTANT_BUFFER);
            }
        }
    }
}

void ShaderResourceCacheD3D11::TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedResource* SRVResources, ID3D11ShaderResourceView** d3d11SRVs)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        auto& SRVRes = SRVResources[i];
        if (auto* pTexture = SRVRes.pTexture)
        {
            if (pTexture->IsInKnownState() && !pTexture->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
            { /*
                if (pTexture->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    // Even though we have unbound resources from UAV, we only checked shader
                    // stages active in current PSO, so we still may need to unbind the resource
                    // from UAV (for instance, unbind resource from CS UAV when running draw command).
                    Ctx.UnbindResourceFromUAV(pTexture, SRVRes.pd3d11Resource);
                    pTexture->ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
                }

                if (pTexture->CheckState(RESOURCE_STATE_RENDER_TARGET))
                    Ctx.UnbindTextureFromRenderTarget(pTexture);

                if (pTexture->CheckState(RESOURCE_STATE_DEPTH_WRITE))
                    Ctx.UnbindTextureFromDepthStencil(pTexture);

                pTexture->SetState(RESOURCE_STATE_SHADER_RESOURCE);*/
                Ctx.TransitionResource(pTexture, RESOURCE_STATE_SHADER_RESOURCE);
            }
        }
        else if (auto* pBuffer = SRVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
            { /*
                if (pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    Ctx.UnbindResourceFromUAV(pBuffer, SRVRes.pd3d11Resource);
                    pBuffer->ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
                }
                pBuffer->AddState(RESOURCE_STATE_SHADER_RESOURCE);*/
                Ctx.TransitionResource(pBuffer, RESOURCE_STATE_SHADER_RESOURCE);
            }
        }
    }
}

void ShaderResourceCacheD3D11::TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedSampler* Samplers, ID3D11SamplerState** pd3d11Samplers)
{
}

void ShaderResourceCacheD3D11::TransitionResource(DeviceContextD3D11Impl& Ctx, Uint32 Count, CachedResource* UAVResources, ID3D11UnorderedAccessView** pd3d11UAVs)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        auto& UAVRes = UAVResources[i];
        if (auto* pTexture = UAVRes.pTexture)
        {
            if (pTexture->IsInKnownState() && !pTexture->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            { /*
                if (pTexture->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
                    Ctx.UnbindTextureFromInput(pTexture, UAVRes.pd3d11Resource);
                pTexture->SetState(RESOURCE_STATE_UNORDERED_ACCESS);*/
                Ctx.TransitionResource(pTexture, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
        else if (auto* pBuffer = UAVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            { /*
                if ((pBuffer->GetState() & RESOURCE_STATE_GENERIC_READ) != 0)
                    Ctx.UnbindBufferFromInput(pBuffer, UAVRes.pd3d11Resource);
                pBuffer->SetState(RESOURCE_STATE_UNORDERED_ACCESS);*/
                Ctx.TransitionResource(pBuffer, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
    }
}

#ifdef DILIGENT_DEVELOPMENT
void ShaderResourceCacheD3D11::DvpVerifyResourceState(DeviceContextD3D11Impl&, Uint32 Count, const CachedCB* CBs, ID3D11Buffer**)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        if (const auto* pBuff = CBs[i].pBuff.RawPtr<BufferD3D11Impl>())
        {
            if (pBuff->IsInKnownState() && !pBuff->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBuff->GetDesc().Name, "' has not been transitioned to Constant Buffer state. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
            }
        }
    }
}

void ShaderResourceCacheD3D11::DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedResource* SRVResources, ID3D11ShaderResourceView**)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        auto& SRVRes = SRVResources[i];
        if (const auto* pTexture = SRVRes.pTexture)
        {
            if (pTexture->IsInKnownState() &&
                !(pTexture->CheckState(RESOURCE_STATE_SHADER_RESOURCE) || Ctx.HasActiveRenderPass() && pTexture->CheckState(RESOURCE_STATE_INPUT_ATTACHMENT)))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name, "' has not been transitioned to Shader Resource state. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the texture to required state.");
            }
        }
        else if (const auto* pBuffer = SRVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name, "' has not been transitioned to Shader Resource state. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
            }
        }
    }
}

void ShaderResourceCacheD3D11::DvpVerifyResourceState(DeviceContextD3D11Impl& Ctx, Uint32 Count, const CachedSampler* Samplers, ID3D11SamplerState**)
{
}

void ShaderResourceCacheD3D11::DvpVerifyResourceState(DeviceContextD3D11Impl&, Uint32 Count, const CachedResource* UAVResources, ID3D11UnorderedAccessView**)
{
    for (Uint32 i = 0; i < Count; ++i)
    {
        auto& UAVRes = UAVResources[i];
        if (const auto* pTexture = UAVRes.pTexture)
        {
            if (pTexture->IsInKnownState() && !pTexture->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name, "' has not been transitioned to Unordered Access state. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the texture to required state.");
            }
        }
        else if (const auto* pBuffer = UAVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name, "' has not been transitioned to Unordered Access state. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
            }
        }
    }
}
#endif // DILIGENT_DEVELOPMENT

void ShaderResourceCacheD3D11::BindResources(DeviceContextD3D11Impl& Ctx, const TBindingsPerStage& BaseBindings, TMinMaxSlotPerStage& MinMaxSlot, TCommittedResources& CommittedRes, SHADER_TYPE ActiveStages) const
{
    const auto CBCount = GetCBCount();
    if (CBCount != 0)
    {
        const auto                                Range = DESCRIPTOR_RANGE_CBV;
        ShaderResourceCacheD3D11::CachedCB const* CBs;
        ID3D11Buffer* const*                      d3d11CBs;
        TBindPointsAndActiveBits const*           bindPoints;
        GetConstCBArrays(CBs, d3d11CBs, bindPoints);

        for (Uint32 cb = 0; cb < CBCount; ++cb)
        {
            Uint32      ActiveBits = bindPoints[cb][0] & ActiveStages;
            const auto* BindPoints = &bindPoints[cb][1];
            VERIFY(ActiveBits != 0, "resource is not initialized");
            while (ActiveBits != 0)
            {
                const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
                ActiveBits &= ~(1u << ShaderInd);
                VERIFY_EXPR(BindPoints[ShaderInd] != InvalidBindPoint);

                auto*      CommittedD3D11CBs = CommittedRes.D3D11CBs[ShaderInd];
                UINT&      MinSlot           = MinMaxSlot[ShaderInd][Range].MinSlot;
                UINT&      MaxSlot           = MinMaxSlot[ShaderInd][Range].MaxSlot;
                const UINT Slot              = BaseBindings[ShaderInd][Range] + BindPoints[ShaderInd];
                const bool IsNewCB           = CommittedD3D11CBs[Slot] != d3d11CBs[cb];
                MinSlot                      = IsNewCB ? std::min(MinSlot, Slot) : MinSlot;
                MaxSlot                      = IsNewCB ? Slot : MaxSlot;

                VERIFY_EXPR(!IsNewCB || (Slot >= MinSlot && Slot <= MaxSlot));
                VERIFY_EXPR(d3d11CBs[cb] != nullptr);
                CommittedD3D11CBs[Slot] = d3d11CBs[cb];
            }
        }
    }

    const auto SRVCount = GetSRVCount();
    if (SRVCount != 0)
    {
        const auto                                      Range = DESCRIPTOR_RANGE_SRV;
        ShaderResourceCacheD3D11::CachedResource const* SRVResources;
        ID3D11ShaderResourceView* const*                d3d11SRVs;
        TBindPointsAndActiveBits const*                 bindPoints;
        GetConstSRVArrays(SRVResources, d3d11SRVs, bindPoints);

        for (Uint32 srv = 0; srv < SRVCount; ++srv)
        {
            Uint32      ActiveBits = bindPoints[srv][0] & ActiveStages;
            const auto* BindPoints = &bindPoints[srv][1];
            VERIFY(ActiveBits != 0, "resource is not initialized");
            while (ActiveBits != 0)
            {
                const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
                ActiveBits &= ~(1u << ShaderInd);
                VERIFY_EXPR(BindPoints[ShaderInd] != InvalidBindPoint);

                auto*      CommittedD3D11SRVs   = CommittedRes.D3D11SRVs[ShaderInd];
                auto*      CommittedD3D11SRVRes = CommittedRes.D3D11SRVResources[ShaderInd];
                UINT&      MinSlot              = MinMaxSlot[ShaderInd][Range].MinSlot;
                UINT&      MaxSlot              = MinMaxSlot[ShaderInd][Range].MaxSlot;
                const UINT Slot                 = BaseBindings[ShaderInd][Range] + BindPoints[ShaderInd];
                const bool IsNewSRV             = CommittedD3D11SRVs[Slot] != d3d11SRVs[srv];
                MinSlot                         = IsNewSRV ? std::min(MinSlot, Slot) : MinSlot;
                MaxSlot                         = IsNewSRV ? Slot : MaxSlot;

                VERIFY_EXPR(!IsNewSRV || (Slot >= MinSlot && Slot <= MaxSlot));
                VERIFY_EXPR(d3d11SRVs[srv] != nullptr);
                CommittedD3D11SRVRes[Slot] = SRVResources[srv].pd3d11Resource;
                CommittedD3D11SRVs[Slot]   = d3d11SRVs[srv];
            }
        }
    }

    const auto SamplerCount = GetSamplerCount();
    if (SamplerCount != 0)
    {
        const auto                                     Range = DESCRIPTOR_RANGE_SAMPLER;
        ShaderResourceCacheD3D11::CachedSampler const* Samplers;
        ID3D11SamplerState* const*                     d3d11Samplers;
        TBindPointsAndActiveBits const*                bindPoints;
        GetConstSamplerArrays(Samplers, d3d11Samplers, bindPoints);

        for (Uint32 sam = 0; sam < SamplerCount; ++sam)
        {
            Uint32      ActiveBits = bindPoints[sam][0] & ActiveStages;
            const auto* BindPoints = &bindPoints[sam][1];
            VERIFY(ActiveBits != 0, "resource is not initialized");
            while (ActiveBits != 0)
            {
                const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
                ActiveBits &= ~(1u << ShaderInd);
                VERIFY_EXPR(BindPoints[ShaderInd] != InvalidBindPoint);

                auto*      CommittedD3D11Samplers = CommittedRes.D3D11Samplers[ShaderInd];
                UINT&      MinSlot                = MinMaxSlot[ShaderInd][Range].MinSlot;
                UINT&      MaxSlot                = MinMaxSlot[ShaderInd][Range].MaxSlot;
                const UINT Slot                   = BaseBindings[ShaderInd][Range] + BindPoints[ShaderInd];
                const bool IsNewSam               = CommittedD3D11Samplers[Slot] != d3d11Samplers[sam];
                MinSlot                           = IsNewSam ? std::min(MinSlot, Slot) : MinSlot;
                MaxSlot                           = IsNewSam ? Slot : MaxSlot;

                VERIFY_EXPR(!IsNewSam || (Slot >= MinSlot && Slot <= MaxSlot));
                VERIFY_EXPR(d3d11Samplers[sam] != nullptr);
                CommittedD3D11Samplers[Slot] = d3d11Samplers[sam];
            }
        }
    }

    const auto UAVCount = GetUAVCount();
    if (UAVCount != 0)
    {
        const auto                                      Range = DESCRIPTOR_RANGE_UAV;
        ShaderResourceCacheD3D11::CachedResource const* UAVResources;
        ID3D11UnorderedAccessView* const*               d3d11UAVs;
        TBindPointsAndActiveBits const*                 bindPoints;
        GetConstUAVArrays(UAVResources, d3d11UAVs, bindPoints);

        for (Uint32 uav = 0; uav < UAVCount; ++uav)
        {
            Uint32      ActiveBits = bindPoints[uav][0] & ActiveStages;
            const auto* BindPoints = &bindPoints[uav][1];
            VERIFY(ActiveBits != 0, "resource is not initialized");
            while (ActiveBits != 0)
            {
                const Uint32 ShaderInd = PlatformMisc::GetLSB(ActiveBits);
                ActiveBits &= ~(1u << ShaderInd);
                VERIFY_EXPR(BindPoints[ShaderInd] != InvalidBindPoint);

                auto*      CommittedD3D11UAVs   = CommittedRes.D3D11UAVs[ShaderInd];
                auto*      CommittedD3D11UAVRes = CommittedRes.D3D11UAVResources[ShaderInd];
                UINT&      MinSlot              = MinMaxSlot[ShaderInd][Range].MinSlot;
                UINT&      MaxSlot              = MinMaxSlot[ShaderInd][Range].MaxSlot;
                const UINT Slot                 = BaseBindings[ShaderInd][Range] + BindPoints[ShaderInd];
                const bool IsNewUAV             = CommittedD3D11UAVs[Slot] != d3d11UAVs[uav];
                MinSlot                         = IsNewUAV ? std::min(MinSlot, Slot) : MinSlot;
                MaxSlot                         = IsNewUAV ? Slot : MaxSlot;

                VERIFY_EXPR(!IsNewUAV || (Slot >= MinSlot && Slot <= MaxSlot));
                VERIFY_EXPR(d3d11UAVs[uav] != nullptr);
                CommittedD3D11UAVRes[Slot] = UAVResources[uav].pd3d11Resource;
                CommittedD3D11UAVs[Slot]   = d3d11UAVs[uav];
            }
        }
    }
}

void ShaderResourceCacheD3D11::TCommittedResources::Clear()
{
    for (int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType)
    {
        // clang-format off
        memset(D3D11CBs[ShaderType],          0, sizeof(D3D11CBs[ShaderType][0])          * NumCBs[ShaderType]);
        memset(D3D11SRVs[ShaderType],         0, sizeof(D3D11SRVs[ShaderType][0])         * NumSRVs[ShaderType]);
        memset(D3D11Samplers[ShaderType],     0, sizeof(D3D11Samplers[ShaderType][0])     * NumSamplers[ShaderType]);
        memset(D3D11UAVs[ShaderType],         0, sizeof(D3D11UAVs[ShaderType][0])         * NumUAVs[ShaderType]);
        memset(D3D11SRVResources[ShaderType], 0, sizeof(D3D11SRVResources[ShaderType][0]) * NumSRVs[ShaderType]);
        memset(D3D11UAVResources[ShaderType], 0, sizeof(D3D11UAVResources[ShaderType][0]) * NumUAVs[ShaderType]);
        // clang-format on

        NumCBs[ShaderType]      = 0;
        NumSRVs[ShaderType]     = 0;
        NumSamplers[ShaderType] = 0;
        NumUAVs[ShaderType]     = 0;
    }
}

} // namespace Diligent
