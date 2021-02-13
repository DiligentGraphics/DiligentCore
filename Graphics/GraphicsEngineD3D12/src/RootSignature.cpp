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
                                       Uint32                                                   SignatureCount) :
    ObjectBase<IObject>{pRefCounters},
    m_SignatureCount{static_cast<Uint8>(SignatureCount)},
    m_pDeviceD3D12Impl{pDeviceD3D12Impl}
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

    if (m_SignatureCount > 0)
    {
        HashCombine(m_Hash, m_SignatureCount);
        for (Uint32 i = 0; i < m_SignatureCount; ++i)
        {
            if (m_Signatures[i] != nullptr)
                HashCombine(m_Hash, m_Signatures[i]->GetHash());
            else
                HashCombine(m_Hash, 0);
        }
    }
}

RootSignatureD3D12::~RootSignatureD3D12()
{
    m_pDeviceD3D12Impl->GetRootSignatureCache().OnDestroyRootSig(this);
}

void RootSignatureD3D12::Finalize()
{
    VERIFY(m_pd3d12RootSignature == nullptr, "This root signature is already initialized");

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Uint32 TotalParams              = 0;
    Uint32 TotalD3D12StaticSamplers = 0;

    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        auto& pSignature = m_Signatures[s];
        if (pSignature != nullptr)
        {
            auto& RootParams = pSignature->m_RootParams;

            m_FirstRootIndex[s] = TotalParams;
            TotalParams += RootParams.GetNumRootTables() + RootParams.GetNumRootViews();

            for (Uint32 samp = 0, SampCount = pSignature->GetImmutableSamplerCount(); samp < SampCount; ++samp)
            {
                const auto& ImtblSam = pSignature->GetImmutableSamplerAttribs(samp);
                VERIFY_EXPR(ImtblSam.IsAssigned());

                TotalD3D12StaticSamplers += ImtblSam.ArraySize;
            }
        }
    }

    std::vector<D3D12_ROOT_PARAMETER, STDAllocatorRawMem<D3D12_ROOT_PARAMETER>>           D3D12Parameters(TotalParams, D3D12_ROOT_PARAMETER{}, STD_ALLOCATOR_RAW_MEM(D3D12_ROOT_PARAMETER, GetRawAllocator(), "Allocator for vector<D3D12_ROOT_PARAMETER>"));
    std::vector<D3D12_STATIC_SAMPLER_DESC, STDAllocatorRawMem<D3D12_STATIC_SAMPLER_DESC>> D3D12StaticSamplers(STD_ALLOCATOR_RAW_MEM(D3D12_STATIC_SAMPLER_DESC, GetRawAllocator(), "Allocator for vector<D3D12_STATIC_SAMPLER_DESC>"));
    D3D12StaticSamplers.reserve(TotalD3D12StaticSamplers);

    for (Uint32 sig = 0; sig < m_SignatureCount; ++sig)
    {
        auto& pSignature = m_Signatures[sig];

        if (pSignature != nullptr)
        {
            const auto   FirstRootIndex = m_FirstRootIndex[sig];
            const Uint32 FirstSpace     = pSignature->GetBaseRegisterSpace();

            auto& RootParams = pSignature->m_RootParams;
            for (Uint32 rt = 0; rt < RootParams.GetNumRootTables(); ++rt)
            {
                const auto&  RootTable = RootParams.GetRootTable(rt);
                const auto&  SrcParam  = RootTable.d3d12RootParam;
                const Uint32 RootIndex = FirstRootIndex + RootTable.RootIndex;
                VERIFY(SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && SrcParam.DescriptorTable.NumDescriptorRanges > 0, "Non-empty descriptor table is expected");
                D3D12Parameters[RootIndex] = SrcParam;
            }

            for (Uint32 rv = 0; rv < RootParams.GetNumRootViews(); ++rv)
            {
                const auto&  RootView  = RootParams.GetRootView(rv);
                const auto&  SrcParam  = RootView.d3d12RootParam;
                const Uint32 RootIndex = FirstRootIndex + RootView.RootIndex;
                VERIFY(SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV, "Root CBV is expected");
                D3D12Parameters[RootIndex] = SrcParam;
            }

            for (Uint32 samp = 0, SampCount = pSignature->GetImmutableSamplerCount(); samp < SampCount; ++samp)
            {
                const auto& SampAttr = pSignature->GetImmutableSamplerAttribs(samp);
                const auto& ImtblSam = pSignature->GetImmutableSamplerDesc(samp);
                const auto& SamDesc  = ImtblSam.Desc;

                for (UINT ArrInd = 0; ArrInd < SampAttr.ArraySize; ++ArrInd)
                {
                    D3D12_SHADER_VISIBILITY ShaderVisibility = (ImtblSam.ShaderStages & (ImtblSam.ShaderStages - 1)) == 0 ?
                        ShaderTypeToD3D12ShaderVisibility(ImtblSam.ShaderStages) :
                        D3D12_SHADER_VISIBILITY_ALL;

                    D3D12StaticSamplers.emplace_back(
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
                            SampAttr.RegisterSpace + FirstSpace,
                            ShaderVisibility //
                        }                    //
                    );
                }
            }
        }
    }

    rootSignatureDesc.NumParameters = static_cast<UINT>(D3D12Parameters.size());
    rootSignatureDesc.pParameters   = D3D12Parameters.size() ? D3D12Parameters.data() : nullptr;

    rootSignatureDesc.NumStaticSamplers = TotalD3D12StaticSamplers;
    rootSignatureDesc.pStaticSamplers   = nullptr;
    if (!D3D12StaticSamplers.empty())
    {
        rootSignatureDesc.pStaticSamplers = D3D12StaticSamplers.data();
        VERIFY_EXPR(D3D12StaticSamplers.size() == TotalD3D12StaticSamplers);
    }

    CComPtr<ID3DBlob> signature;
    CComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (error)
    {
        LOG_ERROR_MESSAGE("Error: ", (const char*)error->GetBufferPointer());
    }
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize root signature");

    auto* pd3d12Device = m_pDeviceD3D12Impl->GetD3D12Device();

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

