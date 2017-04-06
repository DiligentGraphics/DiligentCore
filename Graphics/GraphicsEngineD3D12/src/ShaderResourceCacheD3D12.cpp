/*     Copyright 2015-2017 Egor Yusov
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

#include "ShaderResourceCacheD3D12.h"

namespace Diligent
{
    void ShaderResourceCacheD3D12::Initialize(IMemoryAllocator &MemAllocator, Uint32 NumTables, Uint32 TableSizes[])
    {
        // Memory layout:
        //                                         __________________________________________________________
        //                                        |             m_pResources, m_NumResources                 |
        //  m_pMemory                             |                                                          |
        //  |                                     |                                                          V
        //  |  RootTable[0]  |   ....    |  RootTable[Nrt-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
        //       |                                                A  
        //       |                                                |   
        //       |________________________________________________|                 
        //                    m_pResources, m_NumResources                            
        //                                                             

        VERIFY(m_pAllocator == nullptr && m_pMemory == nullptr, "Cache already initialized")
        m_pAllocator = &MemAllocator;
        m_NumTables = NumTables;
        Uint32 TotalResources = 0;
        for(Uint32 t=0; t < NumTables; ++t)
            TotalResources += TableSizes[t];
        auto MemorySize = NumTables * sizeof(RootTable) + TotalResources * sizeof(Resource);
        if(MemorySize > 0)
        {
            m_pMemory = ALLOCATE( *m_pAllocator, "Memory for shader resource cache data", MemorySize);
            auto *pTables = reinterpret_cast<RootTable*>(m_pMemory);
            auto *pCurrResPtr = reinterpret_cast<Resource*>(pTables + m_NumTables);
            for(Uint32 res=0; res < TotalResources; ++res)
                new(pCurrResPtr + res) Resource();

            for (Uint32 t = 0; t < NumTables; ++t)
            {
                new(&GetRootTable(t)) RootTable(TableSizes[t], TableSizes[t] > 0 ? pCurrResPtr : nullptr);
                pCurrResPtr += TableSizes[t];
            }
            VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory + MemorySize);
        }
    }

    ShaderResourceCacheD3D12::~ShaderResourceCacheD3D12()
    {
        if (m_pMemory)
        {
            Uint32 TotalResources = 0;
            for (Uint32 t = 0; t < m_NumTables; ++t)
                TotalResources += GetRootTable(t).GetSize();
            auto *pResources = reinterpret_cast<Resource*>( reinterpret_cast<RootTable*>(m_pMemory) + m_NumTables);
            for(Uint32 res=0; res < TotalResources; ++res)
                pResources[res].~Resource();
            for (Uint32 t = 0; t < m_NumTables; ++t)
                GetRootTable(t).~RootTable();

            m_pAllocator->Free(m_pMemory);
        }
    }
}
