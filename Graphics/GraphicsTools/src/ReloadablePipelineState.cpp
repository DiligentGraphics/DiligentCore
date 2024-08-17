/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "ReloadablePipelineState.hpp"

#include <unordered_set>
#include <vector>

#include "RenderStateCacheImpl.hpp"
#include "ReloadableShader.hpp"

namespace Diligent
{

constexpr INTERFACE_ID ReloadablePipelineState::IID_InternalImpl;


template <typename CreateInfoType>
struct ReloadablePipelineState::CreateInfoWrapperBase : DynamicHeapObjectBase
{
    CreateInfoWrapperBase(const CreateInfoType& CI) :
        m_CI{CI},
        m_Variables{CI.PSODesc.ResourceLayout.Variables, CI.PSODesc.ResourceLayout.Variables + CI.PSODesc.ResourceLayout.NumVariables},
        m_ImtblSamplers{CI.PSODesc.ResourceLayout.ImmutableSamplers, CI.PSODesc.ResourceLayout.ImmutableSamplers + CI.PSODesc.ResourceLayout.NumImmutableSamplers},
        m_ppSignatures{CI.ppResourceSignatures, CI.ppResourceSignatures + CI.ResourceSignaturesCount}
    {
        if (CI.PSODesc.Name != nullptr)
            m_CI.PSODesc.Name = m_Strings.emplace(CI.PSODesc.Name).first->c_str();

        for (auto& Var : m_Variables)
            Var.Name = m_Strings.emplace(Var.Name).first->c_str();
        for (auto& ImtblSam : m_ImtblSamplers)
            ImtblSam.SamplerOrTextureName = m_Strings.emplace(ImtblSam.SamplerOrTextureName).first->c_str();

        m_CI.PSODesc.ResourceLayout.Variables         = m_Variables.data();
        m_CI.PSODesc.ResourceLayout.ImmutableSamplers = m_ImtblSamplers.data();

        m_CI.ppResourceSignatures = !m_ppSignatures.empty() ? m_ppSignatures.data() : nullptr;
        for (auto* pSign : m_ppSignatures)
            m_Objects.emplace_back(pSign);

        m_Objects.emplace_back(m_CI.pPSOCache);

        // Replace shaders with reloadable shaders
        ProcessPsoCreateInfoShaders(m_CI,
                                    [&](IShader*& pShader) {
                                        AddShader(pShader);
                                    });
    }

    CreateInfoWrapperBase(const CreateInfoWrapperBase&) = delete;
    CreateInfoWrapperBase(CreateInfoWrapperBase&&)      = delete;
    CreateInfoWrapperBase& operator=(const CreateInfoWrapperBase&) = delete;
    CreateInfoWrapperBase& operator=(CreateInfoWrapperBase&&) = delete;

    const CreateInfoType& Get() const
    {
        return m_CI;
    }

    operator const CreateInfoType&() const
    {
        return m_CI;
    }

    operator CreateInfoType&()
    {
        return m_CI;
    }

    void AddShader(IShader* pShader)
    {
        if (pShader == nullptr)
            return;

        if (!RefCntAutoPtr<IShader>{pShader, ReloadableShader::IID_InternalImpl})
        {
            const auto* Name = pShader->GetDesc().Name;
            LOG_WARNING_MESSAGE("Shader '", (Name ? Name : "<unnamed>"),
                                "' is not a reloadable shader. To enable hot pipeline state reload, all shaders must be created through the render state cache.");
        }

        m_Objects.emplace_back(pShader);
    }

protected:
    CreateInfoType m_CI;

