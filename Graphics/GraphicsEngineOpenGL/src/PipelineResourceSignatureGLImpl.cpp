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
#include "PipelineResourceSignatureGLImpl.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "ShaderResourceBindingGLImpl.hpp"
#include "ShaderVariableGL.hpp"

namespace Diligent
{
namespace
{

inline bool ResourcesCompatible(const PipelineResourceSignatureGLImpl::ResourceAttribs& lhs,
                                const PipelineResourceSignatureGLImpl::ResourceAttribs& rhs)
{
    // Ignore sampler index.
    // clang-format off
    return lhs.CacheOffset          == rhs.CacheOffset &&
           lhs.ImtblSamplerAssigned == rhs.ImtblSamplerAssigned;
    // clang-format on
}

struct PatchedPipelineResourceSignatureDesc : PipelineResourceSignatureDesc
{
    std::vector<ImmutableSamplerDesc> m_ImmutableSamplers;

    PatchedPipelineResourceSignatureDesc(RenderDeviceGLImpl* pDeviceGL, const PipelineResourceSignatureDesc& Desc) :
        PipelineResourceSignatureDesc{Desc}
    {
        if (NumImmutableSamplers > 0 && !pDeviceGL->GetDeviceCaps().Features.SeparablePrograms)
        {
            m_ImmutableSamplers.resize(NumImmutableSamplers);

            SHADER_TYPE ActiveStages = SHADER_TYPE_UNKNOWN;
            for (Uint32 r = 0; r < NumResources; ++r)
                ActiveStages |= Resources[r].ShaderStages;

            for (Uint32 s = 0; s < NumImmutableSamplers; ++s)
            {
                m_ImmutableSamplers[s] = ImmutableSamplers[s];
                m_ImmutableSamplers[s].ShaderStages |= ActiveStages;
            }

            ImmutableSamplers = m_ImmutableSamplers.data();
        }
    }
};

} // namespace


const char* GetBindingRangeName(BINDING_RANGE Range)
{
    static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
    switch (Range)
    {
        // clang-format off
        case BINDING_RANGE_UNIFORM_BUFFER: return "Uniform buffer";
        case BINDING_RANGE_TEXTURE:        return "Texture";
        case BINDING_RANGE_IMAGE:          return "Image";
        case BINDING_RANGE_STORAGE_BUFFER: return "Storage buffer";
        // clang-format on
        default:
            return "Unknown";
    }
}

BINDING_RANGE PipelineResourceToBindingRange(const PipelineResourceDesc& Desc)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
    switch (Desc.ResourceType)
    {
        // clang-format off
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER: return BINDING_RANGE_UNIFORM_BUFFER;
        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:     return BINDING_RANGE_TEXTURE;
        case SHADER_RESOURCE_TYPE_BUFFER_SRV:      return (Desc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? BINDING_RANGE_TEXTURE : BINDING_RANGE_STORAGE_BUFFER;
        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:     return BINDING_RANGE_IMAGE;
        case SHADER_RESOURCE_TYPE_BUFFER_UAV:      return (Desc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? BINDING_RANGE_IMAGE : BINDING_RANGE_STORAGE_BUFFER;
            // clang-format on
        case SHADER_RESOURCE_TYPE_SAMPLER:
        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
        default:
            return BINDING_RANGE_UNKNOWN;
    }
}


PipelineResourceSignatureGLImpl::PipelineResourceSignatureGLImpl(IReferenceCounters*                  pRefCounters,
                                                                 RenderDeviceGLImpl*                  pDeviceGL,
                                                                 const PipelineResourceSignatureDesc& Desc,
                                                                 bool                                 bIsDeviceInternal) :
    PipelineResourceSignatureGLImpl{pRefCounters, pDeviceGL, PatchedPipelineResourceSignatureDesc{pDeviceGL, Desc}, bIsDeviceInternal, 0}
{}

PipelineResourceSignatureGLImpl::PipelineResourceSignatureGLImpl(IReferenceCounters*                  pRefCounters,
                                                                 RenderDeviceGLImpl*                  pDeviceGL,
                                                                 const PipelineResourceSignatureDesc& Desc,
                                                                 bool                                 bIsDeviceInternal,
                                                                 int) :
    TPipelineResourceSignatureBase{pRefCounters, pDeviceGL, Desc, bIsDeviceInternal}
{
    try
    {
        FixedLinearAllocator MemPool{GetRawAllocator()};

        // Reserve at least 1 element because m_pResourceAttribs must hold a pointer to memory
        MemPool.AddSpace<ResourceAttribs>(std::max(1u, Desc.NumResources));
        MemPool.AddSpace<SamplerPtr>(Desc.NumImmutableSamplers);

        ReserveSpaceForDescription(MemPool, Desc);

        const auto NumStaticResStages = GetNumStaticResStages();
        if (NumStaticResStages > 0)
        {
            MemPool.AddSpace<ShaderResourceCacheGL>(1);
            MemPool.AddSpace<ShaderVariableGL>(NumStaticResStages);
        }

        MemPool.Reserve();

        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(std::max(1u, m_Desc.NumResources));
        m_ImmutableSamplers = MemPool.ConstructArray<SamplerPtr>(m_Desc.NumImmutableSamplers);

        // The memory is now owned by PipelineResourceSignatureGLImpl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pResourceAttribs);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        if (NumStaticResStages > 0)
        {
            m_pStaticResCache = MemPool.Construct<ShaderResourceCacheGL>(ShaderResourceCacheGL::CacheContentType::Signature);
            m_StaticVarsMgrs  = MemPool.ConstructArray<ShaderVariableGL>(NumStaticResStages, std::ref(*this), std::ref(*m_pStaticResCache));
        }

        CreateLayouts();

        if (NumStaticResStages > 0)
        {
            constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
            for (Uint32 i = 0; i < m_StaticResStageIndex.size(); ++i)
            {
                Int8 Idx = m_StaticResStageIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(static_cast<Uint32>(Idx) < NumStaticResStages);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    m_StaticVarsMgrs[Idx].Initialize(*this, AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
                }
            }
        }

        m_Hash = CalculateHash();
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureGLImpl::CreateLayouts()
{
    std::array<Uint32, BINDING_RANGE_COUNT> StaticCounter = {};

    for (Uint32 s = 0; s < m_Desc.NumImmutableSamplers; ++s)
        GetDevice()->CreateSampler(m_Desc.ImmutableSamplers[s].Desc, &m_ImmutableSamplers[s]);

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];
        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
        {
            Int32 ImtblSamplerIdx = FindImmutableSampler(ResDesc.ShaderStages, ResDesc.Name);
            if (ImtblSamplerIdx < 0)
            {
                LOG_WARNING_MESSAGE("Pipeline resource signature '", m_Desc.Name, "' has separate sampler with name '", ResDesc.Name, "' that is not supported in OpenGL.");
            }

            new (m_pResourceAttribs + i) ResourceAttribs //
                {
                    ResourceAttribs::InvalidCacheOffset,
                    ImtblSamplerIdx < 0 ? ResourceAttribs::InvalidSamplerInd : static_cast<Uint32>(ImtblSamplerIdx),
                    ImtblSamplerIdx >= 0 //
                };
        }
        else
        {
            const auto Range = PipelineResourceToBindingRange(ResDesc);
            VERIFY_EXPR(Range != BINDING_RANGE_UNKNOWN);

            const Uint32 CacheOffset     = m_BindingCount[Range];
            Uint32       SamplerIdx      = ResourceAttribs::InvalidSamplerInd;
            Int32        ImtblSamplerIdx = -1;

            if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
            {
                ImtblSamplerIdx = FindImmutableSampler(ResDesc.ShaderStages, ResDesc.Name);
                if (ImtblSamplerIdx < 0)
                    SamplerIdx = FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd);
                else
                    SamplerIdx = static_cast<Uint32>(ImtblSamplerIdx);
            }

            new (m_pResourceAttribs + i) ResourceAttribs //
                {
                    CacheOffset,
                    SamplerIdx,
                    ImtblSamplerIdx >= 0 //
                };

            m_BindingCount[Range] += ResDesc.ArraySize;

            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                StaticCounter[Range] += ResDesc.ArraySize;
        }
    }

    if (m_pStaticResCache)
    {
        m_pStaticResCache->Initialize(StaticCounter[BINDING_RANGE_UNIFORM_BUFFER],
                                      StaticCounter[BINDING_RANGE_TEXTURE],
                                      StaticCounter[BINDING_RANGE_IMAGE],
                                      StaticCounter[BINDING_RANGE_STORAGE_BUFFER],
                                      GetRawAllocator());
        // Set immutable samplers for static resources.
        const auto ResIdxRange = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
        {
            const auto& ResDesc = GetResourceDesc(r);
            const auto& ResAttr = GetResourceAttribs(r);

            if (ResDesc.ResourceType != SHADER_RESOURCE_TYPE_TEXTURE_SRV || !ResAttr.IsSamplerAssigned())
                continue;

            ISampler* Sampler = nullptr;
            if (ResAttr.IsImmutableSamplerAssigned())
            {
                VERIFY_EXPR(ResAttr.SamplerInd < GetImmutableSamplerCount());

                Sampler = m_ImmutableSamplers[ResAttr.SamplerInd].RawPtr();
            }
            else
            {
                const auto& SampAttr = GetResourceAttribs(ResAttr.SamplerInd);
                if (!SampAttr.IsImmutableSamplerAssigned())
                    continue;

                Sampler = m_ImmutableSamplers[SampAttr.SamplerInd].RawPtr();
            }

            for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                m_pStaticResCache->SetSampler(ResAttr.CacheOffset + ArrInd, Sampler);
        }
#ifdef DILIGENT_DEVELOPMENT
        m_pStaticResCache->SetStaticResourcesInitialized();
#endif
    }
}

size_t PipelineResourceSignatureGLImpl::CalculateHash() const
{
    if (m_Desc.NumResources == 0 && m_Desc.NumImmutableSamplers == 0)
        return 0;

    auto Hash = CalculatePipelineResourceSignatureDescHash(m_Desc);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& Attr = m_pResourceAttribs[i];
        HashCombine(Hash, Attr.CacheOffset);
    }

