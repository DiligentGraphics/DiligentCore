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

size_t ShaderResourceCacheD3D11::GetRequriedMemorySize(const TBindingsPerStage& ResCount)
{
    size_t MemSize = 0;
    // clang-format off
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        MemSize = AlignUp(MemSize + (sizeof(CachedCB)       + sizeof(ID3D11Buffer*))              * ResCount[D3D11_RESOURCE_RANGE_CBV][ShaderInd],     MaxAlignment);
    
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        MemSize = AlignUp(MemSize + (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*))  * ResCount[D3D11_RESOURCE_RANGE_SRV][ShaderInd],     MaxAlignment);
        
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        MemSize = AlignUp(MemSize + (sizeof(CachedSampler)  + sizeof(ID3D11SamplerState*))        * ResCount[D3D11_RESOURCE_RANGE_SAMPLER][ShaderInd], MaxAlignment);
        
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        MemSize = AlignUp(MemSize + (sizeof(CachedResource) + sizeof(ID3D11UnorderedAccessView*)) * ResCount[D3D11_RESOURCE_RANGE_UAV][ShaderInd],     MaxAlignment);
    // clang-format on

    VERIFY(MemSize < std::numeric_limits<OffsetType>::max(), "Memory size exeed the maximum allowed size.");
    return MemSize;
}

void ShaderResourceCacheD3D11::Initialize(const TBindingsPerStage& ResCount, IMemoryAllocator& MemAllocator)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
    VERIFY(!IsInitialized(), "Resource cache has already been intialized!");

    size_t MemOffset = 0;
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 Idx = CBOffset + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedCB) + sizeof(ID3D11Buffer*)) * ResCount[D3D11_RESOURCE_RANGE_CBV][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 Idx = SRVOffset + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*)) * ResCount[D3D11_RESOURCE_RANGE_SRV][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 Idx = SampOffset + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedSampler) + sizeof(ID3D11SamplerState*)) * ResCount[D3D11_RESOURCE_RANGE_SAMPLER][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 Idx = UAVOffset + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedResource) + sizeof(ID3D11UnorderedAccessView*)) * ResCount[D3D11_RESOURCE_RANGE_UAV][ShaderInd], MaxAlignment);
    }
    m_Offsets[MaxOffsets - 1] = static_cast<OffsetType>(MemOffset);

    const size_t BufferSize = MemOffset;

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
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const auto CBCount = GetCBCount(ShaderInd);
        if (CBCount != 0)
        {
            CachedCB*      CBs      = nullptr;
            ID3D11Buffer** d3d11CBs = nullptr;
            GetCBArrays(ShaderInd, CBs, d3d11CBs);
            for (Uint32 cb = 0; cb < CBCount; ++cb)
                new (CBs + cb) CachedCB{};
        }

        const auto SRVCount = GetSRVCount(ShaderInd);
        if (SRVCount != 0)
        {
            CachedResource*            SRVResources = nullptr;
            ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
            GetSRVArrays(ShaderInd, SRVResources, d3d11SRVs);
            for (Uint32 srv = 0; srv < SRVCount; ++srv)
                new (SRVResources + srv) CachedResource{};
        }

        const auto SamplerCount = GetSamplerCount(ShaderInd);
        if (SamplerCount != 0)
        {
            CachedSampler*       Samplers      = nullptr;
            ID3D11SamplerState** d3d11Samplers = nullptr;
            GetSamplerArrays(ShaderInd, Samplers, d3d11Samplers);
            for (Uint32 sam = 0; sam < SamplerCount; ++sam)
                new (Samplers + sam) CachedSampler{};
        }

        const auto UAVCount = GetUAVCount(ShaderInd);
        if (UAVCount != 0)
        {
            CachedResource*             UAVResources = nullptr;
            ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
            GetUAVArrays(ShaderInd, UAVResources, d3d11UAVs);
            for (Uint32 uav = 0; uav < UAVCount; ++uav)
                new (UAVResources + uav) CachedResource{};
        }
    }

    m_IsInitialized = true;
}

