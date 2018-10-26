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
//     ||   VkResource[0]  ...  VkResource[s-1]   |   VkResource[s]  ...  VkResource[s+m-1]   |   VkResource[s+m]  ...  VkResource[s+m+d-1]   ||
//     ||                                         |                                           |                                               ||
//     ||        SHADER_VARIABLE_TYPE_STATIC      |          SHADER_VARIABLE_TYPE_MUTABLE     |            SHADER_VARIABLE_TYPE_DYNAMIC       ||
//     ||                                         |                                           |                                               ||
//
//      s == m_NumResources[SHADER_VARIABLE_TYPE_STATIC]
//      m == m_NumResources[SHADER_VARIABLE_TYPE_MUTABLE]
//      d == m_NumResources[SHADER_VARIABLE_TYPE_DYNAMIC]
//
//
//
//   * Every VkResource structure holds a reference to SPIRVShaderResourceAttribs structure from SPIRVShaderResources.
//   * ShaderResourceLayoutVk keeps a shared pointer to SPIRVShaderResources instance.
//   * Every ShaderVariableVkImpl variable managed by ShaderVariableManagerVk keeps a reference to corresponding VkResource.
//
//
//    ______________________                  ________________________________________________________________________
//   |                      |  unique_ptr    |        |         |          |          |       |         |             |
//   | SPIRVShaderResources |--------------->|   UBs  |   SBs   | StrgImgs | SmplImgs |  ACs  | SepImgs | SepSamplers |
//   |______________________|                |________|_________|__________|__________|_______|_________|_____________|
//            A                                           A                     A
//            |                                           |                     |
//            |shared_ptr                                Ref                   Ref
//    ________|__________________                  ________\____________________|_____________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                     |
//   | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//   |___________________________|                |___________________|_________________|_______________|_____________________|
//                                                                       A                       A
//                                                                      /                        |
//                                                                    Ref                       Ref
//                                                                    /                          |
//    __________________________                   __________________/___________________________|___________________________
//   |                          |                 |                           |                            |                 |
//   |  ShaderVariableManagerVk |---------------->|  ShaderVariableVkImpl[0]  |   ShaderVariableVkImpl[1]  |     ...         |
//   |__________________________|                 |___________________________|____________________________|_________________|
//
//
//
//
//   Resources in the resource cache are identified by the descriptor set index and the offset from the set start
//
//    ___________________________                  ___________________________________________________________________________
//   |                           |   unique_ptr   |                   |                 |               |                     |
//   | ShaderResourceLayoutVk    |--------------->|   VkResource[0]   |  VkResource[1]  |       ...     | VkResource[s+m+d-1] |
//   |___________________________|                |___________________|_________________|_______________|_____________________|
//                                                       |                                                            |
//                                                       |                                                            |
//                                                       | (DescriptorSet, CacheOffset)                              / (DescriptorSet, CacheOffset)
//                                                        \                                                         /
//    __________________________                   ________V_______________________________________________________V_______
//   |                          |                 |                                                                        |
//   |   ShaderResourceCacheVk  |---------------->|                                   Resources                            |
//   |__________________________|                 |________________________________________________________________________|
//
//
//
//    ShaderResourceLayoutVk is used as follows:
//    * Every shader object (ShaderVkImpl) contains shader resource layout that facilitates management of static shader resources
//      ** The resource layout defines artificial layout where resource binding matches the 
//         resource type (SPIRVShaderResourceAttribs::ResourceType) 
//    * Every pipeline state object (PipelineStateVkImpl) maintains shader resource layout for every active shader stage
//      ** All variable types are preserved
//      ** Bindings, descriptor sets and offsets are assigned during the initialization

#include <array>
#include <memory>

#include "ShaderBase.h"
#include "HashUtils.h"
#include "ShaderResourceCacheVk.h"
#include "SPIRVShaderResources.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"

namespace Diligent
{

/// Diligent::ShaderResourceLayoutVk class
// sizeof(ShaderResourceLayoutVk)==56 (MS compiler, x64)
class ShaderResourceLayoutVk
{
public:
    ShaderResourceLayoutVk(IObject&                                    Owner, 
                           const VulkanUtilities::VulkanLogicalDevice& LogicalDevice);