    return Hash;
}

PipelineResourceSignatureGLImpl::~PipelineResourceSignatureGLImpl()
{
    Destruct();
}

void PipelineResourceSignatureGLImpl::Destruct()
{
    auto& RawAllocator = GetRawAllocator();

    if (m_ImmutableSamplers != nullptr)
    {
        for (Uint32 s = 0; s < m_Desc.NumImmutableSamplers; ++s)
            m_ImmutableSamplers[s].~SamplerPtr();

        m_ImmutableSamplers = nullptr;
    }

    if (m_StaticVarsMgrs)
    {
        for (auto Idx : m_StaticResStageIndex)
        {
            if (Idx >= 0)
                m_StaticVarsMgrs[Idx].~ShaderVariableGL();
        }
        m_StaticVarsMgrs = nullptr;
    }

    if (m_pStaticResCache)
    {
        m_pStaticResCache->Destroy(RawAllocator);
        m_pStaticResCache = nullptr;
    }

    if (void* pRawMem = m_pResourceAttribs)
    {
        RawAllocator.Free(pRawMem);
        m_pResourceAttribs = nullptr;
    }

    TPipelineResourceSignatureBase::Destruct();
}

void PipelineResourceSignatureGLImpl::ApplyBindings(GLObjectWrappers::GLProgramObj& GLProgram,
                                                    GLContextState&                 State,
                                                    SHADER_TYPE                     Stages,
                                                    const TBindings&                Bindings) const
{
    VERIFY(GLProgram != 0, "Null GL program");
    State.SetProgram(GLProgram);

    for (Uint32 r = 0; r < GetTotalResourceCount(); ++r)
    {
        const auto& ResDesc = m_Desc.Resources[r];
        const auto& ResAttr = m_pResourceAttribs[r];
        const auto  Range   = PipelineResourceToBindingRange(ResDesc);

        if (Range == BINDING_RANGE_UNKNOWN)
            continue;

        if ((ResDesc.ShaderStages & Stages) == 0)
            continue;

        const Uint32 BindingIndex = Bindings[Range] + ResAttr.CacheOffset;

        static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
        switch (Range)
        {
            case BINDING_RANGE_UNIFORM_BUFFER:
            {
                auto UniformBlockIndex = glGetUniformBlockIndex(GLProgram, ResDesc.Name);
                if (UniformBlockIndex == GL_INVALID_INDEX)
                    break; // Uniform block defined in resource signature, but not presented in shader program.

                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    glUniformBlockBinding(GLProgram, UniformBlockIndex + ArrInd, BindingIndex + ArrInd);
                    CHECK_GL_ERROR("glUniformBlockBinding() failed");
                }
                break;
            }
            case BINDING_RANGE_TEXTURE:
            {
                auto UniformLocation = glGetUniformLocation(GLProgram, ResDesc.Name);
                if (UniformLocation < 0)
                    break; // Uniform defined in resource signature, but not presented in shader program.

                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    glUniform1i(UniformLocation + ArrInd, BindingIndex + ArrInd);
                    CHECK_GL_ERROR("Failed to set binding point for sampler uniform '", ResDesc.Name, '\'');
                }
                break;
            }
#if GL_ARB_shader_image_load_store
            case BINDING_RANGE_IMAGE:
            {
                auto UniformLocation = glGetUniformLocation(GLProgram, ResDesc.Name);
                if (UniformLocation < 0)
                    break; // Uniform defined in resource signature, but not presented in shader program.

                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    // glUniform1i for image uniforms is not supported in at least GLES3.2.
                    // glProgramUniform1i is not available in GLES3.0
                    const Uint32 ImgBinding = BindingIndex + ArrInd;
                    glUniform1i(UniformLocation + ArrInd, ImgBinding);
                    if (glGetError() != GL_NO_ERROR)
                    {
                        if (ResDesc.ArraySize > 1)
                        {
                            LOG_WARNING_MESSAGE("Failed to set binding for image uniform '", ResDesc.Name, "'[", ArrInd,
                                                "]. Expected binding: ", ImgBinding,
                                                ". Make sure that this binding is explicitly assigned in shader source code."
                                                " Note that if the source code is converted from HLSL and if images are only used"
                                                " by a single shader stage, then bindings automatically assigned by HLSL->GLSL"
                                                " converter will work fine.");
                        }
                        else
                        {
                            LOG_WARNING_MESSAGE("Failed to set binding for image uniform '", ResDesc.Name,
                                                "'. Expected binding: ", ImgBinding,
                                                ". Make sure that this binding is explicitly assigned in shader source code."
                                                " Note that if the source code is converted from HLSL and if images are only used"
                                                " by a single shader stage, then bindings automatically assigned by HLSL->GLSL"
                                                " converter will work fine.");
                        }
                    }
                }
                break;
            }
