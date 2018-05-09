/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
/// Declaration of Diligent::ShaderResourceLayoutVk class

// All resources are stored in a single continuous chunk of memory using the following layout:
//
//   m_ResourceBuffer                                                                                                                             
//      |                                                                                                                                         
//      |   VkResource[0]  ...  VkResource[s-1]   |   VkResource[s]  ...  VkResource[s+m-1]   |   VkResource[s+m]  ...  VkResource[s+m+d-1]   ||
//      |                                         |                                           |                                               ||
//      |        SHADER_VARIABLE_TYPE_STATIC      |          SHADER_VARIABLE_TYPE_MUTABLE     |            SHADER_VARIABLE_TYPE_DYNAMIC       ||
//      |                                         |                                           |                                               ||
//
//      s == m_NumResources[SHADER_VARIABLE_TYPE_STATIC]
//      m == m_NumResources[SHADER_VARIABLE_TYPE_MUTABLE]
//      d == m_NumResources[SHADER_VARIABLE_TYPE_DYNAMIC]
//
//
//   Memory buffer is allocated through the allocator provided by the pipeline state. If allocation granularity > 1, fixed block
//   memory allocator is used. This ensures that all resources from different shader resource bindings reside in
//   continuous memory. If allocation granularity == 1, raw allocator is used.
//
//
//   Every VkResource structure holds a reference to SPIRVShaderResourceAttribs structure from ShaderResources.
//   ShaderResourceLayoutVk holds shared pointer to ShaderResourcesVk instance. Note that ShaderResources::SamplerId 
//   references a sampler in ShaderResources, while VkResource::SamplerId references a sampler in ShaderResourceLayoutVk, 
//   and the two are not the same
//
//    ______________________                  ________________________________________________________________________
//   |                      |  unique_ptr    |        |         |          |          |       |         |             |
//   | SPIRVShaderResources |--------------->|   UBs  |   SBs   | StrgImgs | SmplImgs |  ACs  | SepImgs | SepSamplers |
//   |______________________|                |________|_________|__________|__________|_______|_________|_____________|
//            A                                         A                       A
//            |                                          \                      |
//            |shared_ptr                                Ref                   Ref                     
//    ________|__________________                  ________\____________________|_____________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                     |
//   | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//   |___________________________|                |___________________|_________________|_______________|_____________________|
//            |                                          |                                                            |              
//            | Raw ptr                                  |                                                            |     
//            |                                          |                                                           /     
//            |                                           \                                                         /     
//    ________V_________________                   ________V_______________________________________________________V_______                     
//   |                          |                 |                                                                        |                   
//   |   ShaderResourceCacheVk  |---------------->|                                   Resources                            | 
//   |__________________________|                 |________________________________________________________________________|                   
//
//   Resources in the resource cache are identified by the descriptor set index and the offset in from the set start
//
//                                
//    ShaderResourceLayoutVk is used as follows:
//    * Every pipeline state object (PipelineStateVkImpl) maintains shader resource layout for every active shader stage
//      ** These resource layouts are not bound to a resource cache and are used as reference layouts for shader resource binding objects
//      ** All variable types are preserved
//      ** Bindings, descriptor sets and offsets are assigned during the initialization
//      ** Resource cache is not assigned
//    * Every shader object (ShaderVkImpl) contains shader resource layout that facilitates management of static shader resources
//      ** The resource layout defines artificial layout and is bound to a resource cache that actually holds references to resources
//      ** Resource cache is assigned and initialized
//    * Every shader resource binding object (ShaderResourceBindingVkImpl) encompasses shader resource layout for every active shader 
//      stage in the parent pipeline state
//      ** Resource layouts are initialized by clonning reference layouts from the pipeline state object and are bound to the resource 
//         cache that holds references to resources set by the application
//      ** All shader variable types are clonned
//      ** Resource cache is assigned, but not initialized; Initialization is performed by the pipeline layout object
//

#include <array>
#include <memory>
#include <unordered_map>

// Set this define to 1 to use unordered_map to store shader variables. 
// Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
#define USE_VARIABLE_HASH_MAP 0


#include "ShaderBase.h"
#include "HashUtils.h"
#include "ShaderResourceCacheVk.h"
#include "SPIRVShaderResources.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"

#ifdef _DEBUG
#   define VERIFY_SHADER_BINDINGS
#endif

namespace Diligent
{

/// Diligent::ShaderResourceLayoutVk class
// sizeof(ShaderResourceLayoutVk)==?? (MS compiler, x64)
class ShaderResourceLayoutVk
{
public:
    ShaderResourceLayoutVk(IObject &Owner, IMemoryAllocator &ResourceLayoutDataAllocator);

