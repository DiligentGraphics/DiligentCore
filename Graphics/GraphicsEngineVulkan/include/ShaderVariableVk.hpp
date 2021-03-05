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
/// Declaration of Diligent::ShaderVariableManagerVk and Diligent::ShaderVariableVkImpl classes

//
//  * ShaderVariableManagerVk keeps the list of variables of specific types
//  * Every ShaderVariableVkImpl references ResourceAttribs by index from PipelineResourceSignatureVkImpl
//  * ShaderVariableManagerVk keeps reference to ShaderResourceCacheVk
//  * ShaderVariableManagerVk is used by PipelineResourceSignatureVkImpl to manage static resources and by
//    ShaderResourceBindingVkImpl to manage mutable and dynamic resources
//
//          __________________________                             __________________________________________________________________________
//         |                          |                           |                           |                            |                 |
//    .----|  ShaderVariableManagerVk |-------------------------->|  ShaderVariableVkImpl[0]  |   ShaderVariableVkImpl[1]  |     ...         |
//    |    |__________________________|                           |___________________________|____________________________|_________________|
//    |                |                                                              \                          |
//    |           m_pSignature                                                     m_ResIndex               m_ResIndex
//    |                |                                                                \                        |
//    |     ___________V_____________________                      ______________________V_______________________V____________________________
//    |    |                                 | m_pResourceAttribs |                  |                |               |                     |
//    |    | PipelineResourceSignatureVkImpl |------------------->|    Resource[0]   |   Resource[1]  |       ...     |  Resource[s+m+d-1]  |
//    |    |_________________________________|                    |__________________|________________|_______________|_____________________|
//    |                                                                  |                                                        |
//    |                                                                  |                                                        |
//    |                                                                  | (DescriptorSet, CacheOffset)                          / (DescriptorSet, CacheOffset)
//    |                                                                   \                                                     /
//    |     __________________________                             ________V___________________________________________________V_______
//    |    |                          |                           |                                                                    |
//    '--->|   ShaderResourceCacheVk  |-------------------------->|                                   Resources                        |
//         |__________________________|                           |____________________________________________________________________|
//
//

#include <memory>

#include "ShaderResourceVariableBase.hpp"
#include "ShaderResourceCacheVk.hpp"
#include "PipelineResourceSignatureVkImpl.hpp"

namespace Diligent
{

class ShaderVariableVkImpl;

// sizeof(ShaderVariableManagerVk) == 40 (x64, msvc, Release)
class ShaderVariableManagerVk
{
public:
    ShaderVariableManagerVk(IObject&               Owner,
                            ShaderResourceCacheVk& ResourceCache) noexcept :
        m_Owner{Owner},
        m_ResourceCache{ResourceCache}
    {}

    void Initialize(const PipelineResourceSignatureVkImpl& Signature,
                    IMemoryAllocator&                      Allocator,
                    const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                    Uint32                                 NumAllowedTypes,
                    SHADER_TYPE                            ShaderType);

    ~ShaderVariableManagerVk();

    void Destroy(IMemoryAllocator& Allocator);

    ShaderVariableVkImpl* GetVariable(const Char* Name) const;
    ShaderVariableVkImpl* GetVariable(Uint32 Index) const;

    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags) const;

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureVkImpl& Signature,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                        Uint32                                 NumAllowedTypes,
                                        SHADER_TYPE                            ShaderStages,
                                        Uint32&                                NumVariables);

    Uint32 GetVariableCount() const { return m_NumVariables; }

    IObject& GetOwner() { return m_Owner; }

private:
    friend ShaderVariableVkImpl;
    using ResourceAttribs = PipelineResourceSignatureVkImpl::ResourceAttribs;

    Uint32 GetVariableIndex(const ShaderVariableVkImpl& Variable);

    const PipelineResourceDesc& GetResourceDesc(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetResourceDesc(Index);
    }
    const ResourceAttribs& GetAttribs(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetResourceAttribs(Index);
    }

    template <typename HandlerType>
    static void ProcessSignatureResources(const PipelineResourceSignatureVkImpl& Signature,
                                          const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                          Uint32                                 NumAllowedTypes,
                                          SHADER_TYPE                            ShaderStages,
                                          HandlerType                            Handler);

private:
    PipelineResourceSignatureVkImpl const* m_pSignature = nullptr;

    IObject& m_Owner;

    // Variable mgr is owned by either Pipeline Resource Signature (in which case m_ResourceCache references
    // static resource cache owned by the same PRS object), or by SRB object (in which case
    // m_ResourceCache references the cache in the SRB). Thus the cache and the signature
    // (which the variables reference) are guaranteed to be alive while the manager is alive.
    ShaderResourceCacheVk& m_ResourceCache;

    // Memory is allocated through the allocator provided by the resource signature. If allocation granularity > 1, fixed block
    // memory allocator is used. This ensures that all resources from different shader resource bindings reside in
    // continuous memory. If allocation granularity == 1, raw allocator is used.
    ShaderVariableVkImpl* m_pVariables   = nullptr;
    Uint32                m_NumVariables = 0;

#ifdef DILIGENT_DEBUG
    IMemoryAllocator* m_pDbgAllocator = nullptr;
#endif
};

// sizeof(ShaderVariableVkImpl) == 24 (x64)
class ShaderVariableVkImpl final : public ShaderVariableBase<ShaderVariableManagerVk, IShaderResourceVariable>
{
public:
    using TBase = ShaderVariableBase<ShaderVariableManagerVk, IShaderResourceVariable>;

    ShaderVariableVkImpl(ShaderVariableManagerVk& ParentManager,
                         Uint32                   ResIndex) :
        TBase{ParentManager},
        m_ResIndex{ResIndex}
    {}

    // clang-format off
    ShaderVariableVkImpl            (const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl            (ShaderVariableVkImpl&&)      = delete;
    ShaderVariableVkImpl& operator= (const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl& operator= (ShaderVariableVkImpl&&)      = delete;
    // clang-format on

    virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
    {
        return GetDesc().VarType;
    }

    virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
    {
        BindResource(pObject, 0);
    }

    virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                             Uint32                FirstElement,
                                             Uint32                NumElements) override final;

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(ShaderResourceDesc& ResourceDesc) const override final
    {
        const auto& Desc       = GetDesc();
        ResourceDesc.Name      = Desc.Name;
        ResourceDesc.Type      = Desc.ResourceType;
        ResourceDesc.ArraySize = Desc.ArraySize;
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetIndex() const override final
    {
        return m_ParentManager.GetVariableIndex(*this);
    }

    virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
    {
        return m_ParentManager.m_pSignature->IsBound(ArrayIndex, m_ResIndex, m_ParentManager.m_ResourceCache);
    }

    const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResourceDesc(m_ResIndex); }

    void BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const
    {
        m_ParentManager.m_pSignature->BindResource(pObj, ArrayIndex, m_ResIndex, m_ParentManager.m_ResourceCache);
    }

private:
    using ResourceAttribs = PipelineResourceSignatureVkImpl::ResourceAttribs;

    const ResourceAttribs& GetAttribs() const { return m_ParentManager.GetAttribs(m_ResIndex); }

private:
    const Uint32 m_ResIndex; // Index in Signatures' m_Desc.Resources
};

} // namespace Diligent
