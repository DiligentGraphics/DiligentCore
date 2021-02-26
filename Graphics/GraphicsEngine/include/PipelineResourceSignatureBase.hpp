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

#pragma once

/// \file
/// Implementation of the Diligent::PipelineResourceSignatureBase template class

#include <array>
#include <limits>
#include <algorithm>

#include "PrivateConstants.h"
#include "PipelineResourceSignature.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "FixedLinearAllocator.hpp"
#include "BasicMath.hpp"
#include "StringTools.hpp"
#include "PlatformMisc.hpp"

namespace Diligent
{

/// Validates pipeline resource signature description and throws an exception in case of an error.
void ValidatePipelineResourceSignatureDesc(const PipelineResourceSignatureDesc& Desc, bool ShaderResourceRuntimeArraySupported) noexcept(false);

static constexpr Uint32 InvalidImmutableSamplerIndex = ~0u;
/// Finds an immutable sampler for the resource name 'ResourceName' that is defined in shader stages 'ShaderStages'.
/// If 'SamplerSuffix' is not null, it will be appended to the 'ResourceName'.
/// Returns an index of the sampler in ImtblSamplers array, or InvalidImmutableSamplerIndex if there is no suitable sampler.
Uint32 FindImmutableSampler(const ImmutableSamplerDesc* ImtblSamplers,
                            Uint32                      NumImtblSamplers,
                            SHADER_TYPE                 ShaderStages,
                            const char*                 ResourceName,
                            const char*                 SamplerSuffix);

/// Returns true if two pipeline resource signature descriptions are compatible, and false otherwise
bool PipelineResourceSignaturesCompatible(const PipelineResourceSignatureDesc& Desc0,
                                          const PipelineResourceSignatureDesc& Desc1) noexcept;

/// Calculates hash of the pipeline resource signature description.
size_t CalculatePipelineResourceSignatureDescHash(const PipelineResourceSignatureDesc& Desc) noexcept;

/// Template class implementing base functionality of the pipeline resource signature object.

/// \tparam BaseInterface        - Base interface that this class will inheret
///                                (Diligent::IPipelineResourceSignatureD3D12, Diligent::PipelineResourceSignatureVk, ...).
/// \tparam RenderDeviceImplType - Type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class PipelineResourceSignatureBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, PipelineResourceSignatureDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, PipelineResourceSignatureDesc>;

    /// \param pRefCounters      - Reference counters object that controls the lifetime of this resource signature.
    /// \param pDevice           - Pointer to the device.
    /// \param Desc              - Resource signature description.
    /// \param bIsDeviceInternal - Flag indicating if this resource signature is an internal device object and
    ///                            must not keep a strong reference to the device.
    PipelineResourceSignatureBase(IReferenceCounters*                  pRefCounters,
                                  RenderDeviceImplType*                pDevice,
                                  const PipelineResourceSignatureDesc& Desc,
                                  bool                                 bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        // Don't read from m_Desc until it was allocated and copied in CopyDescription()
        this->m_Desc.Resources             = nullptr;
        this->m_Desc.ImmutableSamplers     = nullptr;
        this->m_Desc.CombinedSamplerSuffix = nullptr;

        ValidatePipelineResourceSignatureDesc(Desc, pDevice->GetDeviceCaps().Features.ShaderResourceRuntimeArray);

        // Determine shader stages that have any resources as well as
        // shader stages that have static resources.
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& ResDesc = Desc.Resources[i];

            m_ShaderStages |= ResDesc.ShaderStages;
            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                m_StaticResShaderStages |= ResDesc.ShaderStages;
        }

        if (m_ShaderStages != SHADER_TYPE_UNKNOWN)
        {
            m_PipelineType = PipelineTypeFromShaderStages(m_ShaderStages);
            DEV_CHECK_ERR(m_PipelineType != PIPELINE_TYPE_INVALID, "Failed to deduce pipeline type from shader stages");
        }