    // This constructor is used by ShaderResourceBindingVkImpl to clone layout from the reference layout in PipelineStateVkImpl. 
    // Descriptor sets and bindings must be correct. Resource cache is assigned, but not initialized.
    ShaderResourceLayoutVk(IObject &Owner, 
                           const ShaderResourceLayoutVk& SrcLayout, 
                           IMemoryAllocator &ResourceLayoutDataAllocator,
                           const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                           Uint32 NumAllowedTypes, 
                           ShaderResourceCacheVk &ResourceCache);

    ShaderResourceLayoutVk(const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk(ShaderResourceLayoutVk&&) = delete;
    ShaderResourceLayoutVk& operator =(const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk& operator =(ShaderResourceLayoutVk&&) = delete;
    
    ~ShaderResourceLayoutVk();

    //  The method is called by
    //  - ShaderVkImpl class instance to initialize static resource layout and initialize shader resource cache
    //    to hold static resources
    //  - PipelineStateVkImpl class instance to reference all types of resources (static, mutable, dynamic). 
    //    Root indices and descriptor table offsets are assigned during the initialization; 
    //    no shader resource cache is provided
    void Initialize(const VulkanUtilities::VulkanLogicalDevice&         LogicalDevice,
                    const std::shared_ptr<const SPIRVShaderResources>&  pSrcResources,
                    IMemoryAllocator&                                   LayoutDataAllocator,
                    const SHADER_VARIABLE_TYPE*                         AllowedVarTypes,
                    Uint32                                              NumAllowedTypes, 
                    ShaderResourceCacheVk*                              pResourceCache,
                    std::vector<uint32_t>*                              pSPIRV,
                    class PipelineLayout*                               pPipelineLayout);

    // sizeof(VkResource) == ?? (x64)
    struct VkResource : IShaderVariable
    {
        VkResource(const VkResource&)              = delete;
        VkResource(VkResource&&)                   = delete;
        VkResource& operator = (const VkResource&) = delete;
        VkResource& operator = (VkResource&&)      = delete;

        const Uint16 Binding;
        const Uint16 DescriptorSet;
        const Uint32 CacheOffset; // Offset from the beginning of the cached descriptor set
        const SPIRVShaderResourceAttribs &SpirvAttribs;
        ShaderResourceLayoutVk &ParentResLayout;

        VkResource(ShaderResourceLayoutVk&              _ParentLayout,
                   const SPIRVShaderResourceAttribs&    _SpirvAttribs,
                   uint32_t                             _Binding,
                   uint32_t                             _DescriptorSet,
                   Uint32                               _CacheOffset) :
            Binding         (static_cast<decltype(Binding)>(_Binding)),
            DescriptorSet   (static_cast<decltype(DescriptorSet)>(_DescriptorSet)),
            CacheOffset     (_CacheOffset),
            SpirvAttribs    (_SpirvAttribs),
            ParentResLayout (_ParentLayout)
        {
            VERIFY(_Binding <= std::numeric_limits<decltype(Binding)>::max(), "Binding (", _Binding, ") exceeds representable max value", std::numeric_limits<decltype(Binding)>::max() );
            VERIFY(_DescriptorSet <= std::numeric_limits<decltype(DescriptorSet)>::max(), "Descriptor set (", _DescriptorSet, ") exceeds representable max value", std::numeric_limits<decltype(DescriptorSet)>::max());
        }

        VkResource(ShaderResourceLayoutVk&  _ParentLayout,
                   const VkResource&        _SrcRes) :
            Binding         (_SrcRes.Binding),
            DescriptorSet   (_SrcRes.DescriptorSet),
            CacheOffset     (_SrcRes.CacheOffset),
            SpirvAttribs    (_SrcRes.SpirvAttribs),
            ParentResLayout (_ParentLayout)
        {
        }

        virtual IReferenceCounters* GetReferenceCounters()const override final
        {
            return ParentResLayout.GetOwner().GetReferenceCounters();
        }

        virtual Atomics::Long AddRef()override final
        {
            return ParentResLayout.GetOwner().AddRef();
        }

        virtual Atomics::Long Release()override final
        {
            return ParentResLayout.GetOwner().Release();
        }

        void QueryInterface(const INTERFACE_ID &IID, IObject **ppInterface)override final
        {
            if (ppInterface == nullptr)
                return;

            *ppInterface = nullptr;
            if (IID == IID_ShaderVariable || IID == IID_Unknown)
            {
                *ppInterface = this;
                (*ppInterface)->AddRef();
            }
        }

        bool IsBound(Uint32 ArrayIndex);
        // Non-virtual function
        void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutVk *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 Elem = 0; Elem < NumElements; ++Elem)
                BindResource(ppObjects[Elem], FirstElement+Elem, nullptr);
        }

    private:
        void CacheBuffer(IDeviceObject*                     pBuffer, 
                         ShaderResourceCacheVk::Resource&   DstRes, 
                         VkDescriptorSet                    vkDescrSet,
                         Uint32                             ArrayInd);

        void CacheTexelBuffer(IDeviceObject*                     pBufferView, 
                              ShaderResourceCacheVk::Resource&   DstRes, 
                              VkDescriptorSet                    vkDescrSet,
                              Uint32                             ArrayInd);

        void CacheImage(IDeviceObject*                   pTexView,
                        ShaderResourceCacheVk::Resource& DstRes,
                        VkDescriptorSet                  vkDescrSet,
                        Uint32                           ArrayInd);

        void CacheSeparateSampler(IDeviceObject*                   pSampler,
                                  ShaderResourceCacheVk::Resource& DstRes,
                                  VkDescriptorSet                  vkDescrSet,
                                  Uint32                           ArrayInd);

        inline void UpdateDescriptorHandle(VkDescriptorSet                  vkDescrSet,
                                           uint32_t                         ArrayElement,
                                           const VkDescriptorImageInfo*     pImageInfo,
                                           const VkDescriptorBufferInfo*    pBufferInfo,
                                           const VkBufferView*              pTexelBufferView);

        bool UpdateCachedResource(ShaderResourceCacheVk::Resource&   DstRes,
                                  Uint32                             ArrayInd,
                                  IDeviceObject*                     pObject, 
                                  INTERFACE_ID                       InterfaceId,
                                  const char*                        ResourceName);
    };