    ShaderResourceLayoutVk              (const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk              (ShaderResourceLayoutVk&&)      = delete;
    ShaderResourceLayoutVk& operator =  (const ShaderResourceLayoutVk&) = delete;
    ShaderResourceLayoutVk& operator =  (ShaderResourceLayoutVk&&)      = delete;
    
    ~ShaderResourceLayoutVk();

    // This method is called by ShaderVkImpl class instance to initialize static 
    // shader resource layout and cache
    void InitializeStaticResourceLayout(std::shared_ptr<const SPIRVShaderResources> pSrcResources, 
                                        IMemoryAllocator&                           LayoutDataAllocator,
                                        ShaderResourceCacheVk&                      StaticResourceCache);

    // This method is called by PipelineStateVkImpl class instance to initialize resource
    // layouts for all shader stages in a pipeline.
    static void Initialize(Uint32 NumShaders,
                           ShaderResourceLayoutVk                       Layouts[],
                           std::shared_ptr<const SPIRVShaderResources>  pShaderResources[],
                           IMemoryAllocator&                            LayoutDataAllocator,
                           std::vector<uint32_t>                        SPIRVs[],
                           class PipelineLayout&                        PipelineLayout);

    // sizeof(VkResource) == 24 (x64)
    struct VkResource
    {
        VkResource(const VkResource&)              = delete;
        VkResource(VkResource&&)                   = delete;
        VkResource& operator = (const VkResource&) = delete;
        VkResource& operator = (VkResource&&)      = delete;

        static constexpr const Uint32 CacheOffsetBits    = 24;
        static constexpr const Uint32 SamplerIndBits     = 8;
        static constexpr const Uint32 InvalidSamplerInd  = (1 << SamplerIndBits)-1;

        const Uint16 Binding;
        const Uint16 DescriptorSet;
        const Uint32 CacheOffset   : CacheOffsetBits; // Offset from the beginning of the cached descriptor set
        const Uint32 SamplerInd    : SamplerIndBits;  // When using combined texture samplers, index of the separate sampler 
                                                      // assigned to separate image
        const SPIRVShaderResourceAttribs&   SpirvAttribs;
        const ShaderResourceLayoutVk&       ParentResLayout;

        VkResource(const ShaderResourceLayoutVk&        _ParentLayout,
                   const SPIRVShaderResourceAttribs&    _SpirvAttribs,
                   uint32_t                             _Binding,
                   uint32_t                             _DescriptorSet,
                   Uint32                               _CacheOffset,
                   Uint32                               _SamplerInd)noexcept :
            Binding         (static_cast<decltype(Binding)>(_Binding)),
            DescriptorSet   (static_cast<decltype(DescriptorSet)>(_DescriptorSet)),
            CacheOffset     (_CacheOffset),
            SamplerInd      (_SamplerInd),
            SpirvAttribs    (_SpirvAttribs),
            ParentResLayout (_ParentLayout)
        {
            VERIFY(_CacheOffset   < (1 << CacheOffsetBits),                               "Cache offset (", _CacheOffset, ") exceeds max representable value ", (1 << CacheOffsetBits) );
            VERIFY(_SamplerInd    < (1 << SamplerIndBits),                                "Sampler index  (", _SamplerInd, ") exceeds max representable value ", (1 << SamplerIndBits) );
            VERIFY(_Binding       <= std::numeric_limits<decltype(Binding)>::max(),       "Binding (", _Binding, ") exceeds max representable value ", std::numeric_limits<decltype(Binding)>::max() );
            VERIFY(_DescriptorSet <= std::numeric_limits<decltype(DescriptorSet)>::max(), "Descriptor set (", _DescriptorSet, ") exceeds max representable value ", std::numeric_limits<decltype(DescriptorSet)>::max());
        }

        // Checks if a resource is bound in ResourceCache at the given ArrayIndex
        bool IsBound(Uint32 ArrayIndex, const ShaderResourceCacheVk& ResourceCache)const;
        
        // Binds a resource pObject in the ResourceCache
        void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, ShaderResourceCacheVk& ResourceCache)const;

        // Updates resource descriptor in the descriptor set
        inline void UpdateDescriptorHandle(VkDescriptorSet                  vkDescrSet,
                                           uint32_t                         ArrayElement,
                                           const VkDescriptorImageInfo*     pImageInfo,
                                           const VkDescriptorBufferInfo*    pBufferInfo,
                                           const VkBufferView*              pTexelBufferView)const;

    private:
        void CacheUniformBuffer(IDeviceObject*                     pBuffer, 
                                ShaderResourceCacheVk::Resource&   DstRes, 
                                VkDescriptorSet                    vkDescrSet,
                                Uint32                             ArrayInd)const;

        void CacheStorageBuffer(IDeviceObject*                     pBufferView, 
                                ShaderResourceCacheVk::Resource&   DstRes, 
                                VkDescriptorSet                    vkDescrSet,
                                Uint32                             ArrayInd)const;

        void CacheTexelBuffer(IDeviceObject*                     pBufferView, 
                              ShaderResourceCacheVk::Resource&   DstRes, 
                              VkDescriptorSet                    vkDescrSet,
                              Uint32                             ArrayInd)const;
            
        template<typename TCacheSampler>
        void CacheImage(IDeviceObject*                   pTexView,
                        ShaderResourceCacheVk::Resource& DstRes,
                        VkDescriptorSet                  vkDescrSet,
                        Uint32                           ArrayInd,
                        TCacheSampler                    CacheSampler)const;

        void CacheSeparateSampler(IDeviceObject*                   pSampler,
                                  ShaderResourceCacheVk::Resource& DstRes,
                                  VkDescriptorSet                  vkDescrSet,
                                  Uint32                           ArrayInd)const;

        bool UpdateCachedResource(ShaderResourceCacheVk::Resource&   DstRes,
                                  Uint32                             ArrayInd,
                                  IDeviceObject*                     pObject, 
                                  INTERFACE_ID                       InterfaceId,
                                  const char*                        ResourceName)const;
    };

