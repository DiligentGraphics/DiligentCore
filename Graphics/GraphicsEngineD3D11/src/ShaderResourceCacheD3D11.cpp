/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

const char* ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_CBV>::Name     = "Constant buffer";
const char* ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_SAMPLER>::Name = "Sampler";
const char* ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_SRV>::Name     = "Shader resource view";
const char* ShaderResourceCacheD3D11::CachedResourceTraits<D3D11_RESOURCE_RANGE_UAV>::Name     = "Unordered access view";

size_t ShaderResourceCacheD3D11::GetRequiredMemorySize(const D3D11ShaderResourceCounters& ResCount)
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

    VERIFY(MemSize < std::numeric_limits<OffsetType>::max(), "Memory size exceed the maximum allowed size.");
    return MemSize;
}

template <D3D11_RESOURCE_RANGE RangeType>
void ShaderResourceCacheD3D11::ConstructResources(Uint32 ShaderInd)
{
    using ResourceType = typename CachedResourceTraits<RangeType>::CachedResourceType;

    const Uint32 ResCount = GetResourceCount<RangeType>(ShaderInd);
    if (ResCount > 0)
    {
        const auto Arrays = GetResourceArrays<RangeType>(ShaderInd);
        for (Uint32 r = 0; r < ResCount; ++r)
            new (Arrays.first + r) ResourceType{};
    }
}

template <D3D11_RESOURCE_RANGE RangeType>
void ShaderResourceCacheD3D11::DestructResources(Uint32 ShaderInd)
{
    using ResourceType = typename CachedResourceTraits<RangeType>::CachedResourceType;

    const Uint32 ResCount = GetResourceCount<RangeType>(ShaderInd);
    if (ResCount > 0)
    {
        auto Arrays = GetResourceArrays<RangeType>(ShaderInd);
        for (Uint32 r = 0; r < ResCount; ++r)
            Arrays.first[r].~ResourceType();
    }
}

void ShaderResourceCacheD3D11::Initialize(const D3D11ShaderResourceCounters&        ResCount,
                                          IMemoryAllocator&                         MemAllocator,
                                          const std::array<Uint16, NumShaderTypes>* pDynamicCBSlotsMask,
                                          const InlineConstantBufferAttribsD3D11*   pInlineCBs,
                                          Uint32                                    NumInlineCBs)
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache/
    VERIFY(!IsInitialized(), "Resource cache has already been initialized!");

    if (pDynamicCBSlotsMask != nullptr)
        m_DynamicCBSlotsMask = *pDynamicCBSlotsMask;

    size_t MemOffset = 0;
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const size_t Idx = FirstCBOffsetIdx + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedCB) + sizeof(ID3D11Buffer*)) * ResCount[D3D11_RESOURCE_RANGE_CBV][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const size_t Idx = FirstSRVOffsetIdx + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedResource) + sizeof(ID3D11ShaderResourceView*)) * ResCount[D3D11_RESOURCE_RANGE_SRV][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const size_t Idx = FirstSamOffsetIdx + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedSampler) + sizeof(ID3D11SamplerState*)) * ResCount[D3D11_RESOURCE_RANGE_SAMPLER][ShaderInd], MaxAlignment);
    }
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const size_t Idx = FirstUAVOffsetIdx + ShaderInd;
        m_Offsets[Idx]   = static_cast<OffsetType>(MemOffset);
        MemOffset        = AlignUp(MemOffset + (sizeof(CachedResource) + sizeof(ID3D11UnorderedAccessView*)) * ResCount[D3D11_RESOURCE_RANGE_UAV][ShaderInd], MaxAlignment);
    }
    m_Offsets[MaxOffsets - 1] = static_cast<OffsetType>(MemOffset);

    size_t BufferSize = MemOffset;

    VERIFY_EXPR(m_pResourceData == nullptr);
    VERIFY_EXPR(BufferSize == GetRequiredMemorySize(ResCount));

    auto ProcessInlineCBs = [pInlineCBs, NumInlineCBs, this](auto Handler) {
        for (Uint32 i = 0; i < NumInlineCBs; ++i)
        {
            const InlineConstantBufferAttribsD3D11& InlineCBAttr = pInlineCBs[i];

            SHADER_TYPE  ActiveStages = InlineCBAttr.BindPoints.GetActiveStages();
            const Uint32 ShaderInd0   = ExtractFirstShaderStageIndex(ActiveStages);
            const Uint32 Binding0     = InlineCBAttr.BindPoints[ShaderInd0];
            const bool   IsInRange    = Binding0 < GetResourceCount<D3D11_RESOURCE_RANGE_CBV>(ShaderInd0);

#ifdef DILIGENT_DEBUG
            while (ActiveStages != SHADER_TYPE_UNKNOWN)
            {
                const Uint32 ShaderInd = ExtractFirstShaderStageIndex(ActiveStages);
                const Uint32 Binding   = InlineCBAttr.BindPoints[ShaderInd];
                VERIFY(IsInRange == (Binding < GetResourceCount<D3D11_RESOURCE_RANGE_CBV>(ShaderInd)),
                       "All binding points for inline constant buffer must be in range or out of range simultaneously.");
            }
#endif

            if (IsInRange)
            {
                Handler(InlineCBAttr);
            }
        }
    };

    ProcessInlineCBs([&BufferSize](const InlineConstantBufferAttribsD3D11& InlineCBAttr) {
        BufferSize += InlineCBAttr.NumConstants * sizeof(Uint32);
    });

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
        ConstructResources<D3D11_RESOURCE_RANGE_CBV>(ShaderInd);
        ConstructResources<D3D11_RESOURCE_RANGE_SRV>(ShaderInd);
        ConstructResources<D3D11_RESOURCE_RANGE_SAMPLER>(ShaderInd);
        ConstructResources<D3D11_RESOURCE_RANGE_UAV>(ShaderInd);
    }

    Uint32* pInlineCBData = reinterpret_cast<Uint32*>(m_pResourceData.get() + MemOffset);
    // Initialize inline constant buffers.
    ProcessInlineCBs([&pInlineCBData, this](const InlineConstantBufferAttribsD3D11& InlineCBAttr) {
        VERIFY_EXPR(InlineCBAttr.NumConstants > 0);
        VERIFY_EXPR(InlineCBAttr.pBuffer != nullptr);
        InitInlineConstantBuffer(InlineCBAttr.BindPoints, InlineCBAttr.pBuffer, InlineCBAttr.NumConstants, pInlineCBData);
        pInlineCBData += InlineCBAttr.NumConstants;
    });

    m_IsInitialized = true;
}