#endif
#if GL_ARB_shader_storage_buffer_object
            case BINDING_RANGE_STORAGE_BUFFER:
            {
                auto SBIndex = glGetProgramResourceIndex(GLProgram, GL_SHADER_STORAGE_BLOCK, ResDesc.Name);
                if (SBIndex == GL_INVALID_INDEX)
                    break; // Storage block defined in resource signature, but not presented in shader program.

                if (glShaderStorageBlockBinding)
                {
                    for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                    {
                        glShaderStorageBlockBinding(GLProgram, SBIndex + ArrInd, BindingIndex + ArrInd);
                        CHECK_GL_ERROR("glShaderStorageBlockBinding() failed");
                    }
                }
                else
                {
                    const GLenum props[]                 = {GL_BUFFER_BINDING};
                    GLint        params[_countof(props)] = {};
                    glGetProgramResourceiv(GLProgram, GL_SHADER_STORAGE_BLOCK, SBIndex, _countof(props), props, _countof(params), nullptr, params);
                    CHECK_GL_ERROR("glGetProgramResourceiv() failed");

                    if (BindingIndex != static_cast<Uint32>(params[0]))
                    {
                        LOG_WARNING_MESSAGE("glShaderStorageBlockBinding is not available on this device and "
                                            "the engine is unable to automatically assign shader storage block bindindg for '",
                                            ResDesc.Name, "' variable. Expected binding: ", BindingIndex, ", actual binding: ", params[0],
                                            ". Make sure that this binding is explicitly assigned in shader source code."
                                            " Note that if the source code is converted from HLSL and if storage blocks are only used"
                                            " by a single shader stage, then bindings automatically assigned by HLSL->GLSL"
                                            " converter will work fine.");
                    }
                }
                break;
            }
