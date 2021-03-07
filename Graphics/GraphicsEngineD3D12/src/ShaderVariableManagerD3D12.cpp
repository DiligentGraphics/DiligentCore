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

#include "ShaderVariableManagerD3D12.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"

namespace Diligent
{

template <typename HandlerType>
void ShaderVariableManagerD3D12::ProcessSignatureResources(const PipelineResourceSignatureD3D12Impl& Signature,
                                                           const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                                           Uint32                                    NumAllowedTypes,
                                                           SHADER_TYPE                               ShaderStages,
                                                           HandlerType                               Handler)
{
    const Uint32 AllowedTypeBits       = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    const bool   UsingCombinedSamplers = Signature.IsUsingCombinedSamplers();

    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        if (IsAllowedType(VarType, AllowedTypeBits))
        {
            const auto ResIdxRange = Signature.GetResourceIndexRange(VarType);
            for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
            {
                const auto& Res  = Signature.GetResourceDesc(r);
                const auto& Attr = Signature.GetResourceAttribs(r);
                VERIFY_EXPR(Res.VarType == VarType);

                if (!(Res.ShaderStages & ShaderStages))
                    continue;

                if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER &&
                    (UsingCombinedSamplers || Attr.IsImmutableSamplerAssigned()))
                {
                    // Skip samplers combined with textures and immutable samplers
                    continue;
                }

                Handler(r);
            }
        }
    }
}

size_t ShaderVariableManagerD3D12::GetRequiredMemorySize(const PipelineResourceSignatureD3D12Impl& Signature,
                                                         const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                                         Uint32                                    NumAllowedTypes,
                                                         SHADER_TYPE                               ShaderStages,
                                                         Uint32&                                   NumVariables)
{
    NumVariables = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderStages,
                              [&NumVariables](Uint32) //
                              {
                                  ++NumVariables;
                              });

    return NumVariables * sizeof(ShaderVariableD3D12Impl);
}

// Creates shader variable for every resource from Signature whose type is one AllowedVarTypes
void ShaderVariableManagerD3D12::Initialize(const PipelineResourceSignatureD3D12Impl& Signature,
                                            IMemoryAllocator&                         Allocator,
                                            const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                            Uint32                                    NumAllowedTypes,
                                            SHADER_TYPE                               ShaderType)
{
#ifdef DILIGENT_DEBUG
    m_pDbgAllocator = &Allocator;
#endif

    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    VERIFY_EXPR(m_NumVariables == 0);
    const auto MemSize = GetRequiredMemorySize(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType, m_NumVariables);

    if (m_NumVariables == 0)
        return;

    auto* pRawMem = ALLOCATE_RAW(Allocator, "Raw memory buffer for shader variables", MemSize);
    m_pVariables  = reinterpret_cast<ShaderVariableD3D12Impl*>(pRawMem);

    Uint32 VarInd = 0;
    ProcessSignatureResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType,
                              [this, &VarInd](Uint32 ResIndex) //
                              {
                                  ::new (m_pVariables + VarInd) ShaderVariableD3D12Impl{*this, ResIndex};
                                  ++VarInd;
                              });
    VERIFY_EXPR(VarInd == m_NumVariables);

    m_pSignature = &Signature;
}

ShaderVariableManagerD3D12::~ShaderVariableManagerD3D12()
{
    VERIFY(m_pVariables == nullptr, "Destroy() has not been called");
}

void ShaderVariableManagerD3D12::Destroy(IMemoryAllocator& Allocator)
{
    if (m_pVariables != nullptr)
    {
        VERIFY(m_pDbgAllocator == &Allocator, "Incosistent alloctor");

        for (Uint32 v = 0; v < m_NumVariables; ++v)
            m_pVariables[v].~ShaderVariableD3D12Impl();
        Allocator.Free(m_pVariables);
        m_pVariables = nullptr;
    }
}

const PipelineResourceDesc& ShaderVariableManagerD3D12::GetResourceDesc(Uint32 Index) const
{
    VERIFY_EXPR(m_pSignature != nullptr);
    return m_pSignature->GetResourceDesc(Index);
}

const ShaderVariableManagerD3D12::ResourceAttribs& ShaderVariableManagerD3D12::GetResourceAttribs(Uint32 Index) const
{
    VERIFY_EXPR(m_pSignature != nullptr);
    return m_pSignature->GetResourceAttribs(Index);
}


ShaderVariableD3D12Impl* ShaderVariableManagerD3D12::GetVariable(const Char* Name) const
{
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto& Var = m_pVariables[v];
        if (strcmp(Var.GetDesc().Name, Name) == 0)
            return &Var;
    }
    return nullptr;
}


ShaderVariableD3D12Impl* ShaderVariableManagerD3D12::GetVariable(Uint32 Index) const
{
    if (Index >= m_NumVariables)
    {
        LOG_ERROR("Index ", Index, " is out of range");
        return nullptr;
    }

    return m_pVariables + Index;
}

Uint32 ShaderVariableManagerD3D12::GetVariableIndex(const ShaderVariableD3D12Impl& Variable)
{
    if (m_pVariables == nullptr)
    {
        LOG_ERROR("This shader variable manager has no variables");
        return ~0u;
    }

    const auto Offset = reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<Uint8*>(m_pVariables);
    DEV_CHECK_ERR(Offset % sizeof(ShaderVariableD3D12Impl) == 0, "Offset is not multiple of ShaderVariableD3D12Impl class size");
    auto Index = static_cast<Uint32>(Offset / sizeof(ShaderVariableD3D12Impl));
    if (Index < m_NumVariables)
        return Index;
    else
    {
        LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader variable manager");
        return ~0u;
    }
}

void ShaderVariableManagerD3D12::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags)
{
    DEV_CHECK_ERR(pResourceMapping != nullptr, "Failed to bind resources: resource mapping is null");

    if ((Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0)
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        m_pVariables[v].BindResources<ShaderVariableD3D12Impl>(pResourceMapping, Flags);
    }
}

void ShaderVariableD3D12Impl::SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)
{
    const auto& ResDesc = GetDesc();
    VerifyAndCorrectSetArrayArguments(ResDesc.Name, ResDesc.ArraySize, FirstElement, NumElements);

    for (Uint32 Elem = 0; Elem < NumElements; ++Elem)
        BindResource(ppObjects[Elem], FirstElement + Elem);
}

bool ShaderVariableD3D12Impl::IsBound(Uint32 ArrayIndex) const
{
    return m_ParentManager.m_pSignature->IsBound(ArrayIndex, m_ResIndex, m_ParentManager.m_ResourceCache);
}

void ShaderVariableD3D12Impl::BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const
{
    m_ParentManager.m_pSignature->BindResource(pObj, ArrayIndex, m_ResIndex, m_ParentManager.m_ResourceCache);
}

} // namespace Diligent
