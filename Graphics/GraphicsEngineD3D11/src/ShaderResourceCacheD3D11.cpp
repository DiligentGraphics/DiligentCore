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
    auto   CBCount      = ResCount[D3D11_RESOURCE_RANGE_CBV];
    auto   SRVCount     = ResCount[D3D11_RESOURCE_RANGE_SRV];
    auto   SamplerCount = ResCount[D3D11_RESOURCE_RANGE_SAMPLER];
    auto   UAVCount     = ResCount[D3D11_RESOURCE_RANGE_UAV];
    size_t MemSize      = 0;
    MemSize = AlignUp(MemSize + (sizeof(CachedCB)       + sizeof(BindPointsD3D11) + sizeof(ID3D11Buffer*))              * CBCount,      MaxAlignment);
    MemSize = AlignUp(MemSize + (sizeof(CachedResource) + sizeof(BindPointsD3D11) + sizeof(ID3D11ShaderResourceView*))  * SRVCount,     MaxAlignment);
    MemSize = AlignUp(MemSize + (sizeof(CachedSampler)  + sizeof(BindPointsD3D11) + sizeof(ID3D11SamplerState*))        * SamplerCount, MaxAlignment);
    MemSize = AlignUp(MemSize + (sizeof(CachedResource) + sizeof(BindPointsD3D11) + sizeof(ID3D11UnorderedAccessView*)) * UAVCount,     MaxAlignment);
    // clang-format on
    VERIFY(MemSize < InvalidResourceOffset, "Memory size exeed the maximum allowed size.");
    return MemSize;
}

