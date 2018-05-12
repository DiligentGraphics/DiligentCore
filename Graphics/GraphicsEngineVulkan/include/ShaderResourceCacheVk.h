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
/// Declaration of Diligent::ShaderResourceCacheVk class

// Shader resource cache stores Vk resources in a continuous chunk of memory:
//   
//
//                                              ______________________________________________________________
//  m_pMemory                                  |                 m_pResources, m_NumResources == m            |
//  |                                          |                                                              |
//  V                                          |                                                              V
//  |  DescriptorSet[0]  |   ....    |  DescriptorSet[Ns-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
//            |                                                  A \
//            |                                                  |  \
//            |__________________________________________________|   \RefCntAutoPtr
//                       m_pResources, m_NumResources == n            \_________     
//                                                                    |  Object |
//                                                                     --------- 
//                                                                    
//  Ns = m_NumSets
//
//
// For static and mutable variable types, the cache is also assigned decriptor set
//
//   
//  |  VkDescriptorSet  |
//          A 
//          | 
//          |
//   |    DescriptorSet[0]    |    DescriptorSet[1]    |
//
//
// Dynamic resources are not VkDescriptorSet 

#include "DescriptorPoolManager.h"

namespace Diligent
{

class ShaderResourceCacheVk
{
public:
    // This enum is used for debug purposes only
    enum DbgCacheContentType
    {
        StaticShaderResources,
        SRBResources
    };

    ShaderResourceCacheVk(DbgCacheContentType dbgContentType)
#ifdef _DEBUG
        : m_DbgContentType(dbgContentType)
#endif
    {
    }

    ~ShaderResourceCacheVk();

    void Initialize(IMemoryAllocator &MemAllocator, Uint32 NumSets, Uint32 SetSizes[]);

    struct Resource
    {
        RefCntAutoPtr<IDeviceObject> pObject;
    };

    class DescriptorSet
    {
    public:
        DescriptorSet(Uint32 NumResources, Resource *pResources) :
            m_NumResources(NumResources),
            m_pResources(pResources)
        {}

        inline Resource& GetResource(Uint32 OffsetFromTableStart)
        {
            VERIFY(OffsetFromTableStart < m_NumResources, "Root table at index is not large enough to store descriptor at offset ", OffsetFromTableStart );
            return m_pResources[OffsetFromTableStart];
        }

        inline Uint32 GetSize()const{return m_NumResources; }

        VkDescriptorSet GetVkDescriptorSet()const
        {
            return m_DescriptorSetAllocation.GetVkDescriptorSet();
        }

        void AssignDescriptorSetAllocation(DescriptorPoolAllocation&& Allocation)
        {
            VERIFY(m_NumResources > 0, "Descriptor set is empty");
            m_DescriptorSetAllocation = std::move(Allocation);
        }

        const Uint32 m_NumResources = 0;
    private:
        
        Resource* const m_pResources = nullptr;
        DescriptorPoolAllocation m_DescriptorSetAllocation;
    };

    inline DescriptorSet& GetDescriptorSet(Uint32 Index)
    {
        VERIFY_EXPR(Index < m_NumSets);
        return reinterpret_cast<DescriptorSet*>(m_pMemory)[Index];
    }

    inline Uint32 GetNumDescriptorSets()const{return m_NumSets; }

#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    DbgCacheContentType DbgGetContentType()const{return m_DbgContentType;}
#endif

private:
    ShaderResourceCacheVk(const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk(ShaderResourceCacheVk&&) = delete;
    ShaderResourceCacheVk& operator = (const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk& operator = (ShaderResourceCacheVk&&) = delete;
    
    IMemoryAllocator *m_pAllocator=nullptr; 
    void *m_pMemory = nullptr;
    Uint32 m_NumSets = 0;

#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    const DbgCacheContentType m_DbgContentType;
#endif
};

}
