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

#include "DescriptorHeap.h"

namespace Diligent
{
#if 0
enum class CachedResourceType : Int32
{
    Unknown = -1,
    CBV = 0,
    TexSRV,
    BufSRV,
    TexUAV,
    BufUAV,
    Sampler,
    NumTypes
};
#endif
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
#if 0
    static constexpr Uint32 InvalidDescriptorOffset = static_cast<Uint32>(-1);
#endif
    struct Resource
    {
#if 0
        CachedResourceType Type = CachedResourceType::Unknown;
        // CPU descriptor handle of a cached resource in CPU-only descriptor heap
        // Note that for dynamic resources, this is the only available CPU descriptor handle
        Vk_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle = {0};
#endif
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
            UNSUPPORTED("Not yet implemented");
            return VK_NULL_HANDLE;
        }

        const Uint32 m_NumResources = 0;
    private:
        
        Resource* const m_pResources = nullptr;
    };

    inline DescriptorSet& GetDescriptorSet(Uint32 Index)
    {
        VERIFY_EXPR(Index < m_NumSets);
        return reinterpret_cast<DescriptorSet*>(m_pMemory)[Index];
    }

    inline Uint32 GetNumDescriptorSets()const{return m_NumSets; }
#if 0
    void SetDescriptorHeapSpace(DescriptorHeapAllocation &&CbcSrvUavHeapSpace, DescriptorHeapAllocation &&SamplerHeapSpace)
    {
        VERIFY(m_SamplerHeapSpace.GetCpuHandle().ptr == 0 && m_CbvSrvUavHeapSpace.GetCpuHandle().ptr == 0, "Space has already been allocated in GPU descriptor heaps");
#ifdef _DEBUG
        Uint32 NumSamplerDescriptors = 0, NumSrvCbvUavDescriptors = 0;
        for (Uint32 rt = 0; rt < m_NumTables; ++rt)
        {
            auto &Tbl = GetDescriptorSet(rt);
            if(Tbl.m_TableStartOffset != InvalidDescriptorOffset)
            {
                if(Tbl.DbgGetHeapType() == Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
                {
                    VERIFY(Tbl.m_TableStartOffset == NumSrvCbvUavDescriptors, "Descriptor space allocation is not continuous");
                    NumSrvCbvUavDescriptors = std::max(NumSrvCbvUavDescriptors, Tbl.m_TableStartOffset + Tbl.GetSize());
                }
                else
                {
                    VERIFY(Tbl.m_TableStartOffset == NumSamplerDescriptors, "Descriptor space allocation is not continuous");
                    NumSamplerDescriptors = std::max(NumSamplerDescriptors, Tbl.m_TableStartOffset + Tbl.GetSize());
                }
            }
        }
        VERIFY(NumSrvCbvUavDescriptors == CbcSrvUavHeapSpace.GetNumHandles() || NumSrvCbvUavDescriptors == 0 && CbcSrvUavHeapSpace.GetCpuHandle(0).ptr == 0, "Unexpected descriptor heap allocation size" );
        VERIFY(NumSamplerDescriptors == SamplerHeapSpace.GetNumHandles() || NumSamplerDescriptors == 0 && SamplerHeapSpace.GetCpuHandle(0).ptr == 0, "Unexpected descriptor heap allocation size" );
#endif

        m_CbvSrvUavHeapSpace = std::move(CbcSrvUavHeapSpace);
        m_SamplerHeapSpace = std::move(SamplerHeapSpace);
    }

    IVkDescriptorHeap* GetSrvCbvUavDescriptorHeap(){return m_CbvSrvUavHeapSpace.GetDescriptorHeap();}
    IVkDescriptorHeap* GetSamplerDescriptorHeap()  {return m_SamplerHeapSpace.GetDescriptorHeap();}

    // Returns CPU descriptor handle of a shader visible descriptor heap allocation
    template<Vk_DESCRIPTOR_HEAP_TYPE HeapType>
    Vk_CPU_DESCRIPTOR_HANDLE GetShaderVisibleTableCPUDescriptorHandle(Uint32 RootParamInd, Uint32 OffsetFromTableStart = 0)
    {
        auto &RootParam = GetDescriptorSet(RootParamInd);
        VERIFY(HeapType == RootParam.DbgGetHeapType(), "Invalid descriptor heap type");

        Vk_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle = {0};
        // Descriptor heap allocation is not assigned for dynamic resources or 
        // in a special case when resource cache is used to store static 
        // variable assignments for a shader. It is also not assigned to root views
        if( RootParam.m_TableStartOffset != InvalidDescriptorOffset )
        {
            VERIFY(OffsetFromTableStart < RootParam.m_NumResources, "Offset is out of range");
            if( HeapType == Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER  )
            {
                VERIFY_EXPR(!m_SamplerHeapSpace.IsNull());
                CPUDescriptorHandle = m_SamplerHeapSpace.GetCpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
            }
            else if( HeapType == Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
            {
                VERIFY_EXPR(!m_CbvSrvUavHeapSpace.IsNull());
                CPUDescriptorHandle = m_CbvSrvUavHeapSpace.GetCpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
            }
            else
            {
                UNEXPECTED("Unexpected descriptor heap type");
            }
        }

        return CPUDescriptorHandle;
    }

    // Returns GPU descriptor handle of a shader visible descriptor table
    template<Vk_DESCRIPTOR_HEAP_TYPE HeapType>
    Vk_GPU_DESCRIPTOR_HANDLE GetShaderVisibleTableGPUDescriptorHandle(Uint32 RootParamInd, Uint32 OffsetFromTableStart = 0)
    {
        auto &RootParam = GetDescriptorSet(RootParamInd);
        VERIFY(RootParam.m_TableStartOffset != InvalidDescriptorOffset, "GPU descriptor handle must never be requested for dynamic resources");
        VERIFY(OffsetFromTableStart < RootParam.m_NumResources, "Offset is out of range");

        Vk_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle = {0};
        VERIFY( HeapType == RootParam.DbgGetHeapType(), "Invalid descriptor heap type");
        if( HeapType == Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER )
        {
            VERIFY_EXPR(!m_SamplerHeapSpace.IsNull());
            GPUDescriptorHandle = m_SamplerHeapSpace.GetGpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
        }
        else if( HeapType == Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
        {
            VERIFY_EXPR(!m_CbvSrvUavHeapSpace.IsNull());
            GPUDescriptorHandle = m_CbvSrvUavHeapSpace.GetGpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
        }
        else
        {
            UNEXPECTED("Unexpected descriptor heap type");
        }

        return GPUDescriptorHandle;
    }
#endif
#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    DbgCacheContentType DbgGetContentType()const{return m_DbgContentType;}
#endif

private:
    ShaderResourceCacheVk(const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk(ShaderResourceCacheVk&&) = delete;
    ShaderResourceCacheVk& operator = (const ShaderResourceCacheVk&) = delete;
    ShaderResourceCacheVk& operator = (ShaderResourceCacheVk&&) = delete;

#if 0
    // Allocation in a GPU-visible sampler descriptor heap
    DescriptorHeapAllocation m_SamplerHeapSpace;
    
    // Allocation in a GPU-visible CBV/SRV/UAV descriptor heap
    DescriptorHeapAllocation m_CbvSrvUavHeapSpace;
#endif
    
    IMemoryAllocator *m_pAllocator=nullptr; 
    void *m_pMemory = nullptr;
    Uint32 m_NumSets = 0;

#ifdef _DEBUG
    // Only for debug purposes: indicates what types of resources are stored in the cache
    const DbgCacheContentType m_DbgContentType;
#endif
};

}