    std::unordered_set<std::string>          m_Strings;
    std::vector<ShaderResourceVariableDesc>  m_Variables;
    std::vector<ImmutableSamplerDesc>        m_ImtblSamplers;
    std::vector<IPipelineResourceSignature*> m_ppSignatures;
    std::vector<RefCntAutoPtr<IObject>>      m_Objects;
};


template <>
struct ReloadablePipelineState::CreateInfoWrapper<GraphicsPipelineStateCreateInfo> : CreateInfoWrapperBase<GraphicsPipelineStateCreateInfo>
{
    CreateInfoWrapper(const GraphicsPipelineStateCreateInfo& CI) :
        CreateInfoWrapperBase<GraphicsPipelineStateCreateInfo>{CI},
        m_LayoutElements{CI.GraphicsPipeline.InputLayout.LayoutElements, CI.GraphicsPipeline.InputLayout.LayoutElements + CI.GraphicsPipeline.InputLayout.NumElements}
    {
        m_Objects.emplace_back(CI.GraphicsPipeline.pRenderPass);

        for (auto& Elem : m_LayoutElements)
            Elem.HLSLSemantic = m_Strings.emplace(Elem.HLSLSemantic != nullptr ? Elem.HLSLSemantic : LayoutElement{}.HLSLSemantic).first->c_str();

        m_CI.GraphicsPipeline.InputLayout.LayoutElements = m_LayoutElements.data();
    }

private:
    std::vector<LayoutElement> m_LayoutElements;
};

template <>
struct ReloadablePipelineState::CreateInfoWrapper<ComputePipelineStateCreateInfo> : CreateInfoWrapperBase<ComputePipelineStateCreateInfo>
{
    CreateInfoWrapper(const ComputePipelineStateCreateInfo& CI) :
        CreateInfoWrapperBase<ComputePipelineStateCreateInfo>{CI}
    {
    }
};

template <>
struct ReloadablePipelineState::CreateInfoWrapper<TilePipelineStateCreateInfo> : CreateInfoWrapperBase<TilePipelineStateCreateInfo>
{
    CreateInfoWrapper(const TilePipelineStateCreateInfo& CI) :
        CreateInfoWrapperBase<TilePipelineStateCreateInfo>{CI}
    {
    }
};

template <>
struct ReloadablePipelineState::CreateInfoWrapper<RayTracingPipelineStateCreateInfo> : CreateInfoWrapperBase<RayTracingPipelineStateCreateInfo>
{
    CreateInfoWrapper(const RayTracingPipelineStateCreateInfo& CI) :
        CreateInfoWrapperBase<RayTracingPipelineStateCreateInfo>{CI},
        // clang-format off
        m_pGeneralShaders      {CI.pGeneralShaders,       CI.pGeneralShaders       + CI.GeneralShaderCount},
        m_pTriangleHitShaders  {CI.pTriangleHitShaders,   CI.pTriangleHitShaders   + CI.TriangleHitShaderCount},
        m_pProceduralHitShaders{CI.pProceduralHitShaders, CI.pProceduralHitShaders + CI.ProceduralHitShaderCount}
    // clang-format on
    {
        m_CI.pGeneralShaders       = m_pGeneralShaders.data();
        m_CI.pTriangleHitShaders   = m_pTriangleHitShaders.data();
        m_CI.pProceduralHitShaders = m_pProceduralHitShaders.data();

        if (m_CI.pShaderRecordName != nullptr)
            m_CI.pShaderRecordName = m_Strings.emplace(m_CI.pShaderRecordName).first->c_str();

        // Replace shaders with reloadable shaders
        ProcessRtPsoCreateInfoShaders(m_pGeneralShaders, m_pTriangleHitShaders, m_pProceduralHitShaders,
                                      [&](IShader*& pShader) {
                                          AddShader(pShader);
                                      });
    }

private:
    std::vector<RayTracingGeneralShaderGroup>       m_pGeneralShaders;
    std::vector<RayTracingTriangleHitShaderGroup>   m_pTriangleHitShaders;
    std::vector<RayTracingProceduralHitShaderGroup> m_pProceduralHitShaders;
};

ReloadablePipelineState::ReloadablePipelineState(IReferenceCounters*            pRefCounters,
                                                 RenderStateCacheImpl*          pStateCache,
                                                 IPipelineState*                pPipeline,
                                                 const PipelineStateCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    m_pStateCache{pStateCache},
    m_pPipeline{pPipeline},
    m_Type{CreateInfo.PSODesc.PipelineType}
{
    static_assert(PIPELINE_TYPE_COUNT == 5, "Did you add a new pipeline type? You may need to handle it here.");
    switch (CreateInfo.PSODesc.PipelineType)
    {
        case PIPELINE_TYPE_GRAPHICS:
        case PIPELINE_TYPE_MESH:
            m_pCreateInfo = std::make_unique<CreateInfoWrapper<GraphicsPipelineStateCreateInfo>>(static_cast<const GraphicsPipelineStateCreateInfo&>(CreateInfo));
            break;

        case PIPELINE_TYPE_COMPUTE:
            m_pCreateInfo = std::make_unique<CreateInfoWrapper<ComputePipelineStateCreateInfo>>(static_cast<const ComputePipelineStateCreateInfo&>(CreateInfo));
            break;

        case PIPELINE_TYPE_RAY_TRACING:
            m_pCreateInfo = std::make_unique<CreateInfoWrapper<RayTracingPipelineStateCreateInfo>>(static_cast<const RayTracingPipelineStateCreateInfo&>(CreateInfo));
            break;

        case PIPELINE_TYPE_TILE:
            m_pCreateInfo = std::make_unique<CreateInfoWrapper<TilePipelineStateCreateInfo>>(static_cast<const TilePipelineStateCreateInfo&>(CreateInfo));
            break;

        default:
            UNEXPECTED("Unexpected pipeline type");
    }
}


ReloadablePipelineState::~ReloadablePipelineState()
{
}

void ReloadablePipelineState::QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)
{
    if (ppInterface == nullptr)
        return;
    DEV_CHECK_ERR(*ppInterface == nullptr, "Overwriting reference to an existing object may result in memory leaks");
    *ppInterface = nullptr;

    if (IID == IID_InternalImpl || IID == IID_PipelineState || IID == IID_DeviceObject || IID == IID_Unknown)
    {
        *ppInterface = this;
        (*ppInterface)->AddRef();
    }
    else
    {
        // This will handle implementation-specific interfaces such as PipelineStateD3D11Impl::IID_InternalImpl,
        // PipelineStateD3D12Impl::IID_InternalImpl, etc. requested by e.g. device context implementations
        // (DeviceContextD3D11Impl::SetPipelineState, DeviceContextD3D12Impl::SetPipelineState, etc.)
        m_pPipeline->QueryInterface(IID, ppInterface);
    }
}

template <typename CreateInfoType>
void ModifyPsoCreateInfo(CreateInfoType& PsoCreateInfo, ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData)
{
}

template <>
void ModifyPsoCreateInfo(GraphicsPipelineStateCreateInfo& PsoCreateInfo, ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData)
{
    if (ReloadGraphicsPipeline != nullptr)
    {
        ReloadGraphicsPipeline(PsoCreateInfo.PSODesc.Name, PsoCreateInfo.GraphicsPipeline, pUserData);
    }
}

template <typename CreateInfoType>
bool ReloadablePipelineState::Reload(ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData)
{
    auto& CreateInfo = static_cast<CreateInfoWrapper<CreateInfoType>&>(*m_pCreateInfo);
    ModifyPsoCreateInfo<CreateInfoType>(static_cast<CreateInfoType&>(CreateInfo), ReloadGraphicsPipeline, pUserData);

    RefCntAutoPtr<IPipelineState> pNewPSO;

    // Note that the create info struct references reloadable shaders, so that the pipeline will use the updated shaders
    const auto FoundInCache = m_pStateCache->CreatePipelineStateInternal(static_cast<const CreateInfoType&>(CreateInfo), &pNewPSO);

    if (pNewPSO)
    {
        if (m_pPipeline != pNewPSO)
        {
            const auto SrcSignCount = m_pPipeline->GetResourceSignatureCount();
            const auto DstSignCount = pNewPSO->GetResourceSignatureCount();
            if (SrcSignCount == DstSignCount)
            {
                for (Uint32 s = 0; s < SrcSignCount; ++s)
                {
                    auto* pSrcSign = m_pPipeline->GetResourceSignature(s);
                    auto* pDstSign = pNewPSO->GetResourceSignature(s);
                    if (pSrcSign != pDstSign)
                        pSrcSign->CopyStaticResources(pDstSign);
                }
            }
            else
            {
                UNEXPECTED("The number of resource signatures in old pipeline (", SrcSignCount, ") does not match the number of signatures in new pipeline (", DstSignCount, ")");
            }
            m_pPipeline = pNewPSO;
        }
    }
    else
    {
        const auto* Name = CreateInfo.Get().PSODesc.Name;
        LOG_ERROR_MESSAGE("Failed to reload pipeline state '", (Name ? Name : "<unnamed>"), "'.");
    }
    return !FoundInCache;
}


bool ReloadablePipelineState::Reload(ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData)
{
    static_assert(PIPELINE_TYPE_COUNT == 5, "Did you add a new pipeline type? You may need to handle it here.");
    // Note that all shaders in Create Info are reloadable shaders, so they will automatically redirect all calls
    // to the updated internal shader
    switch (m_Type)
    {
        case PIPELINE_TYPE_GRAPHICS:
        case PIPELINE_TYPE_MESH:
            return Reload<GraphicsPipelineStateCreateInfo>(ReloadGraphicsPipeline, pUserData);

        case PIPELINE_TYPE_COMPUTE:
            return Reload<ComputePipelineStateCreateInfo>(ReloadGraphicsPipeline, pUserData);

        case PIPELINE_TYPE_RAY_TRACING:
            return Reload<RayTracingPipelineStateCreateInfo>(ReloadGraphicsPipeline, pUserData);

        case PIPELINE_TYPE_TILE:
            return Reload<TilePipelineStateCreateInfo>(ReloadGraphicsPipeline, pUserData);

        default:
            UNEXPECTED("Unexpected pipeline type");
            return false;
    }
}

void ReloadablePipelineState::Create(RenderStateCacheImpl*          pStateCache,
                                     IPipelineState*                pPipeline,
                                     const PipelineStateCreateInfo& CreateInfo,
                                     IPipelineState**               ppReloadablePipeline)
{
    try
    {
        RefCntAutoPtr<ReloadablePipelineState> pReloadablePipeline{MakeNewRCObj<ReloadablePipelineState>()(pStateCache, pPipeline, CreateInfo)};
        *ppReloadablePipeline = pReloadablePipeline.Detach();
    }
    catch (...)
    {
        LOG_ERROR("Failed to create reloadable pipeline state '", (CreateInfo.PSODesc.Name ? CreateInfo.PSODesc.Name : "<unnamed>"), "'.");
    }
}

} // namespace Diligent
