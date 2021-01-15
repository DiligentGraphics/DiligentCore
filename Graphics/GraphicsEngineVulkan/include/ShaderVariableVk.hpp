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
//  * ShaderVariableManagerVk keeps list of variables of specific types
//  * Every ShaderVariableVkImpl references VkResource from ShaderResourceLayoutVk
//  * ShaderVariableManagerVk keeps reference to ShaderResourceCacheVk
//  * ShaderVariableManagerVk is used by PipelineStateVkImpl to manage static resources and by
//    ShaderResourceBindingVkImpl to manage mutable and dynamic resources
//
//          __________________________                   __________________________________________________________________________
//         |                          |                 |                           |                            |                 |
//    .----|  ShaderVariableManagerVk |---------------->|  ShaderVariableVkImpl[0]  |   ShaderVariableVkImpl[1]  |     ...         |
//    |    |__________________________|                 |___________________________|____________________________|_________________|
//    |                                                                     \                          |
//    |                                                                     Ref                       Ref
//    |                                                                       \                        |
//    |     ___________________________                  ______________________V_______________________V____________________________
//    |    |                           |   unique_ptr   |                   |                 |               |                     |
//    |    | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//    |    |___________________________|                |___________________|_________________|_______________|_____________________|
//    |                                                        |                                                            |
//    |                                                        |                                                            |
//    |                                                        | (DescriptorSet, CacheOffset)                              / (DescriptorSet, CacheOffset)
//    |                                                         \                                                         /
//    |     __________________________                   ________V_______________________________________________________V_______
//    |    |                          |                 |                                                                        |
//    '--->|   ShaderResourceCacheVk  |---------------->|                                   Resources                            |
//         |__________________________|                 |________________________________________________________________________|
//
//

#include <memory>

#include "ShaderResourceVariableBase.hpp"
#include "ShaderResourceCacheVk.hpp"

namespace Diligent
{

class ShaderVariableVkImpl;
class PipelineResourceSignatureVkImpl;

// sizeof(ShaderVariableManagerVk) == 32 (x64, msvc, Release)
class ShaderVariableManagerVk
{
public:
    ShaderVariableManagerVk(IObject&               Owner,
                            ShaderResourceCacheVk& ResourceCache) noexcept :
        m_Owner{Owner},
        m_ResourceCache{ResourceCache}
    {}

    void Initialize(const PipelineResourceSignatureVkImpl& SrcLayout,
                    IMemoryAllocator&                      Allocator,
                    const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                    Uint32                                 NumAllowedTypes);

    ~ShaderVariableManagerVk();

    void DestroyVariables(IMemoryAllocator& Allocator);

    ShaderVariableVkImpl* GetVariable(const Char* Name) const;
    ShaderVariableVkImpl* GetVariable(Uint32 Index) const;

    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags) const;

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureVkImpl& Layout,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                        Uint32                                 NumAllowedTypes,
                                        Uint32&                                NumVariables);

    Uint32 GetVariableCount() const { return m_NumVariables; }

private:
    friend ShaderVariableVkImpl;
    using PackedBindingIndex = PipelineResourceSignatureVkImpl::PackedBindingIndex;

    Uint32 GetVariableIndex(const ShaderVariableVkImpl& Variable);

    const PipelineResourceDesc& GetResource(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetResource(Index);
    }
    const PackedBindingIndex& GetBinding(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetBinding(Index);
    }

private:
    PipelineResourceSignatureVkImpl const* m_pSignature = nullptr;

    IObject& m_Owner;

    // Variable mgr is owned by either Pipeline state object (in which case m_ResourceCache references
    // static resource cache owned by the same PSO object), or by SRB object (in which case
    // m_ResourceCache references the cache in the SRB). Thus the cache and the resource layout
    // (which the variables reference) are guaranteed to be alive while the manager is alive.
    ShaderResourceCacheVk& m_ResourceCache;

    // Memory is allocated through the allocator provided by the pipeline state. If allocation granularity > 1, fixed block
    // memory allocator is used. This ensures that all resources from different shader resource bindings reside in
    // continuous memory. If allocation granularity == 1, raw allocator is used.
    ShaderVariableVkImpl* m_pVariables   = nullptr;
    Uint32                m_NumVariables = 0;

