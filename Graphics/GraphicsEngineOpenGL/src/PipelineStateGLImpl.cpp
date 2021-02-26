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
#include "PipelineStateGLImpl.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "ShaderGLImpl.hpp"
#include "ShaderResourceBindingGLImpl.hpp"
#include "EngineMemory.h"
#include "DeviceContextGLImpl.hpp"

namespace Diligent
{

void PipelineStateGLImpl::CreateDefaultSignature(const PipelineStateCreateInfo& CreateInfo,
                                                 const TShaderStages&           ShaderStages,
                                                 SHADER_TYPE                    ActiveStages,
                                                 IPipelineResourceSignature**   ppSignature)
{
    std::vector<PipelineResourceDesc> Resources;

    const auto&       LayoutDesc     = CreateInfo.PSODesc.ResourceLayout;
    const auto        DefaultVarType = LayoutDesc.DefaultVariableType;
    ShaderResourcesGL ProgramResources;

    struct UniqueResource
    {
        const ShaderResourcesGL::GLResourceAttribs& Attribs;
        const SHADER_TYPE                           ShaderStages;

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

    const auto HandleResource = [&](const ShaderResourcesGL::GLResourceAttribs& Attribs, PIPELINE_RESOURCE_FLAGS Flags) //
    {
        PipelineResourceDesc ResDesc = {};

        ResDesc.Name         = Attribs.Name;
        ResDesc.ShaderStages = Attribs.ShaderStages;
        ResDesc.ArraySize    = Attribs.ArraySize;
        ResDesc.ResourceType = Attribs.ResourceType;
        ResDesc.VarType      = DefaultVarType;
        ResDesc.Flags        = Flags;

        if (m_IsProgramPipelineSupported)
        {
            const auto VarIndex = FindPipelineResourceLayoutVariable(LayoutDesc, Attribs.Name, ResDesc.ShaderStages, nullptr);
            if (VarIndex != InvalidPipelineResourceLayoutVariableIndex)
            {
                const auto& Var      = LayoutDesc.Variables[VarIndex];
                ResDesc.ShaderStages = Var.ShaderStages;
                ResDesc.VarType      = Var.Type;
            }

            auto IterAndAssigned = UniqueResources.emplace(UniqueResource{Attribs, ResDesc.ShaderStages});
            if (IterAndAssigned.second)
            {
                Resources.push_back(ResDesc);
            }
            else
            {
                DEV_CHECK_ERR(IterAndAssigned.first->Attribs.ResourceType == Attribs.ResourceType,
                              "Shader variable '", Attribs.Name,
                              "' exists in multiple shaders from the same shader stage, but its type is not consistent between "
                              "shaders. All variables with the same name from the same shader stage must have the same type.");
            }
        }
        else
        {
            for (Uint32 i = 0; i < LayoutDesc.NumVariables; ++i)
            {
                const auto& Var = LayoutDesc.Variables[i];
                if ((Var.ShaderStages & Attribs.ShaderStages) != 0 &&
                    std::strcmp(Attribs.Name, Var.Name) == 0)
                {
                    ResDesc.VarType = Var.Type;
                    break;
                }
            }
            Resources.push_back(ResDesc);
        }
    };
    const auto HandleUB = [&](const ShaderResourcesGL::UniformBufferInfo& Attribs) {
        HandleResource(Attribs, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };
    const auto HandleTexture = [&](const ShaderResourcesGL::TextureInfo& Attribs) {
        HandleResource(Attribs, Attribs.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV ? PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER : PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER);
    };
    const auto HandleImage = [&](const ShaderResourcesGL::ImageInfo& Attribs) {
        HandleResource(Attribs, Attribs.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV ? PIPELINE_RESOURCE_FLAG_UNKNOWN : PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER);
    };
    const auto HandleSB = [&](const ShaderResourcesGL::StorageBlockInfo& Attribs) {
        HandleResource(Attribs, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };

    if (m_IsProgramPipelineSupported)
    {
        for (size_t i = 0; i < ShaderStages.size(); ++i)
        {
            auto* pShaderGL = ShaderStages[i].pShader;
            pShaderGL->GetShaderResources()->ProcessConstResources(HandleUB, HandleTexture, HandleImage, HandleSB);
        }
    }
    else
    {
        auto pImmediateCtx = m_pDevice->GetImmediateContext();
        VERIFY_EXPR(pImmediateCtx);
        VERIFY_EXPR(m_GLPrograms[0] != 0);

        ProgramResources.LoadUniforms(ActiveStages, m_GLPrograms[0], pImmediateCtx.RawPtr<DeviceContextGLImpl>()->GetContextState());
        ProgramResources.ProcessConstResources(HandleUB, HandleTexture, HandleImage, HandleSB);
    }

    if (Resources.size())
    {
        String SignName = String{"Implicit signature for PSO '"} + m_Desc.Name + '\'';

        PipelineResourceSignatureDesc ResSignDesc = {};

        ResSignDesc.Name                       = SignName.c_str();
        ResSignDesc.Resources                  = Resources.data();
        ResSignDesc.NumResources               = static_cast<Uint32>(Resources.size());
        ResSignDesc.ImmutableSamplers          = LayoutDesc.ImmutableSamplers;
        ResSignDesc.NumImmutableSamplers       = LayoutDesc.NumImmutableSamplers;
        ResSignDesc.BindingIndex               = 0;
        ResSignDesc.SRBAllocationGranularity   = CreateInfo.PSODesc.SRBAllocationGranularity;
        ResSignDesc.UseCombinedTextureSamplers = true;

        GetDevice()->CreatePipelineResourceSignature(ResSignDesc, ppSignature, true);

        if (*ppSignature == nullptr)
            LOG_ERROR_AND_THROW("Failed to create resource signature for pipeline state");
    }
}

void PipelineStateGLImpl::InitResourceLayouts(const PipelineStateCreateInfo& CreateInfo,
                                              const TShaderStages&           ShaderStages,
                                              SHADER_TYPE                    ActiveStages)
{
    const Uint32                              SignatureCount = CreateInfo.ResourceSignaturesCount;
    RefCntAutoPtr<IPipelineResourceSignature> pImplicitSignature;

    if (SignatureCount == 0 || CreateInfo.ppResourceSignatures == nullptr)
    {
        CreateDefaultSignature(CreateInfo, ShaderStages, ActiveStages, &pImplicitSignature);
        if (pImplicitSignature != nullptr)
        {
            VERIFY_EXPR(pImplicitSignature->GetDesc().BindingIndex == 0);
            m_Signatures[0]  = ValidatedCast<PipelineResourceSignatureGLImpl>(pImplicitSignature.RawPtr());
            m_SignatureCount = 1;
        }
    }
    else
    {
        const auto MaxBindingIndex =
            PipelineResourceSignatureGLImpl::CopyResourceSignatures(CreateInfo.PSODesc.PipelineType, SignatureCount, CreateInfo.ppResourceSignatures,
                                                                    m_Signatures.data(), m_Signatures.size());
        m_SignatureCount = static_cast<decltype(m_SignatureCount)>(MaxBindingIndex + 1);
        VERIFY_EXPR(m_SignatureCount == MaxBindingIndex + 1);
    }

    // Apply resource bindings to programs.
    auto& CtxState = m_pDevice->GetImmediateContext().RawPtr<DeviceContextGLImpl>()->GetContextState();

    PipelineResourceSignatureGLImpl::TBindings Bindings = {};

    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        const auto& pSignature = m_Signatures[s];
        if (pSignature == nullptr)
            continue;

        if (m_IsProgramPipelineSupported)
        {
            for (Uint32 p = 0; p < m_NumPrograms; ++p)
                pSignature->ApplyBindings(m_GLPrograms[p], CtxState, GetShaderStageType(p), Bindings);
        }
        else
        {
            pSignature->ApplyBindings(m_GLPrograms[0], CtxState, ActiveStages, Bindings);
        }
        pSignature->AddBindings(Bindings);
    }

#ifdef DILIGENT_DEVELOPMENT
    const auto& Limits = GetDevice()->GetDeviceLimits();

    DEV_CHECK_ERR(Bindings[BINDING_RANGE_UNIFORM_BUFFER] <= static_cast<Uint32>(Limits.MaxUniformBlocks),
                  "Number of bindings in range '", GetBindingRangeName(BINDING_RANGE_UNIFORM_BUFFER), "' is greater than maximum allowed (", Limits.MaxUniformBlocks, ").");
    DEV_CHECK_ERR(Bindings[BINDING_RANGE_TEXTURE] <= static_cast<Uint32>(Limits.MaxTextureUnits),
                  "Number of bindings in range '", GetBindingRangeName(BINDING_RANGE_TEXTURE), "' is greater than maximum allowed (", Limits.MaxTextureUnits, ").");
    DEV_CHECK_ERR(Bindings[BINDING_RANGE_STORAGE_BUFFER] <= static_cast<Uint32>(Limits.MaxStorageBlock),
                  "Number of bindings in range '", GetBindingRangeName(BINDING_RANGE_STORAGE_BUFFER), "' is greater than maximum allowed (", Limits.MaxStorageBlock, ").");
    DEV_CHECK_ERR(Bindings[BINDING_RANGE_IMAGE] <= static_cast<Uint32>(Limits.MaxImagesUnits),
                  "Number of bindings in range '", GetBindingRangeName(BINDING_RANGE_IMAGE), "' is greater than maximum allowed (", Limits.MaxImagesUnits, ").");

    if (m_IsProgramPipelineSupported)
    {
        for (size_t i = 0; i < ShaderStages.size(); ++i)
        {
            auto* pShaderGL = ShaderStages[i].pShader;
            DvpValidateShaderResources(pShaderGL->GetShaderResources(), pShaderGL->GetDesc().Name, pShaderGL->GetDesc().ShaderType);
        }
    }
    else
    {
        auto pImmediateCtx = m_pDevice->GetImmediateContext();
        VERIFY_EXPR(pImmediateCtx);
        VERIFY_EXPR(m_GLPrograms[0] != 0);

        std::unique_ptr<ShaderResourcesGL> pResources{new ShaderResourcesGL{}};
        pResources->LoadUniforms(ActiveStages, m_GLPrograms[0], pImmediateCtx.RawPtr<DeviceContextGLImpl>()->GetContextState());

        std::shared_ptr<const ShaderResourcesGL> pShaderResources{pResources.release()};
        DvpValidateShaderResources(pShaderResources, m_Desc.Name, ActiveStages);
    }
#endif
}

template <typename PSOCreateInfoType>
void PipelineStateGLImpl::InitInternalObjects(const PSOCreateInfoType& CreateInfo, const TShaderStages& ShaderStages)
{
    const auto& deviceCaps = GetDevice()->GetDeviceCaps();
    VERIFY(deviceCaps.DevType != RENDER_DEVICE_TYPE_UNDEFINED, "Device caps are not initialized");

    m_IsProgramPipelineSupported = deviceCaps.Features.SeparablePrograms != DEVICE_FEATURE_STATE_DISABLED;

    FixedLinearAllocator MemPool{GetRawAllocator()};

    ReserveSpaceForPipelineDesc(CreateInfo, MemPool);
    MemPool.AddSpace<GLProgramObj>(m_IsProgramPipelineSupported ? ShaderStages.size() : 1);

    MemPool.Reserve();

    InitializePipelineDesc(CreateInfo, MemPool);

    // Get active shader stages.
    SHADER_TYPE ActiveStages = SHADER_TYPE_UNKNOWN;
    for (auto& Stage : ShaderStages)
    {
        const auto ShaderType = Stage.pShader->GetDesc().ShaderType;
        VERIFY((ActiveStages & ShaderType) == 0, "Shader stage ", GetShaderTypeLiteralName(ShaderType), " is already active");
        ActiveStages |= ShaderType;
    }

    // Create programs.
    if (m_IsProgramPipelineSupported)
    {
        m_GLPrograms = MemPool.ConstructArray<GLProgramObj>(ShaderStages.size(), false);
        for (size_t i = 0; i < ShaderStages.size(); ++i)
        {
            auto* pShaderGL  = ShaderStages[i].pShader;
            m_GLPrograms[i]  = GLProgramObj{ShaderGLImpl::LinkProgram(&ShaderStages[i], 1, true)};
            m_ShaderTypes[i] = pShaderGL->GetDesc().ShaderType;
        }
        m_NumPrograms = static_cast<Uint8>(ShaderStages.size());
    }
    else
    {
        m_GLPrograms     = MemPool.ConstructArray<GLProgramObj>(1, false);
        m_GLPrograms[0]  = ShaderGLImpl::LinkProgram(ShaderStages.data(), static_cast<Uint32>(ShaderStages.size()), false);
        m_ShaderTypes[0] = ActiveStages;
        m_NumPrograms    = 1;
    }

    InitResourceLayouts(CreateInfo, ShaderStages, ActiveStages);
}

PipelineStateGLImpl::PipelineStateGLImpl(IReferenceCounters*                    pRefCounters,
                                         RenderDeviceGLImpl*                    pDeviceGL,
                                         const GraphicsPipelineStateCreateInfo& CreateInfo,
                                         bool                                   bIsDeviceInternal) :
    // clang-format off
    TPipelineStateBase
    {
        pRefCounters,
        pDeviceGL,
        CreateInfo,
        bIsDeviceInternal
    }
// clang-format on
{
    try
    {
        TShaderStages Shaders;
        ExtractShaders<ShaderGLImpl>(CreateInfo, Shaders);

        RefCntAutoPtr<ShaderGLImpl> pTempPS;
        if (CreateInfo.pPS == nullptr)
        {
            // Some OpenGL implementations fail if fragment shader is not present, so
            // create a dummy one.
            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
            ShaderCI.Source          = "void main(){}";
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.Desc.Name       = "Dummy fragment shader";
            pDeviceGL->CreateShader(ShaderCI, reinterpret_cast<IShader**>(static_cast<ShaderGLImpl**>(&pTempPS)));

            Shaders.emplace_back(pTempPS);
        }

        InitInternalObjects(CreateInfo, Shaders);
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

PipelineStateGLImpl::PipelineStateGLImpl(IReferenceCounters*                   pRefCounters,
                                         RenderDeviceGLImpl*                   pDeviceGL,
                                         const ComputePipelineStateCreateInfo& CreateInfo,
                                         bool                                  bIsDeviceInternal) :
    // clang-format off
    TPipelineStateBase
    {
        pRefCounters,
        pDeviceGL,
        CreateInfo,
        bIsDeviceInternal
    }
// clang-format on
{
    try
    {
        TShaderStages Shaders;
        ExtractShaders<ShaderGLImpl>(CreateInfo, Shaders);

        InitInternalObjects(CreateInfo, Shaders);
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

PipelineStateGLImpl::~PipelineStateGLImpl()
{
    Destruct();
}

void PipelineStateGLImpl::Destruct()
{
    GetDevice()->OnDestroyPSO(this);

    if (m_GLPrograms)
    {
        for (Uint32 i = 0; i < m_NumPrograms; ++i)
        {
            m_GLPrograms[i].~GLProgramObj();
        }
        m_GLPrograms = nullptr;
    }

    m_Signatures.fill({});

    m_SignatureCount = 0;
    m_NumPrograms    = 0;

    TPipelineStateBase::Destruct();
}

IMPLEMENT_QUERY_INTERFACE(PipelineStateGLImpl, IID_PipelineStateGL, TPipelineStateBase)

SHADER_TYPE PipelineStateGLImpl::GetShaderStageType(Uint32 Index) const
{
    VERIFY(Index < m_NumPrograms, "Index is out of range");
    return m_ShaderTypes[Index];
}

bool PipelineStateGLImpl::IsCompatibleWith(const IPipelineState* pPSO) const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;

    const auto& lhs = *this;
    const auto& rhs = *ValidatedCast<const PipelineStateGLImpl>(pPSO);

    if (lhs.GetSignatureCount() != rhs.GetSignatureCount())
        return false;

    for (Uint32 s = 0, SigCount = lhs.GetSignatureCount(); s < SigCount; ++s)
    {
        if (!lhs.GetSignature(s)->IsCompatibleWith(*rhs.GetSignature(s)))
            return false;
    }
    return true;
}

void PipelineStateGLImpl::CommitProgram(GLContextState& State)
{
    if (m_IsProgramPipelineSupported)
    {
        // WARNING: glUseProgram() overrides glBindProgramPipeline(). That is, if you have a program in use and
        // a program pipeline bound, all rendering will use the program that is in use, not the pipeline programs!
        // So make sure that glUseProgram(0) has been called if pipeline is in use
        State.SetProgram(GLObjectWrappers::GLProgramObj::Null());
        auto& Pipeline = GetGLProgramPipeline(State.GetCurrentGLContext());
        VERIFY(Pipeline != 0, "Program pipeline must not be null");
        State.SetPipeline(Pipeline);
    }
    else
    {
        VERIFY_EXPR(m_GLPrograms != nullptr);
        State.SetProgram(m_GLPrograms[0]);
    }
}

GLObjectWrappers::GLPipelineObj& PipelineStateGLImpl::GetGLProgramPipeline(GLContext::NativeGLContextType Context)
{
    ThreadingTools::LockHelper Lock(m_ProgPipelineLockFlag);
    for (auto& ctx_pipeline : m_GLProgPipelines)
    {
        if (ctx_pipeline.first == Context)
            return ctx_pipeline.second;
    }

    // Create new progam pipeline
    m_GLProgPipelines.emplace_back(Context, true);
    auto&  ctx_pipeline = m_GLProgPipelines.back();
    GLuint Pipeline     = ctx_pipeline.second;
    for (Uint32 i = 0; i < GetNumShaderStages(); ++i)
    {
        auto GLShaderBit = ShaderTypeToGLShaderBit(GetShaderStageType(i));
        // If the program has an active code for each stage mentioned in set flags,
        // then that code will be used by the pipeline. If program is 0, then the given
        // stages are cleared from the pipeline.
        glUseProgramStages(Pipeline, GLShaderBit, m_GLPrograms[i]);
        CHECK_GL_ERROR("glUseProgramStages() failed");
    }
    return ctx_pipeline.second;
}

#ifdef DILIGENT_DEVELOPMENT
PipelineStateGLImpl::ResourceAttribution PipelineStateGLImpl::GetResourceAttribution(const char* Name, SHADER_TYPE Stage) const
{
    const auto SignCount = GetSignatureCount();
    for (Uint32 sign = 0; sign < SignCount; ++sign)
    {
        const auto* const pSignature = GetSignature(sign);
        if (pSignature == nullptr)
            continue;

        const auto ResIndex = pSignature->FindResource(Stage, Name);
        if (ResIndex != ResourceAttribution::InvalidResourceIndex)
            return ResourceAttribution{pSignature, sign, ResIndex};
        else
        {
            const auto ImtblSamIndex = pSignature->FindImmutableSampler(Stage, Name);
            if (ImtblSamIndex != ResourceAttribution::InvalidSamplerIndex)
                return ResourceAttribution{pSignature, sign, ResourceAttribution::InvalidResourceIndex, ImtblSamIndex};
        }
    }
    return ResourceAttribution{};
}

void PipelineStateGLImpl::DvpValidateShaderResources(const std::shared_ptr<const ShaderResourcesGL>& pShaderResources, const char* ShaderName, SHADER_TYPE ShaderStages)
{
    m_ShaderResources.emplace_back(pShaderResources);
    m_ShaderNames.emplace_back(ShaderName);

    const auto HandleResource = [&](const ShaderResourcesGL::GLResourceAttribs& Attribs, SHADER_RESOURCE_TYPE ReadOnlyResourceType, PIPELINE_RESOURCE_FLAGS Flags) //
    {
        m_ResourceAttibutions.emplace_back();
        auto& ResAttribution = m_ResourceAttibutions.back();

        ResAttribution = GetResourceAttribution(Attribs.Name, ShaderStages);
        if (!ResAttribution)
        {
            LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource '", Attribs.Name,
                                "' that is not present in any pipeline resource signature used to create pipeline state '",
                                m_Desc.Name, "'.");
        }

        const auto* const pSignature = ResAttribution.pSignature;
        VERIFY_EXPR(pSignature != nullptr);

        if (ResAttribution.ResourceIndex != ResourceAttribution::InvalidResourceIndex)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(ResAttribution.ResourceIndex);

            // Shader reflection does not contain read-only flag, so image and storage buffer can be UAV or SRV.
            if (Attribs.ResourceType != ResDesc.ResourceType &&
                ReadOnlyResourceType != ResDesc.ResourceType)
            {
                LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource with name '", Attribs.Name,
                                    "' and type '", GetShaderResourceTypeLiteralName(Attribs.ResourceType), "' that is not compatible with type '",
                                    GetShaderResourceTypeLiteralName(ResDesc.ResourceType), "' in pipeline resource signature '", pSignature->GetDesc().Name, "'.");
            }

            if ((Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER))
            {
                LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource '", Attribs.Name,
                                    "' that is", ((Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                                    " labeled as formatted buffer, while the same resource specified by the pipeline resource signature '",
                                    pSignature->GetDesc().Name, "' is", ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                                    " labeled as such.");
            }
        }
    };

    const auto HandleUB = [&](const ShaderResourcesGL::UniformBufferInfo& Attribs) {
        HandleResource(Attribs, Attribs.ResourceType, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };

    const auto HandleTexture = [&](const ShaderResourcesGL::TextureInfo& Attribs) {
        const bool IsTexelBuffer = (Attribs.ResourceType != SHADER_RESOURCE_TYPE_TEXTURE_SRV);
        HandleResource(Attribs,
                       Attribs.ResourceType,
                       IsTexelBuffer ? PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER : PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER);
    };

    const auto HandleImage = [&](const ShaderResourcesGL::ImageInfo& Attribs) {
        const bool IsImageBuffer = (Attribs.ResourceType != SHADER_RESOURCE_TYPE_TEXTURE_UAV);
        HandleResource(Attribs,
                       IsImageBuffer ? SHADER_RESOURCE_TYPE_BUFFER_SRV : SHADER_RESOURCE_TYPE_TEXTURE_SRV,
                       IsImageBuffer ? PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER : PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };

    const auto HandleSB = [&](const ShaderResourcesGL::StorageBlockInfo& Attribs) {
        HandleResource(Attribs, SHADER_RESOURCE_TYPE_BUFFER_SRV, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };

    pShaderResources->ProcessConstResources(HandleUB, HandleTexture, HandleImage, HandleSB);
}

void PipelineStateGLImpl::DvpVerifySRBResources(ShaderResourceBindingGLImpl* pSRBs[],
                                                const TBindings              BoundResOffsets[],
                                                Uint32                       NumSRBs) const
{
    // Verify SRB compatibility with this pipeline
    const auto SignCount = GetResourceSignatureCount();
    TBindings  Bindings  = {};
    for (Uint32 sign = 0; sign < SignCount; ++sign)
    {
        // Get resource signature from the root signature
        const auto& pSignature = GetSignature(sign);
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

        DEV_CHECK_ERR(Bindings == BoundResOffsets[sign],
                      "Bound resources has incorrect base binding indices, this may indicate a bug in resource signature compatibility comparison.");

        pSignature->AddBindings(Bindings);
    }


    using AttribIter = std::vector<ResourceAttribution>::const_iterator;
    struct HandleResourceHelper
    {
        PipelineStateGLImpl const&    PSO;
        ShaderResourceBindingGLImpl** ppSRBs;
        const Uint32                  NumSRBs;
        AttribIter                    attrib_it;
        Uint32&                       shader_ind;

        HandleResourceHelper(const PipelineStateGLImpl& _PSO, ShaderResourceBindingGLImpl** _ppSRBs, Uint32 _NumSRBs, AttribIter iter, Uint32& ind) :
            PSO{_PSO}, ppSRBs{_ppSRBs}, NumSRBs{_NumSRBs}, attrib_it{iter}, shader_ind{ind}
        {}

        void Validate(const ShaderResourcesGL::GLResourceAttribs& Attribs, RESOURCE_DIMENSION ResDim, bool IsMS)
        {
            if (*attrib_it && !attrib_it->IsImmutableSampler())
            {
                if (attrib_it->SignatureIndex >= NumSRBs || ppSRBs[attrib_it->SignatureIndex] == nullptr)
                {
                    LOG_ERROR_MESSAGE("No resource is bound to variable '", Attribs.Name, "' in shader '", PSO.m_ShaderNames[shader_ind],
                                      "' of PSO '", PSO.m_Desc.Name, "': SRB at index ", attrib_it->SignatureIndex, " is not bound in the context.");
                    return;
                }

                const auto& SRBCache = ppSRBs[attrib_it->SignatureIndex]->GetResourceCache();
                attrib_it->pSignature->DvpValidateCommittedResource(Attribs, ResDim, IsMS, attrib_it->ResourceIndex, SRBCache, PSO.m_ShaderNames[shader_ind].c_str(), PSO.m_Desc.Name);
            }
            ++attrib_it;
        }

        void operator()(const ShaderResourcesGL::GLResourceAttribs& Attribs) { Validate(Attribs, RESOURCE_DIM_UNDEFINED, false); }
        void operator()(const ShaderResourcesGL::TextureInfo& Attribs) { Validate(Attribs, Attribs.ResourceDim, Attribs.IsMultisample); }
        void operator()(const ShaderResourcesGL::ImageInfo& Attribs) { Validate(Attribs, Attribs.ResourceDim, Attribs.IsMultisample); }
    };

    Uint32               i = 0;
    HandleResourceHelper HandleResource{*this, pSRBs, NumSRBs, m_ResourceAttibutions.begin(), i};

    VERIFY_EXPR(m_ShaderResources.size() == m_ShaderNames.size());
    for (; i < m_ShaderResources.size(); ++i)
    {
        m_ShaderResources[i]->ProcessConstResources(std::ref(HandleResource), std::ref(HandleResource), std::ref(HandleResource), std::ref(HandleResource));
    }
    VERIFY_EXPR(HandleResource.attrib_it == m_ResourceAttibutions.end());
}
#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
