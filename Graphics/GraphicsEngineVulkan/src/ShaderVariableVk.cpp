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

#include "ShaderVariableVk.hpp"
#include "ShaderResourceVariableBase.hpp"
#include "PipelineResourceSignatureVkImpl.hpp"

namespace Diligent
{

template <typename HandlerType>
void ShaderVariableManagerVk::ProcessSignatureResources(const PipelineResourceSignatureVkImpl& Signature,
                                                        const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                                        Uint32                                 NumAllowedTypes,
                                                        SHADER_TYPE                            ShaderStages,
                                                        HandlerType                            Handler)
{
    const Uint32 AllowedTypeBits       = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    const bool   UsingSeparateSamplers = Signature.IsUsingSeparateSamplers();

    for (Uint32 var_type = 0; var_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++var_type)
    {
        const auto VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(var_type);
        if (IsAllowedType(VarType, AllowedTypeBits))
        {
            const auto ResIdxRange = Signature.GetResourceIndexRange(VarType);
            for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
            {
                const auto& Res  = Signature.GetResourceDesc(r);
                const auto& Attr = Signature.GetResourceAttribs(r);
                VERIFY_EXPR(Res.VarType == VarType);

                if ((Res.ShaderStages & ShaderStages) == 0)
                    continue;

                // When using HLSL-style combined image samplers, we need to skip separate samplers.
                // Also always skip immutable separate samplers.
                if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER &&
                    (!UsingSeparateSamplers || Attr.IsImmutableSamplerAssigned()))
                    continue;

                Handler(r);
            }
        }
    }
}

size_t ShaderVariableManagerVk::GetRequiredMemorySize(const PipelineResourceSignatureVkImpl& Signature,
                                                      const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                                      Uint32                                 NumAllowedTypes,
                                                      SHADER_TYPE                            ShaderStages,
                                                      Uint32&                                NumVariables)
{
    NumVariables = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderStages,
                              [&NumVariables](Uint32) //
                              {
                                  ++NumVariables;
                              });

    return NumVariables * sizeof(ShaderVariableVkImpl);
}

// Creates shader variable for every resource from SrcLayout whose type is one AllowedVarTypes
void ShaderVariableManagerVk::Initialize(const PipelineResourceSignatureVkImpl& Signature,
                                         IMemoryAllocator&                      Allocator,
                                         const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                         Uint32                                 NumAllowedTypes,
                                         SHADER_TYPE                            ShaderType)
{
#ifdef DILIGENT_DEBUG
    m_pDbgAllocator = &Allocator;
#endif

    VERIFY_EXPR(m_pSignature == nullptr);

    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    VERIFY_EXPR(m_NumVariables == 0);
    auto MemSize = GetRequiredMemorySize(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType, m_NumVariables);

    if (m_NumVariables == 0)
        return;

    auto* pRawMem = ALLOCATE_RAW(Allocator, "Raw memory buffer for shader variables", MemSize);
    m_pVariables  = reinterpret_cast<ShaderVariableVkImpl*>(pRawMem);

    Uint32 VarInd = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType,
                              [this, &VarInd](Uint32 ResIndex) //
                              {
                                  ::new (m_pVariables + VarInd) ShaderVariableVkImpl{*this, ResIndex};
                                  ++VarInd;
                              });
    VERIFY_EXPR(VarInd == m_NumVariables);

    m_pSignature = &Signature;
}

ShaderVariableManagerVk::~ShaderVariableManagerVk()
{
    VERIFY(m_pVariables == nullptr, "DestroyVariables() has not been called");
}

