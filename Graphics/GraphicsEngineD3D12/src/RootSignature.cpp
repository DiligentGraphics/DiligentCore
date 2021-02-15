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

#include "RootSignature.hpp"
#include "CommandContext.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

RootSignatureD3D12::RootSignatureD3D12(IReferenceCounters*                                      pRefCounters,
                                       RenderDeviceD3D12Impl*                                   pDeviceD3D12Impl,
                                       const RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>* ppSignatures,
                                       Uint32                                                   SignatureCount,
                                       size_t                                                   Hash) :
    ObjectBase<IObject>{pRefCounters},
    m_SignatureCount{static_cast<Uint8>(SignatureCount)},
    m_Hash{Hash},
    m_Cache{pDeviceD3D12Impl->GetRootSignatureCache()}
{
    VERIFY(m_SignatureCount == SignatureCount, "Signature count (", SignatureCount, ") exceeds maximum representable value");

    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        m_Signatures[i] = ppSignatures[i];

        if (ppSignatures[i] != nullptr)
        {
            VERIFY(ppSignatures[i]->GetDesc().BindingIndex == i, "Signature placed to another binding index");
        }
    }

    Finalize(pDeviceD3D12Impl);
}

RootSignatureD3D12::~RootSignatureD3D12()
{
    m_Cache.OnDestroyRootSig(this);
}