#endif
            default:
                UNEXPECTED("Unsupported shader resource range type.");
        }
    }

    State.SetProgram(GLObjectWrappers::GLProgramObj::Null());
}

void PipelineResourceSignatureGLImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                  bool                     InitStaticResources)
{
    auto* pRenderDeviceGL = GetDevice();
    auto& SRBAllocator    = pRenderDeviceGL->GetSRBAllocator();
    auto  pResBinding     = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingGLImpl instance", ShaderResourceBindingGLImpl)(this);
    if (InitStaticResources)
        InitializeStaticSRBResources(pResBinding);
    pResBinding->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

void PipelineResourceSignatureGLImpl::InitializeStaticSRBResources(IShaderResourceBinding* pSRB) const
{
    InitializeStaticSRBResourcesImpl(ValidatedCast<ShaderResourceBindingGLImpl>(pSRB),
                                     [&](ShaderResourceBindingGLImpl* pSRBGL) //
                                     {
                                         CopyStaticResources(pSRBGL->GetResourceCache());
                                     } //
    );
}

Uint32 PipelineResourceSignatureGLImpl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    return GetStaticVariableCountImpl(ShaderType, m_StaticVarsMgrs);
}

IShaderResourceVariable* PipelineResourceSignatureGLImpl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    return GetStaticVariableByNameImpl(ShaderType, Name, m_StaticVarsMgrs);
}