ShaderResourceCacheD3D11::~ShaderResourceCacheD3D11()
{
    if (IsInitialized())
    {
        // Explicitly destory all objects
        for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        {
            const auto CBCount = GetCBCount(ShaderInd);
            if (CBCount != 0)
            {
                CachedCB*      CBs      = nullptr;
                ID3D11Buffer** d3d11CBs = nullptr;
                GetCBArrays(ShaderInd, CBs, d3d11CBs);
                for (size_t cb = 0; cb < CBCount; ++cb)
                    CBs[cb].~CachedCB();
            }

            const auto SRVCount = GetSRVCount(ShaderInd);
            if (SRVCount != 0)
            {
                CachedResource*            SRVResources = nullptr;
                ID3D11ShaderResourceView** d3d11SRVs    = nullptr;
                GetSRVArrays(ShaderInd, SRVResources, d3d11SRVs);
                for (size_t srv = 0; srv < SRVCount; ++srv)
                    SRVResources[srv].~CachedResource();
            }

            const auto SamplerCount = GetSamplerCount(ShaderInd);
            if (SamplerCount != 0)
            {
                CachedSampler*       Samplers      = nullptr;
                ID3D11SamplerState** d3d11Samplers = nullptr;
                GetSamplerArrays(ShaderInd, Samplers, d3d11Samplers);
                for (size_t sam = 0; sam < SamplerCount; ++sam)
                    Samplers[sam].~CachedSampler();
            }

            const auto UAVCount = GetUAVCount(ShaderInd);
            if (UAVCount != 0)
            {
                CachedResource*             UAVResources = nullptr;
                ID3D11UnorderedAccessView** d3d11UAVs    = nullptr;
                GetUAVArrays(ShaderInd, UAVResources, d3d11UAVs);
                for (size_t uav = 0; uav < UAVCount; ++uav)
                    UAVResources[uav].~CachedResource();
            }
        }
        m_Offsets       = {};
        m_IsInitialized = false;

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

    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        CachedCB*                   CBs           = nullptr;
        ID3D11Buffer**              d3d11CBs      = nullptr;
        CachedResource*             SRVResources  = nullptr;
        ID3D11ShaderResourceView**  d3d11SRVs     = nullptr;
        CachedSampler*              Samplers      = nullptr;
        ID3D11SamplerState**        d3d11Samplers = nullptr;
        CachedResource*             UAVResources  = nullptr;
        ID3D11UnorderedAccessView** d3d11UAVs     = nullptr;

        GetCBArrays(ShaderInd, CBs, d3d11CBs);
        GetSRVArrays(ShaderInd, SRVResources, d3d11SRVs);
        GetSamplerArrays(ShaderInd, Samplers, d3d11Samplers);
        GetUAVArrays(ShaderInd, UAVResources, d3d11UAVs);

        auto CBCount = GetCBCount(ShaderInd);
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

        auto SRVCount = GetSRVCount(ShaderInd);
        for (size_t srv = 0; srv < SRVCount; ++srv)
        {
            auto& Res       = SRVResources[srv];
            auto* pd3d11SRV = d3d11SRVs[srv];
            DvpVerifyResource(Res, pd3d11SRV, "SRV");
        }

        auto UAVCount = GetUAVCount(ShaderInd);
        for (size_t uav = 0; uav < UAVCount; ++uav)
        {
            auto& Res       = UAVResources[uav];
            auto* pd3d11UAV = d3d11UAVs[uav];
            DvpVerifyResource(Res, pd3d11UAV, "UAV");
        }

        auto SamplerCount = GetSamplerCount(ShaderInd);
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
}
#endif // DILIGENT_DEVELOPMENT

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
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const auto CBCount = GetCBCount(ShaderInd);
        if (CBCount == 0)
            continue;

        CachedCB*      CBs;
        ID3D11Buffer** d3d11CBs;
        GetCBArrays(ShaderInd, CBs, d3d11CBs);

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
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11ShaderResourceView* /*Selector*/) const
{
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const auto SRVCount = GetSRVCount(ShaderInd);
        if (SRVCount == 0)
            continue;

        CachedResource*            SRVResources;
        ID3D11ShaderResourceView** d3d11SRVs;
        GetSRVArrays(ShaderInd, SRVResources, d3d11SRVs);

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
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11SamplerState* /*Selector*/) const
{
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11UnorderedAccessView* /*Selector*/) const
{
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const auto UAVCount = GetUAVCount(ShaderInd);
        if (UAVCount == 0)
            continue;

        CachedResource*             UAVResources;
        ID3D11UnorderedAccessView** d3d11UAVs;
        GetUAVArrays(ShaderInd, UAVResources, d3d11UAVs);

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
}

ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindCBs(
    Uint32        ShaderInd,
    ID3D11Buffer* CommittedD3D11CBs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT],
    Uint8&        Binding) const
{
    CachedCB const*      CBs;
    ID3D11Buffer* const* d3d11CBs;
    GetConstCBArrays(ShaderInd, CBs, d3d11CBs);

    MinMaxSlot Slots;

    const auto CBCount = GetCBCount(ShaderInd);
    for (Uint32 cb = 0; cb < CBCount; ++cb)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11CBs[Slot] != d3d11CBs[cb])
            Slots.Add(Slot);

        VERIFY_EXPR(d3d11CBs[cb] != nullptr);
        CommittedD3D11CBs[Slot] = d3d11CBs[cb];
    }

    return Slots;
}

ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindSRVs(
    Uint32                    ShaderInd,
    ID3D11ShaderResourceView* CommittedD3D11SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
    ID3D11Resource*           CommittedD3D11SRVResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT],
    Uint8&                    Binding) const
{
    CachedResource const*            SRVResources;
    ID3D11ShaderResourceView* const* d3d11SRVs;
    GetConstSRVArrays(ShaderInd, SRVResources, d3d11SRVs);

    MinMaxSlot Slots;

    const auto SRVCount = GetSRVCount(ShaderInd);
    for (Uint32 srv = 0; srv < SRVCount; ++srv)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11SRVs[Slot] != d3d11SRVs[srv])
            Slots.Add(Slot);

        VERIFY_EXPR(d3d11SRVs[srv] != nullptr);
        CommittedD3D11SRVResources[Slot] = SRVResources[srv].pd3d11Resource;
        CommittedD3D11SRVs[Slot]         = d3d11SRVs[srv];
    }

    return Slots;
}

ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindSamplers(
    Uint32              ShaderInd,
    ID3D11SamplerState* CommittedD3D11Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT],
    Uint8&              Binding) const
{
    CachedSampler const*       Samplers;
    ID3D11SamplerState* const* d3d11Samplers;
    GetConstSamplerArrays(ShaderInd, Samplers, d3d11Samplers);

    MinMaxSlot Slots;

    const auto SamplerCount = GetSamplerCount(ShaderInd);
    for (Uint32 sam = 0; sam < SamplerCount; ++sam)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11Samplers[Slot] != d3d11Samplers[sam])
            Slots.Add(Slot);

        VERIFY_EXPR(d3d11Samplers[sam] != nullptr);
        CommittedD3D11Samplers[Slot] = d3d11Samplers[sam];
    }

    return Slots;
}

ShaderResourceCacheD3D11::MinMaxSlot ShaderResourceCacheD3D11::BindUAVs(
    Uint32                     ShaderInd,
    ID3D11UnorderedAccessView* CommittedD3D11UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT],
    ID3D11Resource*            CommittedD3D11UAVResources[D3D11_PS_CS_UAV_REGISTER_COUNT],
    Uint8&                     Binding) const
{
    CachedResource const*             UAVResources;
    ID3D11UnorderedAccessView* const* d3d11UAVs;
    GetConstUAVArrays(ShaderInd, UAVResources, d3d11UAVs);

    MinMaxSlot Slots;

    const auto UAVCount = GetUAVCount(ShaderInd);
    for (Uint32 uav = 0; uav < UAVCount; ++uav)
    {
        const Uint32 Slot = Binding++;
        if (CommittedD3D11UAVs[Slot] != d3d11UAVs[uav])
            Slots.Add(Slot);

        VERIFY_EXPR(d3d11UAVs[uav] != nullptr);
        CommittedD3D11UAVResources[Slot] = UAVResources[uav].pd3d11Resource;
        CommittedD3D11UAVs[Slot]         = d3d11UAVs[uav];
    }

    return Slots;
}

} // namespace Diligent