        {
            Uint32 StaticVarStageIdx = 0;
            for (auto StaticResStages = m_StaticResShaderStages; StaticResStages != SHADER_TYPE_UNKNOWN; ++StaticVarStageIdx)
            {
                const auto StageBit                  = ExtractLSB(StaticResStages);
                const auto ShaderTypeInd             = GetShaderTypePipelineIndex(StageBit, m_PipelineType);
                m_StaticResStageIndex[ShaderTypeInd] = static_cast<Int8>(StaticVarStageIdx);
            }
            VERIFY_EXPR(StaticVarStageIdx == GetNumStaticResStages());
        }
    }

    ~PipelineResourceSignatureBase()
    {
        VERIFY(m_IsDestructed, "This object must be explicitly destructed with Destruct()");
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineResourceSignature, TDeviceObjectBase)

    size_t GetHash() const { return m_Hash; }

    PIPELINE_TYPE GetPipelineType() const { return m_PipelineType; }

    const char* GetCombinedSamplerSuffix() const { return this->m_Desc.CombinedSamplerSuffix; }

    bool IsUsingCombinedSamplers() const { return this->m_Desc.CombinedSamplerSuffix != nullptr; }
    bool IsUsingSeparateSamplers() const { return !IsUsingCombinedSamplers(); }

    Uint32 GetTotalResourceCount() const { return this->m_Desc.NumResources; }
    Uint32 GetImmutableSamplerCount() const { return this->m_Desc.NumImmutableSamplers; }

    std::pair<Uint32, Uint32> GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE VarType) const
    {
        return std::pair<Uint32, Uint32>{m_ResourceOffsets[VarType], m_ResourceOffsets[VarType + 1]};
    }

    // Returns the number of shader stages that have resources.
    Uint32 GetNumActiveShaderStages() const
    {
        return PlatformMisc::CountOneBits(Uint32{m_ShaderStages});
    }

    SHADER_TYPE GetActiveShaderStages() const
    {
        return m_ShaderStages;
    }

    // Returns the number of shader stages that have static resources.
    Uint32 GetNumStaticResStages() const
    {
        return PlatformMisc::CountOneBits(Uint32{m_StaticResShaderStages});
    }

    // Returns the type of the active shader stage with the given index.
    SHADER_TYPE GetActiveShaderStageType(Uint32 StageIndex) const
    {
        VERIFY_EXPR(StageIndex < GetNumActiveShaderStages());

        SHADER_TYPE Stages = m_ShaderStages;
        for (Uint32 Index = 0; Stages != SHADER_TYPE_UNKNOWN; ++Index)
        {
            auto StageBit = ExtractLSB(Stages);

            if (Index == StageIndex)
                return StageBit;
        }

        UNEXPECTED("Index is out of range");
        return SHADER_TYPE_UNKNOWN;
    }

    static constexpr Uint32 InvalidResourceIndex = ~0u;
    /// Finds a resource with the given name in the specified shader stage and returns its
    /// index in m_Desc.Resources[], or InvalidResourceIndex if the resource is not found.
    Uint32 FindResource(SHADER_TYPE ShaderStage, const char* ResourceName) const
    {
        for (Uint32 r = 0; r < this->m_Desc.NumResources; ++r)
        {
            const auto& ResDesc = this->m_Desc.Resources[r];
            if ((ResDesc.ShaderStages & ShaderStage) != 0 && strcmp(ResDesc.Name, ResourceName) == 0)
                return r;
        }

        return InvalidResourceIndex;
    }

    /// Finds an immutable with the given name in the specified shader stage and returns its
    /// index in m_Desc.ImmutableSamplers[], or InvalidImmutableSamplerIndex if the sampler is not found.
    Uint32 FindImmutableSampler(SHADER_TYPE ShaderStage, const char* ResourceName) const
    {
        return Diligent::FindImmutableSampler(this->m_Desc.ImmutableSamplers, this->m_Desc.NumImmutableSamplers,
                                              ShaderStage, ResourceName, GetCombinedSamplerSuffix());
    }

    const PipelineResourceDesc& GetResourceDesc(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < this->m_Desc.NumResources);
        return this->m_Desc.Resources[ResIndex];
    }

    const ImmutableSamplerDesc& GetImmutableSamplerDesc(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < this->m_Desc.NumImmutableSamplers);
        return this->m_Desc.ImmutableSamplers[SampIndex];
    }

    template <typename TPipelineResourceSignature>
    static Uint32 CopyResourceSignatures(PIPELINE_TYPE                             PipelineType,
                                         const Uint32                              SignatureCount,
                                         IPipelineResourceSignature*               ppResourceSignatures[],
                                         RefCntAutoPtr<TPipelineResourceSignature> DstSignatures[],
                                         const size_t                              MaxDstSignatureCount)
    {
        Uint32 MaxSignatureBindIndex = 0;
        for (Uint32 i = 0; i < SignatureCount; ++i)
        {
            auto* pSignature = ValidatedCast<TPipelineResourceSignature>(ppResourceSignatures[i]);
            VERIFY(pSignature != nullptr, "Pipeline resource signature at index ", i, " is null. This error should've been caught by ValidatePipelineResourceSignatures.");

            const Uint8 Index = pSignature->GetDesc().BindingIndex;

#ifdef DILIGENT_DEBUG
            VERIFY(Index < MaxDstSignatureCount,
                   "Pipeline resource signature specifies binding index ", Uint32{Index}, " that exceeds the limit (", MaxDstSignatureCount - 1,
                   "). This error should've been caught by ValidatePipelineResourceSignatureDesc.");

            VERIFY(DstSignatures[Index] == nullptr,
                   "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
                   " conflicts with another resource signature '", DstSignatures[Index]->GetDesc().Name,
                   "' that uses the same index. This error should've been caught by ValidatePipelineResourceSignatures.");

            for (Uint32 s = 0, StageCount = pSignature->GetNumActiveShaderStages(); s < StageCount; ++s)
            {
                const auto ShaderType = pSignature->GetActiveShaderStageType(s);
                VERIFY(IsConsistentShaderType(ShaderType, PipelineType),
                       "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
                       " has shader stage '", GetShaderTypeLiteralName(ShaderType), "' that is not compatible with pipeline type '",
                       GetPipelineTypeString(PipelineType), "'.");
            }
#endif

            MaxSignatureBindIndex = std::max<Uint32>(MaxSignatureBindIndex, Index);
            DstSignatures[Index]  = pSignature;
        }
        return MaxSignatureBindIndex;
    }