IShaderResourceVariable* PipelineResourceSignatureGLImpl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    return GetStaticVariableByIndexImpl(ShaderType, Index, m_StaticVarsMgrs);
}

void PipelineResourceSignatureGLImpl::BindStaticResources(Uint32            ShaderFlags,
                                                          IResourceMapping* pResMapping,
                                                          Uint32            Flags)
{
    BindStaticResourcesImpl(ShaderFlags, pResMapping, Flags, m_StaticVarsMgrs);
}

void PipelineResourceSignatureGLImpl::CopyStaticResources(ShaderResourceCacheGL& DstResourceCache) const
{
    if (m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // DstResourceCache contains static, mutable and dynamic resources.
    const auto& SrcResourceCache = *m_pStaticResCache;
    const auto  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    VERIFY_EXPR(SrcResourceCache.GetContentType() == ShaderResourceCacheGL::CacheContentType::Signature);
    VERIFY_EXPR(DstResourceCache.GetContentType() == ShaderResourceCacheGL::CacheContentType::SRB);

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& ResAttr = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
            continue; // Skip separate samplers

        static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
        switch (PipelineResourceToBindingRange(ResDesc))
        {
            case BINDING_RANGE_UNIFORM_BUFFER:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetConstUB(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pBuffer)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    DstResourceCache.SetUniformBuffer(ResAttr.CacheOffset + ArrInd, RefCntAutoPtr<BufferGLImpl>{SrcCachedRes.pBuffer});
                }
                break;
            case BINDING_RANGE_STORAGE_BUFFER:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetConstSSBO(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pBufferView)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    DstResourceCache.SetSSBO(ResAttr.CacheOffset + ArrInd, RefCntAutoPtr<BufferViewGLImpl>{SrcCachedRes.pBufferView});
                }
                break;
            case BINDING_RANGE_TEXTURE:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetConstTexture(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pView)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    DstResourceCache.CopyTexture(ResAttr.CacheOffset + ArrInd, SrcCachedRes);
                }
                break;
            case BINDING_RANGE_IMAGE:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetConstImage(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pView)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    DstResourceCache.CopyImage(ResAttr.CacheOffset + ArrInd, SrcCachedRes);
                }
                break;
            default:
                UNEXPECTED("Unsupported shader resource range type.");
        }
    }

    // Copy immutable samplers.
    for (Uint32 r = 0; r < m_Desc.NumResources; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& ResAttr = GetResourceAttribs(r);

        if (ResDesc.ResourceType != SHADER_RESOURCE_TYPE_TEXTURE_SRV ||
            ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            continue;

        if (!ResAttr.IsSamplerAssigned())
            continue;

        ISampler* Sampler = nullptr;
        if (ResAttr.IsImmutableSamplerAssigned())
        {
            VERIFY_EXPR(ResAttr.SamplerInd < GetImmutableSamplerCount());

            Sampler = m_ImmutableSamplers[ResAttr.SamplerInd].RawPtr();
        }
        else
        {
            const auto& SampAttr = GetResourceAttribs(ResAttr.SamplerInd);
            if (!SampAttr.IsImmutableSamplerAssigned())
                continue;

            Sampler = m_ImmutableSamplers[SampAttr.SamplerInd].RawPtr();
        }

        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
            DstResourceCache.SetSampler(ResAttr.CacheOffset + ArrInd, Sampler);
    }

#ifdef DILIGENT_DEVELOPMENT
    DstResourceCache.SetStaticResourcesInitialized();
#endif
}