void RootSignatureD3D12::Finalize(RenderDeviceD3D12Impl* pDeviceD3D12Impl)
{
    VERIFY(m_pd3d12RootSignature == nullptr, "This root signature is already initialized");

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Uint32 TotalParams              = 0;
    Uint32 Totald3d12StaticSamplers = 0;
    Uint32 TotalDescriptorRanges    = 0;
    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        auto& pSignature = m_Signatures[s];
        if (pSignature == nullptr)
            continue;

        auto& RootParams = pSignature->m_RootParams;

        m_FirstRootIndex[s] = static_cast<Uint16>(TotalParams);
        TotalParams += RootParams.GetNumRootTables() + RootParams.GetNumRootViews();

        for (Uint32 rt = 0; rt < RootParams.GetNumRootTables(); ++rt)
        {
            const auto& RootTable = RootParams.GetRootTable(rt);
            TotalDescriptorRanges += RootTable.d3d12RootParam.DescriptorTable.NumDescriptorRanges;
        }

        for (Uint32 samp = 0, SampCount = pSignature->GetImmutableSamplerCount(); samp < SampCount; ++samp)
        {
            const auto& ImtblSam = pSignature->GetImmutableSamplerAttribs(samp);
            VERIFY_EXPR(ImtblSam.IsValid());

            Totald3d12StaticSamplers += ImtblSam.ArraySize;
        }
    }

    std::vector<D3D12_ROOT_PARAMETER, STDAllocatorRawMem<D3D12_ROOT_PARAMETER>>           d3d12Parameters(TotalParams, D3D12_ROOT_PARAMETER{}, STD_ALLOCATOR_RAW_MEM(D3D12_ROOT_PARAMETER, GetRawAllocator(), "Allocator for vector<D3D12_ROOT_PARAMETER>"));
    std::vector<D3D12_DESCRIPTOR_RANGE, STDAllocatorRawMem<D3D12_DESCRIPTOR_RANGE>>       d3d12DescrRanges(TotalDescriptorRanges, D3D12_DESCRIPTOR_RANGE{}, STD_ALLOCATOR_RAW_MEM(D3D12_DESCRIPTOR_RANGE, GetRawAllocator(), "Allocator for vector<D3D12_DESCRIPTOR_RANGE>"));
    std::vector<D3D12_STATIC_SAMPLER_DESC, STDAllocatorRawMem<D3D12_STATIC_SAMPLER_DESC>> d3d12StaticSamplers(STD_ALLOCATOR_RAW_MEM(D3D12_STATIC_SAMPLER_DESC, GetRawAllocator(), "Allocator for vector<D3D12_STATIC_SAMPLER_DESC>"));
    d3d12StaticSamplers.reserve(Totald3d12StaticSamplers);

    auto descr_range_it = d3d12DescrRanges.begin();

    Uint32 BaseRegisterSpace = 0;
    for (Uint32 sig = 0; sig < m_SignatureCount; ++sig)
    {
        m_FirstRegisterSpace[sig] = static_cast<Uint16>(BaseRegisterSpace);

        const auto& pSignature = m_Signatures[sig];
        if (pSignature == nullptr)
            continue;

        const auto& RootParams     = pSignature->m_RootParams;
        const auto  FirstRootIndex = m_FirstRootIndex[sig];

        Uint32 MaxSpaceUsed = 0;
        for (Uint32 rt = 0; rt < RootParams.GetNumRootTables(); ++rt)
        {
            const auto&  RootTable     = RootParams.GetRootTable(rt);
            const auto&  d3d12SrcParam = RootTable.d3d12RootParam;
            const auto&  d3d12SrcTbl   = d3d12SrcParam.DescriptorTable;
            const Uint32 RootIndex     = FirstRootIndex + RootTable.RootIndex;
            VERIFY(d3d12SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && d3d12SrcParam.DescriptorTable.NumDescriptorRanges > 0, "Non-empty descriptor table is expected");
            auto& d3d12DstParam = d3d12Parameters[RootIndex];
            auto& d3d12DstTbl   = d3d12DstParam.DescriptorTable;

            d3d12DstParam = d3d12SrcParam;
            memcpy(&*descr_range_it, d3d12SrcTbl.pDescriptorRanges, d3d12SrcTbl.NumDescriptorRanges * sizeof(D3D12_DESCRIPTOR_RANGE));
            d3d12DstTbl.pDescriptorRanges = &*descr_range_it;
            for (Uint32 r = 0; r < d3d12SrcTbl.NumDescriptorRanges; ++r, ++descr_range_it)
            {
                MaxSpaceUsed = std::max(MaxSpaceUsed, descr_range_it->RegisterSpace);
                descr_range_it->RegisterSpace += BaseRegisterSpace;
            }
        }

        for (Uint32 rv = 0; rv < RootParams.GetNumRootViews(); ++rv)
        {
            const auto&  RootView      = RootParams.GetRootView(rv);
            const auto&  d3d12SrcParam = RootView.d3d12RootParam;
            const Uint32 RootIndex     = FirstRootIndex + RootView.RootIndex;
            VERIFY(d3d12SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV, "Root CBV is expected");
            MaxSpaceUsed = std::max(MaxSpaceUsed, d3d12SrcParam.Descriptor.RegisterSpace);

            d3d12Parameters[RootIndex] = d3d12SrcParam;
            d3d12Parameters[RootIndex].Descriptor.RegisterSpace += BaseRegisterSpace;
        }

        for (Uint32 samp = 0, SampCount = pSignature->GetImmutableSamplerCount(); samp < SampCount; ++samp)
        {
            const auto& SampAttr = pSignature->GetImmutableSamplerAttribs(samp);
            const auto& ImtblSam = pSignature->GetImmutableSamplerDesc(samp);
            const auto& SamDesc  = ImtblSam.Desc;

            for (UINT ArrInd = 0; ArrInd < SampAttr.ArraySize; ++ArrInd)
            {
                auto ShaderVisibility = ShaderStagesToD3D12ShaderVisibility(ImtblSam.ShaderStages);

                d3d12StaticSamplers.emplace_back(
                    D3D12_STATIC_SAMPLER_DESC //
                    {
                        FilterTypeToD3D12Filter(SamDesc.MinFilter, SamDesc.MagFilter, SamDesc.MipFilter),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressU),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressV),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressW),
                        SamDesc.MipLODBias,
                        SamDesc.MaxAnisotropy,
                        ComparisonFuncToD3D12ComparisonFunc(SamDesc.ComparisonFunc),
                        BorderColorToD3D12StaticBorderColor(SamDesc.BorderColor),
                        SamDesc.MinLOD,
                        SamDesc.MaxLOD,
                        SampAttr.ShaderRegister + ArrInd,
                        SampAttr.RegisterSpace + BaseRegisterSpace,
                        ShaderVisibility //
                    }                    //
                );
            }
        }

        BaseRegisterSpace += MaxSpaceUsed + 1;
    }
    m_TotalSpacesUsed = BaseRegisterSpace;

    VERIFY_EXPR(descr_range_it == d3d12DescrRanges.end());

    rootSignatureDesc.NumParameters = static_cast<UINT>(d3d12Parameters.size());
    rootSignatureDesc.pParameters   = d3d12Parameters.size() ? d3d12Parameters.data() : nullptr;

    rootSignatureDesc.NumStaticSamplers = Totald3d12StaticSamplers;
    rootSignatureDesc.pStaticSamplers   = nullptr;
    if (!d3d12StaticSamplers.empty())
    {
        rootSignatureDesc.pStaticSamplers = d3d12StaticSamplers.data();
        VERIFY_EXPR(d3d12StaticSamplers.size() == Totald3d12StaticSamplers);
    }

    CComPtr<ID3DBlob> signature;
    CComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (error)
    {
        LOG_ERROR_MESSAGE("Error: ", (const char*)error->GetBufferPointer());
    }
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize root signature");

    auto* pd3d12Device = pDeviceD3D12Impl->GetD3D12Device();

    hr = pd3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(m_pd3d12RootSignature), reinterpret_cast<void**>(static_cast<ID3D12RootSignature**>(&m_pd3d12RootSignature)));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create root signature");
}


