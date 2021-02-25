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
/// Declaration of Diligent::ShaderVariableManagerD3D12 and Diligent::ShaderVariableD3D12Impl classes

//
//  * ShaderVariableManagerD3D12 keeps the list of variables of specific types
//  * Every ShaderVariableD3D12Impl references ResourceAttribs by index from PipelineResourceSignatureD3D12Impl
//  * ShaderVariableManagerD3D12 keeps reference to ShaderResourceCacheD3D12
//  * ShaderVariableManagerD3D12 is used by PipelineResourceSignatureD3D12Impl to manage static resources and by
//    ShaderResourceBindingD3D12Impl to manage mutable and dynamic resources
//
//            _____________________________                   ________________________________________________________________________________
//           |                             |                 |                              |                               |                 |
//      .----|  ShaderVariableManagerD3D12 |---------------->|  ShaderVariableD3D12Impl[0]  |   ShaderVariableD3D12Impl[1]  |     ...         |
//      |    |_____________________________|                 |______________________________|_______________________________|_________________|
//      |                |                                                    |                               |
//      |          m_pSignature                                          m_ResIndex                       m_ResIndex
//      |                |                                                    |                               |
//      |   _____________V____________________                      __________V_______________________________V_________________________________
//      |  |                                  | m_pResourceAttribs |                  |                  |             |                        |
//      |  |PipelineResourceSignatureD3D12Impl|------------------->|    Resource[0]   |    Resource[1]   |     ...     |   Resource[s+m+d-1]    |
//      |  |__________________________________|                    |__________________|__________________|_____________|________________________|
//      |                                                                |                                                    |
// m_ResourceCache                                                       |                                                    |
//      |                                                                | (RootTable, Offset)                               / (RootTable, Offset)
//      |                                                                \                                                  /
//      |     __________________________                   _______________V________________________________________________V_______
//      |    |                          |                 |                                                                        |
//      '--->| ShaderResourceCacheD3D12 |---------------->|                                   Resources                            |
//           |__________________________|                 |________________________________________________________________________|
//

#include "ShaderResourceVariableD3D.h"
#include "ShaderResourceVariableBase.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"

namespace Diligent
{

class ShaderVariableD3D12Impl;

// sizeof(ShaderVariableManagerD3D12) == 40 (x64, msvc, Release)
class ShaderVariableManagerD3D12
{
public:
    ShaderVariableManagerD3D12(IObject&                  Owner,
                               ShaderResourceCacheD3D12& ResourceCache) noexcept :
        m_Owner{Owner},
        m_ResourceCache{ResourceCache}
    {}

    // clang-format off
    ShaderVariableManagerD3D12            (const ShaderVariableManagerD3D12&)  = delete;
    ShaderVariableManagerD3D12            (      ShaderVariableManagerD3D12&&) = delete;
    ShaderVariableManagerD3D12& operator= (const ShaderVariableManagerD3D12&)  = delete;
    ShaderVariableManagerD3D12& operator= (      ShaderVariableManagerD3D12&&) = delete;
    // clang-format on

    void Initialize(const PipelineResourceSignatureD3D12Impl& Signature,
                    IMemoryAllocator&                         Allocator,
                    const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                    Uint32                                    NumAllowedTypes,
                    SHADER_TYPE                               ShaderStages);
    ~ShaderVariableManagerD3D12();

    void Destroy(IMemoryAllocator& Allocator);

    ShaderVariableD3D12Impl* GetVariable(const Char* Name) const;
    ShaderVariableD3D12Impl* GetVariable(Uint32 Index) const;

    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags);

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureD3D12Impl& Signature,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                        Uint32                                    NumAllowedTypes,
                                        SHADER_TYPE                               ShaderStages,
                                        Uint32&                                   NumVariables);

    Uint32 GetVariableCount() const { return m_NumVariables; }

    IObject& GetOwner() { return m_Owner; }