void PipelineResourceSignatureGLImpl::InitSRBResourceCache(ShaderResourceCacheGL& ResourceCache) const
{
    ResourceCache.Initialize(m_BindingCount[BINDING_RANGE_UNIFORM_BUFFER],
                             m_BindingCount[BINDING_RANGE_TEXTURE],
                             m_BindingCount[BINDING_RANGE_IMAGE],
                             m_BindingCount[BINDING_RANGE_STORAGE_BUFFER],
                             GetRawAllocator());
}

bool PipelineResourceSignatureGLImpl::IsCompatibleWith(const PipelineResourceSignatureGLImpl& Other) const
{
    if (this == &Other)
        return true;

    if (GetHash() != Other.GetHash())
        return false;

    if (m_BindingCount != Other.m_BindingCount)
        return false;

    if (!PipelineResourceSignaturesCompatible(GetDesc(), Other.GetDesc()))
        return false;

    const auto ResCount = GetTotalResourceCount();
    VERIFY_EXPR(ResCount == Other.GetTotalResourceCount());
    for (Uint32 r = 0; r < ResCount; ++r)
    {
        if (!ResourcesCompatible(GetResourceAttribs(r), Other.GetResourceAttribs(r)))
            return false;
    }

    return true;
}

#ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureGLImpl::DvpValidateCommittedResource(const ShaderResourcesGL::GLResourceAttribs& GLAttribs,
                                                                   RESOURCE_DIMENSION                          ResourceDim,
                                                                   bool                                        IsMultisample,
                                                                   Uint32                                      ResIndex,
                                                                   const ShaderResourceCacheGL&                ResourceCache,
                                                                   const char*                                 ShaderName,
                                                                   const char*                                 PSOName) const
{
    VERIFY_EXPR(ResIndex < m_Desc.NumResources);
    const auto& ResDesc = m_Desc.Resources[ResIndex];
    const auto& ResAttr = m_pResourceAttribs[ResIndex];
    VERIFY(strcmp(ResDesc.Name, GLAttribs.Name) == 0, "Inconsistent resource names");

    if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
        return true; // Skip separate samplers

    VERIFY_EXPR(GLAttribs.ArraySize <= ResDesc.ArraySize);

    bool BindingsOK = true;

    static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
    switch (PipelineResourceToBindingRange(ResDesc))
    {
        case BINDING_RANGE_UNIFORM_BUFFER:
            for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsUBBound(ResAttr.CacheOffset + ArrInd))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(GLAttribs, ArrInd),
                                      "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
                    BindingsOK = false;
                    continue;
                }
            }
            break;
        case BINDING_RANGE_STORAGE_BUFFER:
            for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsSSBOBound(ResAttr.CacheOffset + ArrInd))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(GLAttribs, ArrInd),
                                      "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
                    BindingsOK = false;
                    continue;
                }
            }
            break;
        case BINDING_RANGE_TEXTURE:
            for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsTextureBound(ResAttr.CacheOffset + ArrInd, ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(GLAttribs, ArrInd),
                                      "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
                    BindingsOK = false;
                    continue;
                }

                const auto& Tex = ResourceCache.GetConstTexture(ResAttr.CacheOffset + ArrInd);
                if (Tex.pTexture)
                    ValidateResourceViewDimension(ResDesc.Name, ResDesc.ArraySize, ArrInd, Tex.pView.RawPtr<ITextureView>(), ResourceDim, IsMultisample);
                else
                    ValidateResourceViewDimension(ResDesc.Name, ResDesc.ArraySize, ArrInd, Tex.pView.RawPtr<IBufferView>(), ResourceDim, IsMultisample);
            }
            break;
        case BINDING_RANGE_IMAGE:
            for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsImageBound(ResAttr.CacheOffset + ArrInd, ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(GLAttribs, ArrInd),
                                      "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
                    BindingsOK = false;
                    continue;
                }

                const auto& Img = ResourceCache.GetConstImage(ResAttr.CacheOffset + ArrInd);
                if (Img.pTexture)
                    ValidateResourceViewDimension(ResDesc.Name, ResDesc.ArraySize, ArrInd, Img.pView.RawPtr<ITextureView>(), ResourceDim, IsMultisample);
                else
                    ValidateResourceViewDimension(ResDesc.Name, ResDesc.ArraySize, ArrInd, Img.pView.RawPtr<IBufferView>(), ResourceDim, IsMultisample);
            }
            break;
        default:
            UNEXPECTED("Unsupported shader resource range type.");
    }
    return BindingsOK;
}
#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