void ShaderVariableManagerVk::DestroyVariables(IMemoryAllocator& Allocator)
{
    if (m_pVariables != nullptr)
    {
        VERIFY(m_pDbgAllocator == &Allocator, "Incosistent alloctor");

        for (Uint32 v = 0; v < m_NumVariables; ++v)
            m_pVariables[v].~ShaderVariableVkImpl();
        Allocator.Free(m_pVariables);
        m_pVariables = nullptr;
    }
}

ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(const Char* Name) const
{
    ShaderVariableVkImpl* pVar = nullptr;
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto&       Var = m_pVariables[v];
        const auto& Res = Var.GetDesc();
        if (strcmp(Res.Name, Name) == 0)
        {
            pVar = &Var;
            break;
        }
    }
    return pVar;
}


ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(Uint32 Index) const
{
    if (Index >= m_NumVariables)
    {
        LOG_ERROR("Index ", Index, " is out of range");
        return nullptr;
    }

    return m_pVariables + Index;
}

Uint32 ShaderVariableManagerVk::GetVariableIndex(const ShaderVariableVkImpl& Variable)
{
    if (m_pVariables == nullptr)
    {
        LOG_ERROR("This shader variable manager has no variables");
        return static_cast<Uint32>(-1);
    }

    auto Offset = reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<Uint8*>(m_pVariables);
    VERIFY(Offset % sizeof(ShaderVariableVkImpl) == 0, "Offset is not multiple of ShaderVariableVkImpl class size");
    auto Index = static_cast<Uint32>(Offset / sizeof(ShaderVariableVkImpl));
    if (Index < m_NumVariables)
        return Index;
    else
    {
        LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader variable manager");
        return static_cast<Uint32>(-1);
    }
}

void ShaderVariableManagerVk::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags) const
{
    if (!pResourceMapping)
    {
        LOG_ERROR_MESSAGE("Failed to bind resources: resource mapping is null");
        return;
    }

    if ((Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0)
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto&       Var  = m_pVariables[v];
        const auto& Res  = Var.GetDesc();
        const auto& Attr = Var.GetAttribs();

        // There should be no immutable separate samplers
        VERIFY(Attr.GetDescriptorType() != DescriptorType::Sampler || !Attr.IsImmutableSamplerAssigned(),
               "There must be no shader resource variables for immutable separate samplers");

        if ((Flags & (1u << Res.VarType)) == 0)
            continue;

        for (Uint32 ArrInd = 0; ArrInd < Res.ArraySize; ++ArrInd)
        {
            if ((Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) && Var.IsBound(ArrInd))
                continue;

            const auto*                  VarName = Res.Name;
            RefCntAutoPtr<IDeviceObject> pObj;
            pResourceMapping->GetResource(VarName, &pObj, ArrInd);
            if (pObj)
            {
                Var.BindResource(pObj, ArrInd);
            }
            else
            {
                if ((Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !Var.IsBound(ArrInd))
                {
                    LOG_ERROR_MESSAGE("Unable to bind resource to shader variable '",
                                      GetShaderResourcePrintName(Res, ArrInd),
                                      "': resource is not found in the resource mapping. "
                                      "Do not use BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED flag to suppress the message if this is not an issue.");
                }
            }
        }
    }
}

void ShaderVariableVkImpl::Set(IDeviceObject* pObject)
{
    BindResource(pObject, 0);
}

void ShaderVariableVkImpl::SetArray(IDeviceObject* const* ppObjects,
                                    Uint32                FirstElement,
                                    Uint32                NumElements)
{
    const auto& ResDesc = GetDesc();
    VerifyAndCorrectSetArrayArguments(ResDesc.Name, ResDesc.ArraySize, FirstElement, NumElements);

    for (Uint32 Elem = 0; Elem < NumElements; ++Elem)
        BindResource(ppObjects[Elem], FirstElement + Elem);
}

bool ShaderVariableVkImpl::IsBound(Uint32 ArrayIndex) const
{
    auto* pSignature    = m_ParentManager.m_pSignature;
    auto& ResourceCache = m_ParentManager.m_ResourceCache;

    return pSignature->IsBound(ArrayIndex, m_ResIndex, ResourceCache);
}

void ShaderVariableVkImpl::BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const
{
    auto* pSignature    = m_ParentManager.m_pSignature;
    auto& ResourceCache = m_ParentManager.m_ResourceCache;

    pSignature->BindResource(pObj, ArrayIndex, m_ResIndex, ResourceCache);
}

} // namespace Diligent