#ifdef DILIGENT_DEBUG
    IMemoryAllocator* m_pDbgAllocator = nullptr;
#endif
};

// sizeof(ShaderVariableVkImpl) == 16 (x64)
class ShaderVariableVkImpl final : public IShaderResourceVariable
{
public:
    explicit ShaderVariableVkImpl(ShaderVariableManagerVk& ParentManager) :
        m_ParentManager{ParentManager}
    {}

    // clang-format off
    ShaderVariableVkImpl            (const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl            (ShaderVariableVkImpl&&)      = delete;
    ShaderVariableVkImpl& operator= (const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl& operator= (ShaderVariableVkImpl&&)      = delete;
    // clang-format on


    virtual IReferenceCounters* DILIGENT_CALL_TYPE GetReferenceCounters() const override final
    {
        return m_ParentManager.m_Owner.GetReferenceCounters();
    }

    virtual Atomics::Long DILIGENT_CALL_TYPE AddRef() override final
    {
        return m_ParentManager.m_Owner.AddRef();
    }

    virtual Atomics::Long DILIGENT_CALL_TYPE Release() override final
    {
        return m_ParentManager.m_Owner.Release();
    }

    void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_ShaderResourceVariable || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

    virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
    {
        return GetDesc().VarType;
    }

    virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final;

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
        VERIFY_EXPR(m_ParentManager.m_pVariables);
        return Uint32(size_t(this - m_ParentManager.m_pVariables));
    }

    virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final;

    String GetPrintName(Uint32 ArrayInd) const;

    RESOURCE_DIMENSION GetResourceDimension() const;

    bool IsMultisample() const;

private:
    friend ShaderVariableManagerVk;
    using PackedBindingIndex = PipelineResourceSignatureVkImpl::PackedBindingIndex;

    const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResource(GetIndex()); }
    const PackedBindingIndex&   GetBinding() const { return m_ParentManager.GetBinding(GetIndex()); }

    void BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const;

    struct UpdateInfo
    {
        ShaderResourceCacheVk::Resource&    DstRes;
        const VkDescriptorSet               vkDescrSet;
        const Uint32                        ArrayIndex;
        const SHADER_RESOURCE_VARIABLE_TYPE VarType;
        const Uint16                        Binding;
        const Uint8                         SamplerInd;
        char const* const                   Name;
    };

    void CacheUniformBuffer(IDeviceObject* pBuffer,
                            UpdateInfo&    Info,
                            Uint16&        DynamicBuffersCounter) const;

    void CacheStorageBuffer(IDeviceObject* pBufferView,
                            UpdateInfo&    Info,
                            Uint16&        DynamicBuffersCounter) const;

    void CacheTexelBuffer(IDeviceObject* pBufferView,
                          UpdateInfo&    Info,
                          Uint16&        DynamicBuffersCounter) const;

    template <typename TCacheSampler>
    void CacheImage(IDeviceObject* pTexView,
                    UpdateInfo&    Info,
                    TCacheSampler  CacheSampler) const;

    void CacheSeparateSampler(IDeviceObject* pSampler,
                              UpdateInfo&    Info) const;

    void CacheInputAttachment(IDeviceObject* pTexView,
                              UpdateInfo&    Info) const;

    void CacheAccelerationStructure(IDeviceObject* pTLAS,
                                    UpdateInfo&    Info) const;

    template <typename ObjectType, typename TPreUpdateObject>
    bool UpdateCachedResource(UpdateInfo&                 Info,
                              RefCntAutoPtr<ObjectType>&& pObject,
                              TPreUpdateObject            PreUpdateObject) const;

    bool IsImmutableSamplerAssigned() const;

    // Updates resource descriptor in the descriptor set
    inline void UpdateDescriptorHandle(UpdateInfo&                                         Info,
                                       const VkDescriptorImageInfo*                        pImageInfo,
                                       const VkDescriptorBufferInfo*                       pBufferInfo,
                                       const VkBufferView*                                 pTexelBufferView,
                                       const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo = nullptr) const;

    static constexpr Uint8 InvalidSamplerInd = PipelineResourceSignatureVkImpl::InvalidSamplerInd;

private:
    ShaderVariableManagerVk& m_ParentManager;
};

} // namespace Diligent