LocalRootSignatureD3D12::LocalRootSignatureD3D12(const char* pCBName, Uint32 ShaderRecordSize) :
    m_pName{pCBName},
    m_ShaderRecordSize{ShaderRecordSize}
{
    VERIFY_EXPR((m_pName != nullptr) == (m_ShaderRecordSize > 0));
}

bool LocalRootSignatureD3D12::IsShaderRecord(const D3DShaderResourceAttribs& CB)
{
    if (m_ShaderRecordSize > 0 &&
        CB.GetInputType() == D3D_SIT_CBUFFER &&
        strcmp(m_pName, CB.Name) == 0)
    {
        return true;
    }
    return false;
}

bool LocalRootSignatureD3D12::Create(ID3D12Device* pDevice, Uint32 RegisterSpace)
{
    if (m_ShaderRecordSize == 0)
        return false;

    VERIFY(m_RegisterSpace == ~0u || m_pd3d12RootSignature == nullptr, "This root signature is already created");

    m_RegisterSpace = RegisterSpace;

    D3D12_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc = {};
    D3D12_ROOT_PARAMETER      d3d12Params            = {};

    d3d12Params.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    d3d12Params.ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;
    d3d12Params.Constants.Num32BitValues = m_ShaderRecordSize / 4;
    d3d12Params.Constants.RegisterSpace  = m_RegisterSpace;
    d3d12Params.Constants.ShaderRegister = GetShaderRegister();

    d3d12RootSignatureDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    d3d12RootSignatureDesc.NumParameters = 1;
    d3d12RootSignatureDesc.pParameters   = &d3d12Params;

    CComPtr<ID3DBlob> signature;
    auto              hr = D3D12SerializeRootSignature(&d3d12RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize local root signature");

    hr = pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pd3d12RootSignature));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D12 local root signature");

    return true;
}


namespace
{
bool RootSignatureCompare(const RootSignatureD3D12* lhs, const RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>* ppSignatures, Uint32 SignatureCount) noexcept
{
    const Uint32 LSigCount = lhs->GetSignatureCount();
    const Uint32 RSigCount = SignatureCount;

    if (LSigCount != RSigCount)
        return false;

    for (Uint32 i = 0; i < LSigCount; ++i)
    {
        auto* pLSig = lhs->GetSignature(i);
        auto* pRSig = ppSignatures[i].RawPtr();

        if (pLSig == pRSig)
            continue;

        if ((pLSig == nullptr) != (pRSig == nullptr))
            return false;

        if (!pLSig->IsCompatibleWith(*pRSig))
            return false;
    }
    return true;
}
} // namespace

RootSignatureCacheD3D12::RootSignatureCacheD3D12(RenderDeviceD3D12Impl& DeviceD3D12Impl) :
    m_DeviceD3D12Impl{DeviceD3D12Impl}
{}

RootSignatureCacheD3D12::~RootSignatureCacheD3D12()
{
    std::lock_guard<std::mutex> Lock{m_RootSigCacheGuard};
    VERIFY(m_RootSigCache.empty(), "All pipeline layouts must be released");
}

RefCntAutoPtr<RootSignatureD3D12> RootSignatureCacheD3D12::GetRootSig(const RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>* ppSignatures, Uint32 SignatureCount)
{
    size_t Hash = 0;
    if (SignatureCount)
    {
        HashCombine(Hash, SignatureCount);
        for (Uint32 i = 0; i < SignatureCount; ++i)
        {
            if (ppSignatures[i] != nullptr)
            {
                VERIFY(ppSignatures[i]->GetDesc().BindingIndex == i, "Signature placed to another binding index");
                HashCombine(Hash, ppSignatures[i]->GetHash());
            }
            else
                HashCombine(Hash, 0);
        }
    }

    std::lock_guard<std::mutex> Lock{m_RootSigCacheGuard};

    auto Range = m_RootSigCache.equal_range(Hash);
    for (auto Iter = Range.first; Iter != Range.second; ++Iter)
    {
        if (auto Ptr = Iter->second.Lock())
        {
            if (RootSignatureCompare(Ptr, ppSignatures, SignatureCount))
                return Ptr;
        }
    }

    RefCntAutoPtr<RootSignatureD3D12> pNewRootSig;
    m_DeviceD3D12Impl.CreateRootSignature(ppSignatures, SignatureCount, Hash, &pNewRootSig);

    m_RootSigCache.emplace(Hash, pNewRootSig);
    return pNewRootSig;
}

void RootSignatureCacheD3D12::OnDestroyRootSig(RootSignatureD3D12* pRootSig)
{
    std::lock_guard<std::mutex> Lock{m_RootSigCacheGuard};

    auto Range = m_RootSigCache.equal_range(pRootSig->GetHash());

    for (auto Iter = Range.first; Iter != Range.second;)
    {
        if (!Iter->second.IsValid())
            Iter = m_RootSigCache.erase(Iter);
        else
            ++Iter;
    }
}

} // namespace Diligent