ShaderResourceCacheD3D11::~ShaderResourceCacheD3D11()
{
    if (IsInitialized())
    {
        // Explicitly destroy all objects
        for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
        {
            DestructResources<D3D11_RESOURCE_RANGE_CBV>(ShaderInd);
            DestructResources<D3D11_RESOURCE_RANGE_SRV>(ShaderInd);
            DestructResources<D3D11_RESOURCE_RANGE_SAMPLER>(ShaderInd);
            DestructResources<D3D11_RESOURCE_RANGE_UAV>(ShaderInd);
        }
        m_Offsets       = {};
        m_IsInitialized = false;

        m_pResourceData.reset();
    }
}

void ShaderResourceCacheD3D11::InitInlineConstantBuffer(const D3D11ResourceBindPoints& BindPoints,
                                                        RefCntAutoPtr<BufferD3D11Impl> pBuffer,
                                                        Uint32                         NumConstants,
                                                        void*                          pInlineConstantData)
{
    VERIFY_EXPR(pBuffer);
    VERIFY_EXPR(pInlineConstantData);
    ID3D11Buffer* pd3d11Buffer = pBuffer->GetD3D11Buffer();
    VERIFY_EXPR(pd3d11Buffer);
    for (SHADER_TYPE ActiveStages = BindPoints.GetActiveStages(); ActiveStages != SHADER_TYPE_UNKNOWN;)
    {
        const Uint32 ShaderInd = ExtractFirstShaderStageIndex(ActiveStages);
        const Uint32 Binding   = BindPoints[ShaderInd];
        VERIFY(Binding < GetResourceCount<D3D11_RESOURCE_RANGE_CBV>(ShaderInd), "Cache offset is out of range");

        auto  ResArrays = GetResourceArrays<D3D11_RESOURCE_RANGE_CBV>(ShaderInd);
        auto& CachedRes = ResArrays.first[Binding];
        auto& pd3d11Res = ResArrays.second[Binding];

        CachedRes.pBuff               = pBuffer;
        CachedRes.RangeSize           = NumConstants * sizeof(Uint32);
        CachedRes.pInlineConstantData = pInlineConstantData;
        pd3d11Res                     = pd3d11Buffer;
    }
}

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResourceStates(DeviceContextD3D11Impl& Ctx)
{
    VERIFY_EXPR(IsInitialized());

    TransitionResources<Mode>(Ctx, static_cast<ID3D11Buffer*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11ShaderResourceView*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11SamplerState*>(nullptr));
    TransitionResources<Mode>(Ctx, static_cast<ID3D11UnorderedAccessView*>(nullptr));
}

// Instantiate templates
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(DeviceContextD3D11Impl& Ctx);
template void ShaderResourceCacheD3D11::TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Verify>(DeviceContextD3D11Impl& Ctx);

