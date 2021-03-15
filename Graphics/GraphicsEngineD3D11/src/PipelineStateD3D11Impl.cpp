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
#include <array>
#include <d3dcompiler.h>

#include "PipelineStateD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "EngineMemory.h"
#include "DXBCUtils.hpp"

namespace Diligent
{
namespace
{
void VerifyResourceMerge(const PipelineStateDesc&        PSODesc,
                         const D3DShaderResourceAttribs& ExistingRes,
                         const D3DShaderResourceAttribs& NewResAttribs)
{
#define LOG_RESOURCE_MERGE_ERROR_AND_THROW(PropertyName)                                                          \
    LOG_ERROR_AND_THROW("Shader variable '", NewResAttribs.Name,                                                  \
                        "' is shared between multiple shaders in pipeline '", (PSODesc.Name ? PSODesc.Name : ""), \
                        "', but its " PropertyName " varies. A variable shared between multiple shaders "         \
                        "must be defined identically in all shaders. Either use separate variables for "          \
                        "different shader stages, change resource name or make sure that " PropertyName " is consistent.");

    if (ExistingRes.GetInputType() != NewResAttribs.GetInputType())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("input type");

    if (ExistingRes.GetSRVDimension() != NewResAttribs.GetSRVDimension())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("resource dimension");

    if (ExistingRes.BindCount != NewResAttribs.BindCount)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("array size");

    if (ExistingRes.IsMultisample() != NewResAttribs.IsMultisample())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("mutlisample state");
#undef LOG_RESOURCE_MERGE_ERROR_AND_THROW
}

} // namespace


RefCntAutoPtr<PipelineResourceSignatureD3D11Impl> PipelineStateD3D11Impl::CreateDefaultResourceSignature(
    const PipelineStateCreateInfo&       CreateInfo,
    const std::vector<ShaderD3D11Impl*>& Shaders)
{
    const auto& LayoutDesc = CreateInfo.PSODesc.ResourceLayout;

    struct UniqueResource
    {
        const D3DShaderResourceAttribs& Attribs;
        const SHADER_TYPE               ShaderStages;

        bool operator==(const UniqueResource& Res) const
        {
            return strcmp(Attribs.Name, Res.Attribs.Name) == 0 && ShaderStages == Res.ShaderStages;
        }

        struct Hasher
        {
            size_t operator()(const UniqueResource& Res) const
            {
                return ComputeHash(CStringHash<Char>{}(Res.Attribs.Name), Uint32{Res.ShaderStages});
            }
        };
    };
    std::unordered_set<UniqueResource, UniqueResource::Hasher> UniqueResources;

    std::vector<PipelineResourceDesc> Resources;
    const char*                       pCombinedSamplerSuffix = nullptr;

    for (auto* pShader : Shaders)
    {
        const auto& ShaderResources = *pShader->GetShaderResources();

        ShaderResources.ProcessResources(
            [&](const D3DShaderResourceAttribs& Attribs, Uint32) //
            {
                const char* const SamplerSuffix =
                    (ShaderResources.IsUsingCombinedTextureSamplers() && Attribs.GetInputType() == D3D_SIT_SAMPLER) ?
                    ShaderResources.GetCombinedSamplerSuffix() :
                    nullptr;

                // Use default variable type and current shader stages
                auto ShaderStages = pShader->GetDesc().ShaderType;
                auto VarType      = LayoutDesc.DefaultVariableType;

                const auto VarIndex = FindPipelineResourceLayoutVariable(LayoutDesc, Attribs.Name, ShaderStages, SamplerSuffix);
                if (VarIndex != InvalidPipelineResourceLayoutVariableIndex)
                {
                    const auto& Var = LayoutDesc.Variables[VarIndex];
                    // Use shader stages and variable type from the variable desc
                    ShaderStages = Var.ShaderStages;
                    VarType      = Var.Type;
                }

                auto IterAndAssigned = UniqueResources.emplace(UniqueResource{Attribs, ShaderStages});
                if (IterAndAssigned.second)
                {
                    SHADER_RESOURCE_TYPE    ResType = SHADER_RESOURCE_TYPE_UNKNOWN;
                    PIPELINE_RESOURCE_FLAGS Flags   = PIPELINE_RESOURCE_FLAG_UNKNOWN;
                    GetShaderResourceTypeAndFlags(Attribs, ResType, Flags);

                    if (Attribs.BindCount == 0)
                    {
                        LOG_ERROR_AND_THROW("Resource '", Attribs.Name, "' in shader '", pShader->GetDesc().Name, "' is a runtime-sized array. ",
                                            "Use explicit resource signature to specify the array size.");
                    }

                    Resources.emplace_back(ShaderStages, Attribs.Name, Attribs.BindCount, ResType, VarType, Flags);
                }
                else
                {
                    VerifyResourceMerge(CreateInfo.PSODesc, IterAndAssigned.first->Attribs, Attribs);
                }
            } //
        );

        // merge combined sampler suffixes
        if (ShaderResources.IsUsingCombinedTextureSamplers() && ShaderResources.GetNumSamplers() > 0)
        {
            if (pCombinedSamplerSuffix != nullptr)
            {
                if (strcmp(pCombinedSamplerSuffix, ShaderResources.GetCombinedSamplerSuffix()) != 0)
                    LOG_ERROR_AND_THROW("CombinedSamplerSuffix is not compatible between shaders");
            }
            else
            {
                pCombinedSamplerSuffix = ShaderResources.GetCombinedSamplerSuffix();
            }
        }
    }

    RefCntAutoPtr<PipelineResourceSignatureD3D11Impl> pSignature;
    if (Resources.size())
    {
        PipelineResourceSignatureDesc ResSignDesc;
        ResSignDesc.Resources                  = Resources.data();
        ResSignDesc.NumResources               = static_cast<Uint32>(Resources.size());
        ResSignDesc.ImmutableSamplers          = LayoutDesc.ImmutableSamplers;
        ResSignDesc.NumImmutableSamplers       = LayoutDesc.NumImmutableSamplers;
        ResSignDesc.BindingIndex               = 0;
        ResSignDesc.SRBAllocationGranularity   = CreateInfo.PSODesc.SRBAllocationGranularity;
        ResSignDesc.UseCombinedTextureSamplers = pCombinedSamplerSuffix != nullptr;
        ResSignDesc.CombinedSamplerSuffix      = pCombinedSamplerSuffix;

        constexpr bool bIsDeviceInternal = true;
        GetDevice()->CreatePipelineResourceSignature(ResSignDesc, pSignature.DblPtr<IPipelineResourceSignature>(), bIsDeviceInternal);

        if (!pSignature)
            LOG_ERROR_AND_THROW("Failed to create implicit resource signature for pipeline state '", (CreateInfo.PSODesc.Name ? CreateInfo.PSODesc.Name : ""), "'.");
    }

    return pSignature;
}

void PipelineStateD3D11Impl::InitResourceLayouts(const PipelineStateCreateInfo&       CreateInfo,
                                                 const std::vector<ShaderD3D11Impl*>& Shaders,
                                                 std::vector<CComPtr<ID3DBlob>>&      ByteCodes)
{
    if (m_UsingImplicitSignature)
    {
        VERIFY_EXPR(m_SignatureCount == 1);
        m_Signatures[0] = CreateDefaultResourceSignature(CreateInfo, Shaders);
        VERIFY_EXPR(!m_Signatures[0] || m_Signatures[0]->GetDesc().BindingIndex == 0);
    }

    // Verify that pipeline layout is compatible with shader resources and remap resource bindings.
    ByteCodes.resize(Shaders.size());
    for (size_t s = 0; s < Shaders.size(); ++s)
    {
        const auto* const pShader    = Shaders[s];
        const auto        ShaderType = pShader->GetDesc().ShaderType;
        auto*             pBytecode  = Shaders[s]->GetBytecode();

        PipelineResourceSignatureD3D11Impl::TBindingsPerStage BindingsPerStage = {};
        if (m_Desc.IsAnyGraphicsPipeline())
            BindingsPerStage[PSInd][D3D11_RESOURCE_RANGE_UAV] = GetGraphicsPipelineDesc().NumRenderTargets;

        ResourceBinding::TMap ResourceMap;
        for (Uint32 sign = 0; sign < m_SignatureCount; ++sign)
        {
            const PipelineResourceSignatureD3D11Impl* const pSignature = m_Signatures[sign];
            if (pSignature == nullptr)
                continue;

            VERIFY_EXPR(pSignature->GetDesc().BindingIndex == sign);
            pSignature->UpdateShaderResourceBindingMap(ResourceMap, ShaderType, BindingsPerStage);
            pSignature->ShiftBindings(BindingsPerStage);
        }

        ValidateShaderResources(pShader);

        CComPtr<ID3DBlob> pBlob;
        D3DCreateBlob(pBytecode->GetBufferSize(), &pBlob);
        memcpy(pBlob->GetBufferPointer(), pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());

        if (!DXBCUtils::RemapResourceBindings(ResourceMap, pBlob->GetBufferPointer(), pBlob->GetBufferSize()))
            LOG_ERROR_AND_THROW("Failed to remap resource bindings in shader '", pShader->GetDesc().Name, "'.");

        ByteCodes[s] = pBlob;
    }

#ifdef DILIGENT_DEVELOPMENT
    PipelineResourceSignatureD3D11Impl::TBindingsPerStage BindingsPerStage = {};

    if (m_Desc.IsAnyGraphicsPipeline())
        BindingsPerStage[PSInd][D3D11_RESOURCE_RANGE_UAV] = GetGraphicsPipelineDesc().NumRenderTargets;

    for (Uint32 sign = 0; sign < m_SignatureCount; ++sign)
    {
        const auto& pSignature = m_Signatures[sign];
        if (pSignature != nullptr)
            pSignature->ShiftBindings(BindingsPerStage);
    }

    for (Uint32 s = 0; s < BindingsPerStage.size(); ++s)
    {
        const auto& BindCount = BindingsPerStage[s];

        DEV_CHECK_ERR(BindCount[D3D11_RESOURCE_RANGE_CBV] <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
                      "Constant buffer count ", Uint32{BindCount[D3D11_RESOURCE_RANGE_CBV]}, " exceeds D3D11 limit ", D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
        DEV_CHECK_ERR(BindCount[D3D11_RESOURCE_RANGE_SRV] <= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
                      "SRV count ", Uint32{BindCount[D3D11_RESOURCE_RANGE_SRV]}, " exceeds D3D11 limit ", D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
        DEV_CHECK_ERR(BindCount[D3D11_RESOURCE_RANGE_SAMPLER] <= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,
                      "Sampler count ", Uint32{BindCount[D3D11_RESOURCE_RANGE_SAMPLER]}, " exceeds D3D11 limit ", D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
        DEV_CHECK_ERR(BindCount[D3D11_RESOURCE_RANGE_UAV] <= D3D11_PS_CS_UAV_REGISTER_COUNT,
                      "UAV count ", Uint32{BindCount[D3D11_RESOURCE_RANGE_UAV]}, " exceeds D3D11 limit ", D3D11_PS_CS_UAV_REGISTER_COUNT);
    }
#endif
}

template <typename PSOCreateInfoType>
void PipelineStateD3D11Impl::InitInternalObjects(const PSOCreateInfoType&        CreateInfo,
                                                 std::vector<CComPtr<ID3DBlob>>& ByteCodes)
{
    std::vector<ShaderD3D11Impl*> Shaders;
    ExtractShaders<ShaderD3D11Impl>(CreateInfo, Shaders);

    m_NumShaders = static_cast<Uint8>(Shaders.size());

    for (Uint32 s = 0; s < Shaders.size(); ++s)
    {
        auto ShaderType  = Shaders[s]->GetDesc().ShaderType;
        m_ShaderTypes[s] = static_cast<Uint8>(GetShaderTypeIndex(ShaderType));
        VERIFY_EXPR(ShaderType == GetShaderTypeFromIndex(m_ShaderTypes[s]));
    }

    FixedLinearAllocator MemPool{GetRawAllocator()};

    ReserveSpaceForPipelineDesc(CreateInfo, MemPool);

    MemPool.Reserve();

    InitializePipelineDesc(CreateInfo, MemPool);

    InitResourceLayouts(CreateInfo, Shaders, ByteCodes);

    auto* pDeviceD3D11 = GetDevice()->GetD3D11Device();
    for (Uint32 s = 0; s < Shaders.size(); ++s)
    {
        auto        ShaderType = Shaders[s]->GetDesc().ShaderType;
        const auto& pByteCode  = ByteCodes[s];
        switch (ShaderType)
        {
#define CREATE_SHADER(SHADER_NAME, ShaderName, pShader)                                                                                   \
    case SHADER_TYPE_##SHADER_NAME:                                                                                                       \
    {                                                                                                                                     \
        HRESULT hr = pDeviceD3D11->Create##ShaderName##Shader(pByteCode->GetBufferPointer(), pByteCode->GetBufferSize(), NULL, &pShader); \
        CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D11 shader");                                                                      \
        break;                                                                                                                            \
    }
            // clang-format off
            CREATE_SHADER(VERTEX,   Vertex,   m_pVS)
            CREATE_SHADER(PIXEL,    Pixel,    m_pPS)
            CREATE_SHADER(GEOMETRY, Geometry, m_pGS)
            CREATE_SHADER(DOMAIN,   Domain,   m_pDS)
            CREATE_SHADER(HULL,     Hull,     m_pHS)
            CREATE_SHADER(COMPUTE,  Compute,  m_pCS)
            // clang-format on
            default: LOG_ERROR_AND_THROW("Unknown shader type");
        }
#undef CREATE_SHADER
    }
}


PipelineStateD3D11Impl::PipelineStateD3D11Impl(IReferenceCounters*                    pRefCounters,
                                               RenderDeviceD3D11Impl*                 pRenderDeviceD3D11,
                                               const GraphicsPipelineStateCreateInfo& CreateInfo) :
    TPipelineStateBase{pRefCounters, pRenderDeviceD3D11, CreateInfo}
{
    try
    {
        std::vector<CComPtr<ID3DBlob>> ByteCodes;
        InitInternalObjects(CreateInfo, ByteCodes);

        if (m_pVS == nullptr)
            LOG_ERROR_AND_THROW("Vertex shader is null");

        auto& GraphicsPipeline = GetGraphicsPipelineDesc();
        auto* pDeviceD3D11     = pRenderDeviceD3D11->GetD3D11Device();

        D3D11_BLEND_DESC D3D11BSDesc = {};
        BlendStateDesc_To_D3D11_BLEND_DESC(GraphicsPipeline.BlendDesc, D3D11BSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateBlendState(&D3D11BSDesc, &m_pd3d11BlendState),
                               "Failed to create D3D11 blend state object");

        D3D11_RASTERIZER_DESC D3D11RSDesc = {};
        RasterizerStateDesc_To_D3D11_RASTERIZER_DESC(GraphicsPipeline.RasterizerDesc, D3D11RSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateRasterizerState(&D3D11RSDesc, &m_pd3d11RasterizerState),
                               "Failed to create D3D11 rasterizer state");

        D3D11_DEPTH_STENCIL_DESC D3D11DSSDesc = {};
        DepthStencilStateDesc_To_D3D11_DEPTH_STENCIL_DESC(GraphicsPipeline.DepthStencilDesc, D3D11DSSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateDepthStencilState(&D3D11DSSDesc, &m_pd3d11DepthStencilState),
                               "Failed to create D3D11 depth stencil state");

        // Create input layout
        const auto& InputLayout = GraphicsPipeline.InputLayout;
        if (InputLayout.NumElements > 0)
        {
            std::vector<D3D11_INPUT_ELEMENT_DESC, STDAllocatorRawMem<D3D11_INPUT_ELEMENT_DESC>> d311InputElements(STD_ALLOCATOR_RAW_MEM(D3D11_INPUT_ELEMENT_DESC, GetRawAllocator(), "Allocator for vector<D3D11_INPUT_ELEMENT_DESC>"));
            LayoutElements_To_D3D11_INPUT_ELEMENT_DESCs(InputLayout, d311InputElements);

            ID3DBlob* pVSByteCode = ByteCodes.front();
            CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateInputLayout(d311InputElements.data(), static_cast<UINT>(d311InputElements.size()), pVSByteCode->GetBufferPointer(), pVSByteCode->GetBufferSize(), &m_pd3d11InputLayout),
                                   "Failed to create the Direct3D11 input layout");
        }
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

PipelineStateD3D11Impl::PipelineStateD3D11Impl(IReferenceCounters*                   pRefCounters,
                                               RenderDeviceD3D11Impl*                pRenderDeviceD3D11,
                                               const ComputePipelineStateCreateInfo& CreateInfo) :
    TPipelineStateBase{pRefCounters, pRenderDeviceD3D11, CreateInfo}
{
    try
    {
        std::vector<CComPtr<ID3DBlob>> ByteCodes;
        InitInternalObjects(CreateInfo, ByteCodes);
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

PipelineStateD3D11Impl::~PipelineStateD3D11Impl()
{
    Destruct();
}

void PipelineStateD3D11Impl::Destruct()
{
    m_ShaderTypes = {};
    m_NumShaders  = 0;

    m_pd3d11BlendState        = nullptr;
    m_pd3d11RasterizerState   = nullptr;
    m_pd3d11DepthStencilState = nullptr;
    m_pd3d11InputLayout       = nullptr;
    m_pVS                     = nullptr;
    m_pPS                     = nullptr;
    m_pGS                     = nullptr;
    m_pDS                     = nullptr;
    m_pHS                     = nullptr;
    m_pCS                     = nullptr;

    TPipelineStateBase::Destruct();
}

IMPLEMENT_QUERY_INTERFACE(PipelineStateD3D11Impl, IID_PipelineStateD3D11, TPipelineStateBase)


bool PipelineStateD3D11Impl::IsCompatibleWith(const IPipelineState* pPSO) const
{
    if (!TPipelineStateBase::IsCompatibleWith(pPSO))
        return false;

    const auto& rhs = *ValidatedCast<const PipelineStateD3D11Impl>(pPSO);
    if (m_NumShaders != rhs.m_NumShaders || m_ShaderTypes != rhs.m_ShaderTypes)
        return false;

    return true;
}

SHADER_TYPE PipelineStateD3D11Impl::GetShaderStageType(Uint32 Index) const
{
    return GetShaderTypeFromIndex(m_ShaderTypes[Index]);
}

void PipelineStateD3D11Impl::ValidateShaderResources(const ShaderD3D11Impl* pShader)
{
    const auto& pShaderResources = pShader->GetShaderResources();
    const auto  ShaderType       = pShader->GetDesc().ShaderType;

#ifdef DILIGENT_DEVELOPMENT
    m_ShaderResources.emplace_back(pShaderResources);
#endif

    // Check compatibility between shader resources and resource signature.
    pShaderResources->ProcessResources(
        [&](const D3DShaderResourceAttribs& Attribs, Uint32) //
        {
#ifdef DILIGENT_DEVELOPMENT
            m_ResourceAttibutions.emplace_back();
#endif

            const auto IsSampler = Attribs.GetInputType() == D3D_SIT_SAMPLER;
            if (IsSampler && pShaderResources->IsUsingCombinedTextureSamplers())
                return;

#ifdef DILIGENT_DEVELOPMENT
            auto& ResAttribution = m_ResourceAttibutions.back();
#else
            ResourceAttribution ResAttribution;
#endif
            ResAttribution = GetResourceAttribution(Attribs.Name, ShaderType);
            if (!ResAttribution)
            {
                LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource '", Attribs.Name,
                                    "' that is not present in any pipeline resource signature used to create pipeline state '",
                                    m_Desc.Name, "'.");
            }

            SHADER_RESOURCE_TYPE    Type  = SHADER_RESOURCE_TYPE_UNKNOWN;
            PIPELINE_RESOURCE_FLAGS Flags = PIPELINE_RESOURCE_FLAG_UNKNOWN;
            GetShaderResourceTypeAndFlags(Attribs, Type, Flags);

            const auto* const pSignature = ResAttribution.pSignature;
            VERIFY_EXPR(pSignature != nullptr);

            if (ResAttribution.ResourceIndex != ResourceAttribution::InvalidResourceIndex)
            {
                const auto& ResDesc = pSignature->GetResourceDesc(ResAttribution.ResourceIndex);

                auto ResourceType = ResDesc.ResourceType;
                if (ResourceType == SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT)
                    ResourceType = SHADER_RESOURCE_TYPE_TEXTURE_SRV;
                if (Type != ResourceType)
                {
                    LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource with name '", Attribs.Name,
                                        "' and type '", GetShaderResourceTypeLiteralName(Type), "' that is not compatible with type '",
                                        GetShaderResourceTypeLiteralName(ResDesc.ResourceType), "' in pipeline resource signature '", pSignature->GetDesc().Name, "'.");
                }

                if ((Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER))
                {
                    LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource '", Attribs.Name,
                                        "' that is", ((Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                                        " labeled as formatted buffer, while the same resource specified by the pipeline resource signature '",
                                        pSignature->GetDesc().Name, "' is", ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                                        " labeled as such.");
                }

                VERIFY(Attribs.BindCount != 0,
                       "Runtime-sized array is not supported in Direct3D11, shader must not be compiled.");
                VERIFY((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0,
                       "Runtime-sized array flag is not supported in Direct3D11, this error must be handled by ValidatePipelineResourceSignatureDesc()");

                if (ResDesc.ArraySize < Attribs.BindCount)
                {
                    LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource '", Attribs.Name,
                                        "' whose array size (", Attribs.BindCount, ") is greater than the array size (",
                                        ResDesc.ArraySize, ") specified by the pipeline resource signature '", pSignature->GetDesc().Name, "'.");
                }
            }
            else if (ResAttribution.ImmutableSamplerIndex != ResourceAttribution::InvalidResourceIndex)
            {
                if (Type != SHADER_RESOURCE_TYPE_SAMPLER)
                {
                    LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource with name '", Attribs.Name,
                                        "' and type '", GetShaderResourceTypeLiteralName(Type),
                                        "' that is not compatible with immutable sampler defined in pipeline resource signature '",
                                        pSignature->GetDesc().Name, "'.");
                }
            }
            else
            {
                UNEXPECTED("Either immutable sampler or resource index should be valid");
            }
        } //
    );
}

#ifdef DILIGENT_DEVELOPMENT
void PipelineStateD3D11Impl::DvpVerifySRBResources(class ShaderResourceBindingD3D11Impl* pSRBs[], const TBindingsPerStage BaseBindings[], Uint32 NumSRBs) const
{
    // Verify SRB compatibility with this pipeline
    const auto        SignCount = GetResourceSignatureCount();
    TBindingsPerStage Bindings  = {};

    if (m_Desc.IsAnyGraphicsPipeline())
        Bindings[GetShaderTypeIndex(SHADER_TYPE_PIXEL)][D3D11_RESOURCE_RANGE_UAV] = static_cast<Uint8>(GetGraphicsPipelineDesc().NumRenderTargets);

    for (Uint32 sign = 0; sign < SignCount; ++sign)
    {
        // Get resource signature from the root signature
        const auto* pSignature = GetResourceSignature(sign);
        if (pSignature == nullptr || pSignature->GetTotalResourceCount() == 0)
            continue; // Skip null and empty signatures

        VERIFY_EXPR(pSignature->GetDesc().BindingIndex == sign);
        const auto* const pSRB = pSRBs[sign];
        if (pSRB == nullptr)
        {
            LOG_ERROR_MESSAGE("Pipeline state '", m_Desc.Name, "' requires SRB at index ", sign, " but none is bound in the device context.");
            continue;
        }

        const auto* const pSRBSign = pSRB->GetSignature();
        if (!pSignature->IsCompatibleWith(pSRBSign))
        {
            LOG_ERROR_MESSAGE("Shader resource binding at index ", sign, " with signature '", pSRBSign->GetDesc().Name,
                              "' is not compatible with pipeline layout in current pipeline '", m_Desc.Name, "'.");
        }

        DEV_CHECK_ERR(Bindings == BaseBindings[sign],
                      "Bound resources has incorrect base binding indices, this may indicate a bug in resource signature compatibility comparison.");

        pSignature->ShiftBindings(Bindings);
    }

    auto attrib_it = m_ResourceAttibutions.begin();
    for (const auto& pResources : m_ShaderResources)
    {
        pResources->ProcessResources(
            [&](const D3DShaderResourceAttribs& Attribs, Uint32) //
            {
                if (*attrib_it && !attrib_it->IsImmutableSampler())
                {
                    if (attrib_it->SignatureIndex >= NumSRBs || pSRBs[attrib_it->SignatureIndex] == nullptr)
                    {
                        LOG_ERROR_MESSAGE("No resource is bound to variable '", Attribs.Name, "' in shader '", pResources->GetShaderName(),
                                          "' of PSO '", m_Desc.Name, "': SRB at index ", attrib_it->SignatureIndex, " is not bound in the context.");
                        return;
                    }

                    const auto& SRBCache = pSRBs[attrib_it->SignatureIndex]->GetResourceCache();
                    attrib_it->pSignature->DvpValidateCommittedResource(Attribs, attrib_it->ResourceIndex, SRBCache, pResources->GetShaderName(), m_Desc.Name);
                }
                ++attrib_it;
            } //
        );
    }
    VERIFY_EXPR(attrib_it == m_ResourceAttibutions.end());
}

#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
