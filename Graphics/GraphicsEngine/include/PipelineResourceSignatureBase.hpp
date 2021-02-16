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

namespace Diligent
{

/// Validates pipeline resource signature description and throws an exception in case of an error.
void ValidatePipelineResourceSignatureDesc(const PipelineResourceSignatureDesc& Desc) noexcept(false);

/// Finds an immutable sampler for the resource name 'ResourceName' that is defined in shader stages 'ShaderStages'.
/// If 'SamplerSuffix' is not null, it will be appended to the 'ResourceName'.
/// Returns an index of the sampler in ImtblSamplers array, or -1 if there is no suitable sampler.
Int32 FindImmutableSampler(const ImmutableSamplerDesc* ImtblSamplers,
                           Uint32                      NumImtblSamplers,
                           SHADER_TYPE                 ShaderStages,
                           const char*                 ResourceName,
                           const char*                 SamplerSuffix);

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

        ValidatePipelineResourceSignatureDesc(Desc);
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
    Uint32 GetNumActiveShaderStages() const { return m_NumShaderStages; }

    // Returns the type of the active shader stage with the given index.
    SHADER_TYPE GetActiveShaderStageType(Uint32 StageIndex) const
    {
        VERIFY_EXPR(StageIndex < m_NumShaderStages);

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
            VERIFY(Desc.ImmutableSamplers[i].SamplerOrTextureName != nullptr,
                   "SamplerOrTextureName can't be null. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
            VERIFY(Desc.ImmutableSamplers[i].SamplerOrTextureName[0] != '\0',
                   "SamplerOrTextureName can't be empty. This error should've been caught by ValidatePipelineResourceSignatureDesc().");

            Allocator.AddSpaceForString(Desc.ImmutableSamplers[i].SamplerOrTextureName);
        }

        if (Desc.UseCombinedTextureSamplers)
            Allocator.AddSpaceForString(Desc.CombinedSamplerSuffix);
    }

    void CopyDescription(FixedLinearAllocator& Allocator, const PipelineResourceSignatureDesc& Desc) noexcept(false)
    {
        PipelineResourceDesc* pResources = Allocator.Allocate<PipelineResourceDesc>(Desc.NumResources);
        ImmutableSamplerDesc* pSamplers  = Allocator.Allocate<ImmutableSamplerDesc>(Desc.NumImmutableSamplers);

        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            auto& Dst = pResources[i];
            Dst       = Desc.Resources[i];
            VERIFY_EXPR(Desc.Resources[i].Name != nullptr && Desc.Resources[i].Name[0] != '\0');
            Dst.Name = Allocator.CopyString(Desc.Resources[i].Name);

            ++m_ResourceOffsets[Dst.VarType + 1];
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
            auto& Dst = pSamplers[i];
            Dst       = Desc.ImmutableSamplers[i];
            VERIFY_EXPR(Desc.ImmutableSamplers[i].SamplerOrTextureName != nullptr && Desc.ImmutableSamplers[i].SamplerOrTextureName[0] != '\0');
            Dst.SamplerOrTextureName = Allocator.CopyString(Desc.ImmutableSamplers[i].SamplerOrTextureName);
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

#if DILIGENT_DEBUG
        m_IsDestructed = true;
#endif
    }

    Int8 GetStaticVariableCountHelper(SHADER_TYPE ShaderType, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& StaticVarIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = StaticVarIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return VarMngrInd;
    }

    Int8 GetStaticVariableByNameHelper(SHADER_TYPE ShaderType, const Char* Name, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& StaticVarIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = StaticVarIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return VarMngrInd;
    }

    Int8 GetStaticVariableByIndexHelper(SHADER_TYPE ShaderType, Uint32 Index, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& StaticVarIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto VarMngrInd    = StaticVarIndex[ShaderTypeInd];
        if (VarMngrInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return VarMngrInd;
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

    PIPELINE_TYPE m_PipelineType = PIPELINE_TYPE_INVALID;

    // The number of shader stages that have resources.
    Uint8 m_NumShaderStages = 0;

#ifdef DILIGENT_DEBUG
    bool m_IsDestructed = false;
#endif
};

} // namespace Diligent