protected:
    void ReserveSpaceForDescription(FixedLinearAllocator& Allocator, const PipelineResourceSignatureDesc& Desc) const noexcept(false)
    {
        Allocator.AddSpace<PipelineResourceDesc>(Desc.NumResources);
        Allocator.AddSpace<ImmutableSamplerDesc>(Desc.NumImmutableSamplers);

        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& Res = Desc.Resources[i];

            VERIFY(Res.Name != nullptr, "Name can't be null. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            VERIFY(Res.Name[0] != '\0', "Name can't be empty. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            VERIFY(Res.ShaderStages != SHADER_TYPE_UNKNOWN, "ShaderStages can't be SHADER_TYPE_UNKNOWN. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            VERIFY(Res.ArraySize != 0, "ArraySize can't be 0. This error should've been caught by ValidatePipelineResourceSignatureDesc().");

            Allocator.AddSpaceForString(Res.Name);
        }

        for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
        {
            const auto* SamOrTexName = Desc.ImmutableSamplers[i].SamplerOrTextureName;
            VERIFY(SamOrTexName != nullptr, "SamplerOrTextureName can't be null. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            VERIFY(SamOrTexName[0] != '\0', "SamplerOrTextureName can't be empty. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            Allocator.AddSpaceForString(SamOrTexName);
        }

        if (Desc.UseCombinedTextureSamplers)
            Allocator.AddSpaceForString(Desc.CombinedSamplerSuffix);
    }

    void CopyDescription(FixedLinearAllocator& Allocator, const PipelineResourceSignatureDesc& Desc) noexcept(false)
    {
        PipelineResourceDesc* pResources = Allocator.ConstructArray<PipelineResourceDesc>(Desc.NumResources);
        ImmutableSamplerDesc* pSamplers  = Allocator.ConstructArray<ImmutableSamplerDesc>(Desc.NumImmutableSamplers);

        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& SrcRes = Desc.Resources[i];
            auto&       DstRes = pResources[i];

            DstRes = SrcRes;
            VERIFY_EXPR(SrcRes.Name != nullptr && SrcRes.Name[0] != '\0');
            DstRes.Name = Allocator.CopyString(SrcRes.Name);

            ++m_ResourceOffsets[DstRes.VarType + 1];
        }

        // Sort resources by variable type (all static -> all mutable -> all dynamic)
        std::sort(pResources, pResources + Desc.NumResources,
                  [](const PipelineResourceDesc& lhs, const PipelineResourceDesc& rhs) {
                      return lhs.VarType < rhs.VarType;
                  });

        for (size_t i = 1; i < m_ResourceOffsets.size(); ++i)
            m_ResourceOffsets[i] += m_ResourceOffsets[i - 1];

#ifdef DILIGENT_DEBUG
        VERIFY_EXPR(m_ResourceOffsets[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES] == Desc.NumResources);
        for (Uint32 VarType = 0; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++VarType)
        {
            auto IdxRange = GetResourceIndexRange(static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType));
            for (Uint32 idx = IdxRange.first; idx < IdxRange.second; ++idx)
                VERIFY(pResources[idx].VarType == VarType, "Unexpected resource var type");
        }
