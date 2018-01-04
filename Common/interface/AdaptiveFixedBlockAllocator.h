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

#include "MemoryAllocator.h"
#include "FixedBlockMemoryAllocator.h"
#include "DebugUtilities.h"

namespace Diligent
{
    // Adaptive allocator that can function as a raw memory allocator or as
    // fixed block memory allocator. In the fixed block memory allocator mode,
    // the block size is determined by the size of the first allocation
    class AdaptiveFixedBlockAllocator : public IMemoryAllocator
    {
    public:
        AdaptiveFixedBlockAllocator(IMemoryAllocator &RawMemAllocator, Uint32 NumBlocksPerAllocation) : 
            m_RawMemAllocator(RawMemAllocator),
            m_pFixedBlockAllocator(nullptr, STDDeleterRawMem<FixedBlockMemoryAllocator>(RawMemAllocator)),
            m_NumBlocksPerAllocation(NumBlocksPerAllocation)
        {
            // Initialize allocator when we get the fist allocation request and know allocation size
        }

        // Allocates block of memory
        virtual void* Allocate(size_t Size, const Char* dbgDescription, const char* dbgFileName, const  Int32 dbgLineNumber)override final
        {
            if (m_NumBlocksPerAllocation > 1)
            {
                if( !m_pFixedBlockAllocator )
                {
                    // Create fixed block allocator
                    auto *pRawMem = m_RawMemAllocator.Allocate(sizeof(FixedBlockMemoryAllocator), "Memory for FixedBlockMemoryAllocator", __FILE__, __LINE__);
                    m_pFixedBlockAllocator.reset( new(pRawMem) FixedBlockMemoryAllocator(m_RawMemAllocator, Size, m_NumBlocksPerAllocation) );
                }

                return m_pFixedBlockAllocator->Allocate(Size, dbgDescription, dbgFileName, dbgLineNumber);
            }
            else
            {
                // Use default raw allocator
                return m_RawMemAllocator.Allocate(Size, dbgDescription, dbgFileName, dbgLineNumber);
            }
        }

        // Releases memory
        virtual void Free(void *Ptr)override final
        {
            if (m_NumBlocksPerAllocation > 1)
            {
                VERIFY_EXPR(m_pFixedBlockAllocator);
                m_pFixedBlockAllocator->Free(Ptr);
            }
            else
            {
                VERIFY_EXPR(!m_pFixedBlockAllocator);
                m_RawMemAllocator.Free(Ptr);
            }
        }

    private:
        IMemoryAllocator &m_RawMemAllocator;
        Uint32 m_NumBlocksPerAllocation = 0;
        std::unique_ptr<FixedBlockMemoryAllocator, STDDeleterRawMem<FixedBlockMemoryAllocator> > m_pFixedBlockAllocator;
    };
}