    // Copies static resources from SrcResourceCache defined by SrcLayout
    // to DstResourceCache defined by this layout
    void InitializeStaticResources(const ShaderResourceLayoutVk& SrcLayout, 
                                   ShaderResourceCacheVk&        SrcResourceCache,
                                   ShaderResourceCacheVk&        DstResourceCache)const;

#ifdef DEVELOPMENT
    void dvpVerifyBindings(const ShaderResourceCacheVk& ResourceCache)const;
#endif

    Uint32 GetResourceCount(SHADER_VARIABLE_TYPE VarType)const
    {
        return m_NumResources[VarType];
    }

    // Initializes resource slots in the ResourceCache
    void InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache)const;
    
    // Writes dynamic resource descriptors from ResourceCache to vkDynamicDescriptorSet
    void CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache,
                                VkDescriptorSet              vkDynamicDescriptorSet)const;

    const Char* GetShaderName()const;

    const VkResource& GetResource(SHADER_VARIABLE_TYPE VarType, Uint32 r)const
    {
        VERIFY_EXPR( r < m_NumResources[VarType] );
        auto* Resources = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get());
        return Resources[GetResourceOffset(VarType,r)];
    }

    bool IsUsingSeparateSamplers()const {return !m_pResources->IsUsingCombinedSamplers();}

private:
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
        auto* Resources = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
        return Resources[GetResourceOffset(VarType,r)];
    }

    const VkResource& GetResource(Uint32 r)const
    {
        VERIFY_EXPR(r < GetTotalResourceCount());
        auto* Resources = reinterpret_cast<const VkResource*>(m_ResourceBuffer.get());
        return Resources[r];
    }

    Uint32 GetTotalResourceCount()const
    {
        return m_NumResources[SHADER_VARIABLE_TYPE_NUM_TYPES];
    }

    void AllocateMemory(std::shared_ptr<const SPIRVShaderResources> pSrcResources, 
                        IMemoryAllocator&                           Allocator,
                        const SHADER_VARIABLE_TYPE*                 AllowedVarTypes,
                        Uint32                                      NumAllowedTypes);

    Uint32 FindAssignedSampler(const SPIRVShaderResourceAttribs& SepImg, Uint32 CurrResourceCount)const;


    IObject&                                            m_Owner;
    const VulkanUtilities::VulkanLogicalDevice&         m_LogicalDevice;
    std::unique_ptr<void, STDDeleterRawMem<void> >      m_ResourceBuffer;

    // We must use shared_ptr to reference ShaderResources instance, because
    // there may be multiple objects referencing the same set of resources
    std::shared_ptr<const SPIRVShaderResources>         m_pResources;

    std::array<Uint16, SHADER_VARIABLE_TYPE_NUM_TYPES+1>  m_NumResources = {};
};

}