#endif

        for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
        {
            const auto& SrcSam = Desc.ImmutableSamplers[i];
            auto&       DstSam = pSamplers[i];

            DstSam = SrcSam;
            VERIFY_EXPR(SrcSam.SamplerOrTextureName != nullptr && SrcSam.SamplerOrTextureName[0] != '\0');
            DstSam.SamplerOrTextureName = Allocator.CopyString(SrcSam.SamplerOrTextureName);
        }

        this->m_Desc.Resources         = pResources;
        this->m_Desc.ImmutableSamplers = pSamplers;

        if (Desc.UseCombinedTextureSamplers)
            this->m_Desc.CombinedSamplerSuffix = Allocator.CopyString(Desc.CombinedSamplerSuffix);
    }

    void Destruct()
    {
        VERIFY(!m_IsDestructed, "This object has already been destructed");

        this->m_Desc.Resources             = nullptr;
        this->m_Desc.ImmutableSamplers     = nullptr;
        this->m_Desc.CombinedSamplerSuffix = nullptr;

        m_StaticResStageIndex.fill(-1);

#if DILIGENT_DEBUG
        m_IsDestructed = true;
#endif
    }

    template <typename ShaderVarManagerType>
    Uint32 GetStaticVariableCountImpl(SHADER_TYPE ShaderType, const ShaderVarManagerType StaticVarMgrs[]) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return 0;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = m_StaticResStageIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
            return 0;

        VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < GetNumStaticResStages());
        return StaticVarMgrs[VarMngrInd].GetVariableCount();
    }

    template <typename ShaderVarManagerType>
    IShaderResourceVariable* GetStaticVariableByNameImpl(SHADER_TYPE ShaderType, const Char* Name, const ShaderVarManagerType StaticVarMgrs[]) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return nullptr;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = m_StaticResStageIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
            return nullptr;

        VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < GetNumStaticResStages());
        return StaticVarMgrs[VarMngrInd].GetVariable(Name);
    }

    template <typename ShaderVarManagerType>
    IShaderResourceVariable* GetStaticVariableByIndexImpl(SHADER_TYPE ShaderType, Uint32 Index, const ShaderVarManagerType StaticVarMgrs[]) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return nullptr;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = m_StaticResStageIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
            return nullptr;

        VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < GetNumStaticResStages());
        return StaticVarMgrs[VarMngrInd].GetVariable(Index);
    }

    template <typename ShaderVarManagerType>
    void BindStaticResourcesImpl(Uint32               ShaderFlags,
                                 IResourceMapping*    pResMapping,
                                 Uint32               Flags,
                                 ShaderVarManagerType StaticVarMgrs[])
    {
        const auto PipelineType = GetPipelineType();
        for (Uint32 ShaderInd = 0; ShaderInd < m_StaticResStageIndex.size(); ++ShaderInd)
        {
            const auto VarMngrInd = m_StaticResStageIndex[ShaderInd];
            if (VarMngrInd >= 0)
            {
                VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < GetNumStaticResStages());
                // ShaderInd is the shader type pipeline index here
                const auto ShaderType = GetShaderTypeFromPipelineIndex(ShaderInd, PipelineType);
                if (ShaderFlags & ShaderType)
                {
                    StaticVarMgrs[VarMngrInd].BindResources(pResMapping, Flags);
                }
            }
        }
    }

    template <typename SRBImplType, typename InitResourcesHandler>
    void InitializeStaticSRBResourcesImpl(SRBImplType* pSRB, InitResourcesHandler Handler) const
    {
        DEV_CHECK_ERR(pSRB != nullptr, "SRB must not be null");
        if (pSRB->StaticResourcesInitialized())
        {
            LOG_WARNING_MESSAGE("Static resources have already been initialized in this shader resource binding object.");
            return;
        }

        const auto* const pSRBSignature = pSRB->GetPipelineResourceSignature();
#ifdef DILIGENT_DEVELOPMENT
        if (!pSRBSignature->IsCompatibleWith(this))
        {
            LOG_ERROR_MESSAGE("Shader resource binding is not compatible with resource signature '", this->m_Desc.Name, "'.");
        }
#endif

        Handler(pSRB);

        pSRB->SetStaticResourcesInitialized();
    }

    // Finds a sampler that is assigned to texture Tex, when combined texture samplers are used.
    // Returns an index of the sampler in m_Desc.Resources array, or InvalidSamplerValue if there is
    // no such sampler, or if combined samplers are not used.
    Uint32 FindAssignedSampler(const PipelineResourceDesc& Tex, Uint32 InvalidSamplerValue) const
    {
        VERIFY_EXPR(Tex.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV);
        Uint32 SamplerInd = InvalidSamplerValue;
        if (IsUsingCombinedSamplers())
        {
            const auto IdxRange = GetResourceIndexRange(Tex.VarType);

            for (Uint32 i = IdxRange.first; i < IdxRange.second; ++i)
            {
                const auto& Res = this->m_Desc.Resources[i];
                VERIFY_EXPR(Tex.VarType == Res.VarType);

                if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER &&
                    (Tex.ShaderStages & Res.ShaderStages) != 0 &&
                    StreqSuff(Res.Name, Tex.Name, GetCombinedSamplerSuffix()))
                {
                    VERIFY_EXPR((Res.ShaderStages & Tex.ShaderStages) == Tex.ShaderStages);
                    SamplerInd = i;
                    break;
                }
            }
        }
        return SamplerInd;
    }

protected:
    size_t m_Hash = 0;

    // Resource offsets (e.g. index of the first resource), for each variable type.
    std::array<Uint16, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES + 1> m_ResourceOffsets = {};

    // Shader stages that have resources.
    SHADER_TYPE m_ShaderStages = SHADER_TYPE_UNKNOWN;

    // Shader stages that have static resources.
    SHADER_TYPE m_StaticResShaderStages = SHADER_TYPE_UNKNOWN;

    PIPELINE_TYPE m_PipelineType = PIPELINE_TYPE_INVALID;

    // Index of the shader stage that has static resources, for every shader
    // type in the pipeline (given by GetShaderTypePipelineIndex(ShaderType, m_PipelineType)).
    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticResStageIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

#ifdef DILIGENT_DEBUG
    bool m_IsDestructed = false;
#endif
};

} // namespace Diligent
