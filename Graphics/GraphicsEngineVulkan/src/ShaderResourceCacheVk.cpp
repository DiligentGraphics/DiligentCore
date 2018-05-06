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

#include "pch.h"

#include "ShaderResourceCacheVk.h"

namespace Diligent
{
    void ShaderResourceCacheVk::Initialize(IMemoryAllocator &MemAllocator, Uint32 NumSets, Uint32 SetSizes[])
    {
        // Memory layout:
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

        VERIFY(m_pAllocator == nullptr && m_pMemory == nullptr, "Cache already initialized");
        m_pAllocator = &MemAllocator;
        m_NumSets = NumSets;
        Uint32 TotalResources = 0;
        for(Uint32 t=0; t < NumSets; ++t)
            TotalResources += SetSizes[t];
        auto MemorySize = NumSets * sizeof(DescriptorSet) + TotalResources * sizeof(Resource);
        if(MemorySize > 0)
        {
            m_pMemory = ALLOCATE( *m_pAllocator, "Memory for shader resource cache data", MemorySize);
            auto *pSets = reinterpret_cast<DescriptorSet*>(m_pMemory);
            auto *pCurrResPtr = reinterpret_cast<Resource*>(pSets + m_NumSets);
            for(Uint32 res=0; res < TotalResources; ++res)
                new(pCurrResPtr + res) Resource();

            for (Uint32 t = 0; t < NumSets; ++t)
            {
                new(&GetDescriptorSet(t)) DescriptorSet(SetSizes[t], SetSizes[t] > 0 ? pCurrResPtr : nullptr);
                pCurrResPtr += SetSizes[t];
            }
            VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory + MemorySize);
        }
    }

    ShaderResourceCacheVk::~ShaderResourceCacheVk()
    {
        if (m_pMemory)
        {
            Uint32 TotalResources = 0;
            for (Uint32 t = 0; t < m_NumSets; ++t)
                TotalResources += GetDescriptorSet(t).GetSize();
            auto *pResources = reinterpret_cast<Resource*>( reinterpret_cast<DescriptorSet*>(m_pMemory) + m_NumSets);
            for(Uint32 res=0; res < TotalResources; ++res)
                pResources[res].~Resource();
            for (Uint32 t = 0; t < m_NumSets; ++t)
                GetDescriptorSet(t).~DescriptorSet();

            m_pAllocator->Free(m_pMemory);
        }
    }
}