template <ShaderResourceCacheD3D11::StateTransitionMode Mode>
void ShaderResourceCacheD3D11::TransitionResources(DeviceContextD3D11Impl& Ctx, const ID3D11Buffer* /*Selector*/) const
{
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 CBCount = GetCBCount(ShaderInd);
        if (CBCount == 0)
            continue;

        auto CBArrays = GetResourceArrays<D3D11_RESOURCE_RANGE_CBV>(ShaderInd);
        for (Uint32 i = 0; i < CBCount; ++i)
        {
            if (BufferD3D11Impl* pBuffer = CBArrays.first[i].pBuff)
            {
                if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
                {
                    if (Mode == StateTransitionMode::Transition)
                    {
                        Ctx.TransitionResource(*pBuffer, RESOURCE_STATE_CONSTANT_BUFFER);
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name,
                                          "' has not been transitioned to CONSTANT_BUFFER state. Call TransitionShaderResources(), use "
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
        const Uint32 SRVCount = GetSRVCount(ShaderInd);
        if (SRVCount == 0)
            continue;

        auto SRVArrays = GetResourceArrays<D3D11_RESOURCE_RANGE_SRV>(ShaderInd);
        for (Uint32 i = 0; i < SRVCount; ++i)
        {
            CachedResource& SRVRes = SRVArrays.first[i];
            if (TextureBaseD3D11* pTexture = SRVRes.pTexture)
            {
                RESOURCE_STATE RequiredStates = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT;

                const TextureDesc&          TexDesc    = pTexture->GetDesc();
                const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
                if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH || FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
                {
                    RequiredStates |= RESOURCE_STATE_DEPTH_READ;
                }
                if (pTexture->IsInKnownState() && !pTexture->CheckAnyState(RequiredStates))
                {
                    if (Mode == StateTransitionMode::Transition)
                    {
                        Ctx.TransitionResource(*pTexture, RESOURCE_STATE_SHADER_RESOURCE);
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name,
                                          "' has not been transitioned to one of ", GetResourceStateString(RequiredStates),
                                          ", states. Call TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode "
                                          "or explicitly transition the texture to required state.");
                    }
                }
            }
            else if (BufferD3D11Impl* pBuffer = SRVRes.pBuffer)
            {
                if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
                {
                    if (Mode == StateTransitionMode::Transition)
                    {
                        Ctx.TransitionResource(*pBuffer, RESOURCE_STATE_SHADER_RESOURCE);
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBuffer->GetDesc().Name,
                                          "' has not been transitioned to SHADER_RESOURCE state. Call TransitionShaderResources(), use "
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
        const Uint32 UAVCount = GetUAVCount(ShaderInd);
        if (UAVCount == 0)
            continue;

        auto UAVArrays = GetResourceArrays<D3D11_RESOURCE_RANGE_UAV>(ShaderInd);
        for (Uint32 i = 0; i < UAVCount; ++i)
        {
            CachedResource& UAVRes = UAVArrays.first[i];
            if (TextureBaseD3D11* pTexture = UAVRes.pTexture)
            {
                if (pTexture->IsInKnownState() && !pTexture->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    if (Mode == StateTransitionMode::Transition)
                    {
                        Ctx.TransitionResource(*pTexture, RESOURCE_STATE_UNORDERED_ACCESS);
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Texture '", pTexture->GetDesc().Name,
                                          "' has not been transitioned to Unordered Access state. Call TransitionShaderResources(), use "
                                          "RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the texture to required state.");
                    }
                }
            }
            else if (BufferD3D11Impl* pBuffer = UAVRes.pBuffer)
            {
                if (pBuffer->IsInKnownState() && !pBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    if (Mode == StateTransitionMode::Transition)
                    {
                        Ctx.TransitionResource(*pBuffer, RESOURCE_STATE_UNORDERED_ACCESS);
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

#ifdef DILIGENT_DEBUG
void ShaderResourceCacheD3D11::DbgVerifyDynamicBufferMasks() const
{
    for (Uint32 ShaderInd = 0; ShaderInd < NumShaderTypes; ++ShaderInd)
    {
        const Uint32 CBCount = GetCBCount(ShaderInd);
        if (CBCount == 0)
            continue;

        auto CBArrays = GetResourceArrays<D3D11_RESOURCE_RANGE_CBV>(ShaderInd);
        for (Uint32 i = 0; i < CBCount; ++i)
        {
            const Uint32    BuffBit = 1u << i;
            const CachedCB& CB      = CBArrays.first[i];

            const bool IsDynamicOffset = CB.AllowsDynamicOffset() && (m_DynamicCBSlotsMask[ShaderInd] & BuffBit) != 0;
            VERIFY(IsDynamicOffset == ((m_DynamicCBOffsetsMask[ShaderInd] & BuffBit) != 0), "Bit ", i, " in m_DynamicCBOffsetsMask is not valid");
        }
    }
}
#endif

} // namespace Diligent