ID3D12RootSignature* LocalRootSignatureD3D12::Create(ID3D12Device* pDevice)
{
    if (m_ShaderRecordSize == 0)
        return nullptr;

    VERIFY(m_pd3d12RootSignature == nullptr, "This root signature is already created");

    D3D12_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc = {};
    D3D12_ROOT_PARAMETER      d3d12Params            = {};

    d3d12Params.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    d3d12Params.ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;
    d3d12Params.Constants.Num32BitValues = m_ShaderRecordSize / 4;
    d3d12Params.Constants.RegisterSpace  = GetRegisterSpace();
    d3d12Params.Constants.ShaderRegister = GetShaderRegister();

    d3d12RootSignatureDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    d3d12RootSignatureDesc.NumParameters = 1;
    d3d12RootSignatureDesc.pParameters   = &d3d12Params;

    CComPtr<ID3DBlob> signature;
    auto              hr = D3D12SerializeRootSignature(&d3d12RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize local root signature");

    hr = pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pd3d12RootSignature));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D12 local root signature");

    return m_pd3d12RootSignature;
}



bool RootSignatureCacheD3D12::RootSignatureCompare::operator()(const RootSignatureD3D12* lhs, const RootSignatureD3D12* rhs) const noexcept
{
    const Uint32 LSigCount = lhs->GetSignatureCount();
    const Uint32 RSigCount = rhs->GetSignatureCount();

    if (LSigCount != RSigCount)
        return false;

    for (Uint32 i = 0; i < LSigCount; ++i)
    {
        auto* pLSig = lhs->GetSignature(i);
        auto* pRSig = rhs->GetSignature(i);

        if (pLSig == pRSig)
            continue;

        if ((pLSig == nullptr) != (pRSig == nullptr))
            return false;

        if (!pLSig->IsCompatibleWith(*pRSig))
            return false;
    }
    return true;
}

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
    RefCntAutoPtr<RootSignatureD3D12> pNewRootSig;
    m_DeviceD3D12Impl.CreateRootSignature(ppSignatures, SignatureCount, &pNewRootSig);

    if (pNewRootSig == nullptr)
        return {};

    RefCntAutoPtr<RootSignatureD3D12> Result;
    bool                              Inserted = false;
    {
        std::lock_guard<std::mutex> Lock{m_RootSigCacheGuard};

        auto IterAndFlag = m_RootSigCache.insert(pNewRootSig.RawPtr());
        Inserted         = IterAndFlag.second;

        if (Inserted)
        {
            pNewRootSig->Finalize();
            Result = std::move(pNewRootSig);
        }
        else
            Result = *IterAndFlag.first;
    }
    return Result;
}

void RootSignatureCacheD3D12::OnDestroyRootSig(RootSignatureD3D12* pRootSig)
{
    std::lock_guard<std::mutex> Lock{m_RootSigCacheGuard};

    auto Iter = m_RootSigCache.find(pRootSig);
    if (Iter != m_RootSigCache.end() && *Iter == pRootSig)
        m_RootSigCache.erase(Iter);
}

} // namespace Diligent