    void InitializeStaticResources(const ShaderResourceLayoutVk &SrcLayout);

    // dbgResourceCache is only used for sanity check and as a remainder that the resource cache must be alive
    // while Layout is alive
    void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheVk *dbgResourceCache );
    IShaderVariable* GetShaderVariable( const Char* Name );

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyBindings()const;
#endif

    IObject& GetOwner(){return m_Owner;}

private:
    void InitVariablesHashMap();

    const Char* GetShaderName()const;

    // There is no need to use shared ptr as referenced resource cache is either part of the
    // parent ShaderVkImpl object or ShaderResourceBindingVkImpl object
    ShaderResourceCacheVk *m_pResourceCache;

    std::unique_ptr<void, STDDeleterRawMem<void> > m_ResourceBuffer;
    std::array<Uint16, SHADER_VARIABLE_TYPE_NUM_TYPES> m_NumResources = {};

    Uint32 GetResourceOffset(SHADER_VARIABLE_TYPE VarType, Uint32 r)const
    {
        VERIFY_EXPR( r < m_NumResources[VarType] );
        static_assert(SHADER_VARIABLE_TYPE_STATIC == 0, "SHADER_VARIABLE_TYPE_STATIC == 0 expected");
        r += (VarType > SHADER_VARIABLE_TYPE_STATIC) ? m_NumResources[SHADER_VARIABLE_TYPE_STATIC] : 0;
        static_assert(SHADER_VARIABLE_TYPE_MUTABLE == 1, "SHADER_VARIABLE_TYPE_MUTABLE == 1 expected");
        r += (VarType > SHADER_VARIABLE_TYPE_MUTABLE) ? m_NumResources[SHADER_VARIABLE_TYPE_MUTABLE] : 0;
        return r;
    }
    VkResource& GetResource(SHADER_VARIABLE_TYPE VarType, Uint32 r)
    {
        VERIFY_EXPR( r < m_NumResources[VarType] );
        auto* Resoruces = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
        return Resoruces[GetResourceOffset(VarType,r)];
    }
    const VkResource& GetResource(SHADER_VARIABLE_TYPE VarType, Uint32 r)const
    {
        VERIFY_EXPR( r < m_NumResources[VarType] );
        auto* Resources = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get());
        return Resources[GetResourceOffset(VarType,r)];
    }
    VkResource& GetResource(Uint32 r)
    {
        VERIFY_EXPR( r < GetTotalResourceCount() );
        auto* Resources = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
        return Resources[r];
    }

    Uint32 GetTotalResourceCount()const
    {
        static_assert(SHADER_VARIABLE_TYPE_NUM_TYPES == 3, "Did you add new variable type?");
        return m_NumResources[SHADER_VARIABLE_TYPE_STATIC] + m_NumResources[SHADER_VARIABLE_TYPE_MUTABLE] + m_NumResources[SHADER_VARIABLE_TYPE_DYNAMIC];
    }

    void AllocateMemory(IMemoryAllocator &Allocator);

#if USE_VARIABLE_HASH_MAP
    // Hash map to look up shader variables by name.
    // Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
    typedef std::pair<HashMapStringKey, IShaderVariable*> VariableHashElemType;
    std::unordered_map<HashMapStringKey, IShaderVariable*, std::hash<HashMapStringKey>, std::equal_to<HashMapStringKey>, STDAllocatorRawMem<VariableHashElemType> > m_VariableHash;
#endif

    std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> m_pLogicalDevice;

    IObject &m_Owner;
    // We must use shared_ptr to reference ShaderResources instance, because
    // there may be multiple objects referencing the same set of resources
    std::shared_ptr<const SPIRVShaderResources> m_pResources;

};

}