void ShaderResourceCacheD3D11::Initialize(const TResourceCount& ResCount, IMemoryAllocator& MemAllocator)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
    VERIFY(!IsInitialized(), "Resource cache has already been intialized!");

    const Uint32 CBCount      = ResCount[D3D11_RESOURCE_RANGE_CBV];
    const Uint32 SRVCount     = ResCount[D3D11_RESOURCE_RANGE_SRV];
    const Uint32 SamplerCount = ResCount[D3D11_RESOURCE_RANGE_SAMPLER];
    const Uint32 UAVCount     = ResCount[D3D11_RESOURCE_RANGE_UAV];

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
    m_SRVOffset       = static_cast<OffsetType>(AlignUp(m_CBOffset      + (sizeof(CachedCB)       + sizeof(BindPointsD3D11) + sizeof(ID3D11Buffer*))              * CBCount,      MaxAlignment));
    m_SamplerOffset   = static_cast<OffsetType>(AlignUp(m_SRVOffset     + (sizeof(CachedResource) + sizeof(BindPointsD3D11) + sizeof(ID3D11ShaderResourceView*))  * SRVCount,     MaxAlignment));
    m_UAVOffset       = static_cast<OffsetType>(AlignUp(m_SamplerOffset + (sizeof(CachedSampler)  + sizeof(BindPointsD3D11) + sizeof(ID3D11SamplerState*))        * SamplerCount, MaxAlignment));
    size_t BufferSize = static_cast<OffsetType>(AlignUp(m_UAVOffset     + (sizeof(CachedResource) + sizeof(BindPointsD3D11) + sizeof(ID3D11UnorderedAccessView*)) * UAVCount,     MaxAlignment));
    // clang-format on

    VERIFY_EXPR(m_pResourceData == nullptr);
    VERIFY_EXPR(BufferSize == GetRequriedMemorySize(ResCount));

    if (BufferSize > 0)
    {
        m_pResourceData = decltype(m_pResourceData){
            ALLOCATE(MemAllocator, "Shader resource cache data buffer", Uint8, BufferSize),
            STDDeleter<Uint8, IMemoryAllocator>(MemAllocator) //
        };
        memset(m_pResourceData.get(), 0, BufferSize);
    }

    // Explicitly construct all objects
    if (CBCount != 0)
    {
        CachedCB*        CBs        = nullptr;
        ID3D11Buffer**   d3d11CBs   = nullptr;
        BindPointsD3D11* bindPoints = nullptr;
        GetCBArrays(CBs, d3d11CBs, bindPoints);
        for (Uint32 cb = 0; cb < CBCount; ++cb)
        {
            new (CBs + cb) CachedCB{};
            new (bindPoints + cb) BindPointsD3D11{};
        }
    }

    if (SRVCount != 0)
    {
        CachedResource*            SRVResources = nullptr;
        ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
        BindPointsD3D11*           bindPoints   = nullptr;
        GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);
        for (Uint32 srv = 0; srv < SRVCount; ++srv)
        {
            new (SRVResources + srv) CachedResource{};
            new (bindPoints + srv) BindPointsD3D11{};
        }
    }

    if (SamplerCount != 0)
    {
        CachedSampler*       Samplers      = nullptr;
        ID3D11SamplerState** d3d11Samplers = nullptr;
        BindPointsD3D11*     bindPoints    = nullptr;
        GetSamplerArrays(Samplers, d3d11Samplers, bindPoints);
        for (Uint32 sam = 0; sam < SamplerCount; ++sam)
        {
            new (Samplers + sam) CachedSampler{};
            new (bindPoints + sam) BindPointsD3D11{};
        }
    }

    if (UAVCount != 0)
    {
        CachedResource*             UAVResources = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
        BindPointsD3D11*            bindPoints   = nullptr;
        GetUAVArrays(UAVResources, d3d11UAVs, bindPoints);
        for (Uint32 uav = 0; uav < UAVCount; ++uav)
        {
            new (UAVResources + uav) CachedResource{};
            new (bindPoints + uav) BindPointsD3D11{};
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
            CachedCB*        CBs        = nullptr;
            ID3D11Buffer**   d3d11CBs   = nullptr;
            BindPointsD3D11* bindPoints = nullptr;
            GetCBArrays(CBs, d3d11CBs, bindPoints);
            for (size_t cb = 0; cb < CBCount; ++cb)
                CBs[cb].~CachedCB();
        }

        auto SRVCount = GetSRVCount();
        if (SRVCount != 0)
        {
            CachedResource*            SRVResources = nullptr;
            ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
            BindPointsD3D11*           bindPoints   = nullptr;
            GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);
            for (size_t srv = 0; srv < SRVCount; ++srv)
                SRVResources[srv].~CachedResource();
        }

        auto SamplerCount = GetSamplerCount();
        if (SamplerCount != 0)
        {
            CachedSampler*       Samplers      = nullptr;
            ID3D11SamplerState** d3d11Samplers = nullptr;
            BindPointsD3D11*     bindPoints    = nullptr;
            GetSamplerArrays(Samplers, d3d11Samplers, bindPoints);
            for (size_t sam = 0; sam < SamplerCount; ++sam)
                Samplers[sam].~CachedSampler();
        }

        auto UAVCount = GetUAVCount();
        if (UAVCount != 0)
        {
            CachedResource*             UAVResources = nullptr;
            ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
            BindPointsD3D11*            bindPoints   = nullptr;
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

        m_pResourceData.reset();
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
                RefCntAutoPtr<IBufferViewD3D11> pBufView{Res.pView, IID_BufferViewD3D11};
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
                RefCntAutoPtr<ITextureViewD3D11> pTexView{Res.pView, IID_TextureViewD3D11};
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
    BindPointsD3D11*            bindPoints    = nullptr;

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

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResourceStates(DeviceContextD3D11Impl& Ctx)
{
    VERIFY_EXPR(IsInitialized());

    TransitionResources<Mode>(Ctx, static_cast<ID3D11Buffer*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11ShaderResourceView*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11SamplerState*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11UnorderedAccessView*>(nullptr));
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11Buffer* /*Selector*/) const
{
    const auto CBCount = GetCBCount();
    if (CBCount == 0)
        return;

    CachedCB*        CBs;
    ID3D11Buffer**   d3d11CBs;
    BindPointsD3D11* bindPoints;
    GetCBArrays(CBs, d3d11CBs, bindPoints);

    for (Uint32 i = 0; i < CBCount; ++i)
    {
        if (auto* pBuffer = CBs[i].pBuff.RawPtr<BufferD3D11Impl>())
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
            {
                if (Mode == StateTransitionMode::Transition)
                {
                    Ctx.TransitionResource(pBuffer, RESOURCE_STATE_CONSTANT_BUFFER);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name,
                                      "' has not been transitioned to Constant Buffer state. Call TransitionShaderResources(), use "
                                      "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
                }
            }
        }
    }
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11ShaderResourceView* /*Selector*/) const
{
    const auto SRVCount = GetSRVCount();
    if (SRVCount == 0)
        return;

    CachedResource*            SRVResources;
    ID3D11ShaderResourceView** d3d11SRVs;
    BindPointsD3D11*           bindPoints;
    GetSRVArrays(SRVResources, d3d11SRVs, bindPoints);

    for (Uint32 i = 0; i < SRVCount; ++i)
    {
        auto& SRVRes = SRVResources[i];
        if (auto* pTexture = SRVRes.pTexture)
        {
            if (pTexture->IsInKnownState() && !pTexture->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
            {
                if (Mode == StateTransitionMode::Transition)
                {
                    Ctx.TransitionResource(pTexture, RESOURCE_STATE_SHADER_RESOURCE);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name,
                                      "' has not been transitioned to Shader Resource state. Call TransitionShaderResources(), use "
                                      "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the texture to required state.");
                }
            }
        }
        else if (auto* pBuffer = SRVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
            {
                if (Mode == StateTransitionMode::Transition)
                {
                    Ctx.TransitionResource(pBuffer, RESOURCE_STATE_SHADER_RESOURCE);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name,
                                      "' has not been transitioned to Shader Resource state. Call TransitionShaderResources(), use "
                                      "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
                }
            }
        }
    }
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11SamplerState* /*Selector*/) const
{
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11UnorderedAccessView* /*Selector*/) const
{
    const auto UAVCount = GetUAVCount();
    if (UAVCount == 0)
        return;

    CachedResource*             UAVResources;
    ID3D11UnorderedAccessView** d3d11UAVs;
    BindPointsD3D11*            bindPoints;
    GetUAVArrays(UAVResources, d3d11UAVs, bindPoints);

    for (Uint32 i = 0; i < UAVCount; ++i)
    {
        auto& UAVRes = UAVResources[i];
        if (auto* pTexture = UAVRes.pTexture)
        {
            if (pTexture->IsInKnownState() && !pTexture->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                if (Mode == StateTransitionMode::Transition)
                {
                    Ctx.TransitionResource(pTexture, RESOURCE_STATE_UNORDERED_ACCESS);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name,
                                      "' has not been transitioned to Unordered Access state. Call TransitionShaderResources(), use "
                                      "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the texture to required state.");
                }
            }
        }
        else if (auto* pBuffer = UAVRes.pBuffer)
        {
            if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                if (Mode == StateTransitionMode::Transition)
                {
                    Ctx.TransitionResource(pBuffer, RESOURCE_STATE_UNORDERED_ACCESS);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name,
                                      "' has not been transitioned to Unordered Access state. Call TransitionShaderResources(), use "
                                      "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to required state.");
                }
            }
        }
    }
}

} // namespace Diligent
