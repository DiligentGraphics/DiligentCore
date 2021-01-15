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
#include <unordered_map>
#include <algorithm>

#include "PipelineResourceSignature.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "FixedLinearAllocator.hpp"

namespace Diligent
{

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

    /// \param pRefCounters      - Reference counters object that controls the lifetime of this BLAS.
    /// \param pDevice           - Pointer to the device.
    /// \param Desc              - TLAS description.
    /// \param bIsDeviceInternal - Flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    PipelineResourceSignatureBase(IReferenceCounters*                  pRefCounters,
                                  RenderDeviceImplType*                pDevice,
                                  const PipelineResourceSignatureDesc& Desc,
                                  bool                                 bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        this->m_Desc.Resources         = nullptr;
        this->m_Desc.ImmutableSamplers = nullptr;
    }

    ~PipelineResourceSignatureBase()
    {
        VERIFY(m_IsDestructed, "This object must be explicitly destructed with Destruct()");
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineResourceSignature, TDeviceObjectBase)

    Uint32 GetVariableCount(SHADER_TYPE ShaderType) const
    {
        // AZ TODO: optimize
        Uint32 Count = 0;
        for (Uint32 i = 0; i < this->m_Desc.NumResources; ++i)
        {
            auto& Res = this->m_Desc.Resources[i];
            if (Res.VarType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC && (Res.ShaderStages & ShaderType))
                ++Count;
        }
        return Count;
    }

    Uint32 GetVariableGlobalIndexByName(SHADER_TYPE ShaderType, const char* Name) const
    {
        // AZ TODO: optimize
        for (Uint32 i = 0; i < this->m_Desc.NumResources; ++i)
        {
            auto& Res = this->m_Desc.Resources[i];
            if (Res.VarType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC && (Res.ShaderStages & ShaderType))
            {
                if (strcmp(Name, Res.Name) == 0)
                    return i;
            }
        }
        return ~0u;
    }

    Uint32 GetVariableGlobalIndexByIndex(SHADER_TYPE ShaderType, Uint32 Index) const
    {
        // AZ TODO: optimize
        Uint32 Count = 0;
        for (Uint32 i = 0; i < this->m_Desc.NumResources; ++i)
        {
            auto& Res = this->m_Desc.Resources[i];
            if (Res.VarType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC && (Res.ShaderStages & ShaderType))
            {
                if (Index == Count)
                    return i;

                ++Count;
            }
        }
        return ~0u;
    }

    Uint32 GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE VarType) const
    {
        // AZ TODO: optimize
        Uint32 Count = 0;
        for (Uint32 i = 0; i < this->m_Desc.NumResources; ++i)
        {
            auto& Res = this->m_Desc.Resources[i];
            if (Res.VarType == VarType)
                Count += Res.ArraySize;
        }
        return Count;
    }

    PipelineResourceDesc GetResource(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 Index) const
    {
        return {};
    }

    size_t GetHash() const { return m_Hash; }

    PIPELINE_TYPE GetPipelineType() const { return m_PipelineType; }

protected:
#define LOG_PRS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of a pipeline resource signature '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

    void ReserveSpaceForDescription(FixedLinearAllocator& Allocator, const PipelineResourceSignatureDesc& Desc) noexcept(false)
    {
        if (Desc.NumResources == 0)
            LOG_PRS_ERROR_AND_THROW("AZ TODO");

        Allocator.AddSpace<PipelineResourceDesc>(Desc.NumResources);
        Allocator.AddSpace<ImmutableSamplerDesc>(Desc.NumImmutableSamplers);

        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& Res = Desc.Resources[i];

            if (Res.Name == nullptr)
                LOG_PRS_ERROR_AND_THROW("AZ TODO");

            if (Res.ShaderStages == SHADER_TYPE_UNKNOWN)
                LOG_PRS_ERROR_AND_THROW("AZ TODO");

            if (Res.ArraySize == 0)
                LOG_PRS_ERROR_AND_THROW("AZ TODO");

            Allocator.AddSpaceForString(Res.Name);
        }

        for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
        {
            if (Desc.ImmutableSamplers[i].SamplerOrTextureName == nullptr)
                LOG_PRS_ERROR_AND_THROW("AZ TODO");

            Allocator.AddSpaceForString(Desc.ImmutableSamplers[i].SamplerOrTextureName);
        }
    }

    void CopyDescription(FixedLinearAllocator& Allocator, const PipelineResourceSignatureDesc& Desc) noexcept(false)
    {
        PipelineResourceDesc* pResources = Allocator.Allocate<PipelineResourceDesc>(this->m_Desc.NumResources);
        ImmutableSamplerDesc* pSamplers  = Allocator.Allocate<ImmutableSamplerDesc>(this->m_Desc.NumImmutableSamplers);

        m_Hash = ComputeHash(Desc.NumResources, Desc.NumImmutableSamplers);

        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            auto& Dst = pResources[i];
            Dst       = Desc.Resources[i];
            Dst.Name  = Allocator.CopyString(Desc.Resources[i].Name);

            HashCombine(m_Hash, CStringHash<char>{}(Dst.Name), Dst.ArraySize, Dst.ResourceType, Dst.ShaderStages, Dst.VarType, Dst.Flags);
        }

        for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
        {
            auto& Dst                = pSamplers[i];
            Dst                      = Desc.ImmutableSamplers[i];
            Dst.SamplerOrTextureName = Allocator.CopyString(Desc.ImmutableSamplers[i].SamplerOrTextureName);

            //HashCombine(m_Hash, CStringHash<char>{}(Dst.Name), Dst.ShaderStages, Dst.Desc); // AZ TODO
        }

        this->m_Desc.Resources         = pResources;
        this->m_Desc.ImmutableSamplers = pSamplers;
    }

    void Destruct()
    {
        VERIFY(!m_IsDestructed, "This object has already been destructed");

        this->m_Desc.Resources         = nullptr;
        this->m_Desc.ImmutableSamplers = nullptr;

#if DILIGENT_DEBUG
        m_IsDestructed = true;
#endif
    }

#undef LOG_PRS_ERROR_AND_THROW

    Int8 GetStaticVariableCountHelper(SHADER_TYPE ShaderType, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& StaticVarIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, m_PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(m_PipelineType), " pipeline resource signature '", this->m_Desc.Name, "'.");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, m_PipelineType);
        const auto LayoutInd     = StaticVarIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return LayoutInd;
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
        const auto LayoutInd     = StaticVarIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return LayoutInd;
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
        const auto LayoutInd     = StaticVarIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'.");
        }

        return LayoutInd;
    }

protected:
    size_t m_Hash = 0;

    PIPELINE_TYPE m_PipelineType = PIPELINE_TYPE(0xFF);

#ifdef DILIGENT_DEBUG
    bool m_IsDestructed = false;
#endif
};

} // namespace Diligent