private:
    friend ShaderVariableD3D12Impl;
    using ResourceAttribs = PipelineResourceSignatureD3D12Impl::ResourceAttribs;

    Uint32 GetVariableIndex(const ShaderVariableD3D12Impl& Variable);

    const PipelineResourceDesc& GetResourceDesc(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature != nullptr);
        return m_pSignature->GetResourceDesc(Index);
    }
    const ResourceAttribs& GetResourceAttribs(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature != nullptr);
        return m_pSignature->GetResourceAttribs(Index);
    }

    template <typename HandlerType>
    static void ProcessSignatureResources(const PipelineResourceSignatureD3D12Impl& Signature,
                                          const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                          Uint32                                    NumAllowedTypes,
                                          SHADER_TYPE                               ShaderStages,
                                          HandlerType                               Handler);

private:
    PipelineResourceSignatureD3D12Impl const* m_pSignature = nullptr;

    IObject& m_Owner;

    // Variable manager is owned by either Pipeline Resource Signature (in which case m_ResourceCache references
    // static resource cache owned by the same signature object), or by SRB object (in which case
    // m_ResourceCache references the cache in the SRB). Thus the cache and the signature
    // (which the variables reference) are guaranteed to be alive while the manager is alive.
    ShaderResourceCacheD3D12& m_ResourceCache;

    // Memory is allocated through the allocator provided by the pipeline resource signature. If allocation
    // granularity > 1, fixed block memory allocator is used. This ensures that all resources from different
    // shader resource bindings reside in continuous memory. If allocation granularity == 1, raw allocator is used.
    ShaderVariableD3D12Impl* m_pVariables   = nullptr;
    Uint32                   m_NumVariables = 0;

#ifdef DILIGENT_DEBUG
    IMemoryAllocator* m_pDbgAllocator = nullptr;
#endif
};

// sizeof(ShaderVariableD3D12Impl) == 24 (x64)
class ShaderVariableD3D12Impl final : public ShaderVariableBase<ShaderVariableManagerD3D12, IShaderResourceVariableD3D>
{
public:
    using TBase = ShaderVariableBase<ShaderVariableManagerD3D12, IShaderResourceVariableD3D>;
    ShaderVariableD3D12Impl(ShaderVariableManagerD3D12& ParentManager,
                            Uint32                      ResIndex) :
        TBase{ParentManager},
        m_ResIndex{ResIndex}
    {}

    // clang-format off
    ShaderVariableD3D12Impl            (const ShaderVariableD3D12Impl&) = delete;
    ShaderVariableD3D12Impl            (ShaderVariableD3D12Impl&&)      = delete;
    ShaderVariableD3D12Impl& operator= (const ShaderVariableD3D12Impl&) = delete;
    ShaderVariableD3D12Impl& operator= (ShaderVariableD3D12Impl&&)      = delete;
    // clang-format on

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_ShaderResourceVariableD3D || IID == IID_ShaderResourceVariable || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

    virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
    {
        return GetDesc().VarType;
    }

    virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
    {
        BindResource(pObject, 0);
    }

    virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements) override final;

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

    virtual void DILIGENT_CALL_TYPE GetHLSLResourceDesc(HLSLShaderResourceDesc& HLSLResDesc) const override final
    {
        GetResourceDesc(HLSLResDesc);
        HLSLResDesc.ShaderRegister = GetAttribs().Register;
    }

    const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResourceDesc(m_ResIndex); }

    void BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const
    {
        m_ParentManager.m_pSignature->BindResource(pObj, ArrayIndex, m_ResIndex, m_ParentManager.m_ResourceCache);
    }

private:
    using ResourceAttribs = PipelineResourceSignatureD3D12Impl::ResourceAttribs;
    const ResourceAttribs& GetAttribs() const { return m_ParentManager.GetResourceAttribs(m_ResIndex); }

private:
    const Uint32 m_ResIndex;
};

} // namespace Diligent
