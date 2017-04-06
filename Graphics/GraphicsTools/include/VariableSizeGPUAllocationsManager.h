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

#pragma once

#include <deque>
#include "VariableSizeAllocationsManager.h"
#include "STDAllocator.h"

namespace Diligent
{
    // Class extends basic variable-size memory block allocator by deferring deallocation
    // of freed blocks untill the corresponding frame is completed
    class VariableSizeGPUAllocationsManager : public VariableSizeAllocationsManager
    {
    private:
        struct FreedAllocationInfo
        {
            OffsetType Offset;
            OffsetType Size;
            Uint64 FrameNumber;
            FreedAllocationInfo(OffsetType _Offset, OffsetType _Size, Uint64 _FrameNumber) : 
                Offset(_Offset), Size(_Size), FrameNumber(_FrameNumber)
            {}
        };

    public:
        VariableSizeGPUAllocationsManager(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            VariableSizeAllocationsManager(MaxSize, Allocator),
            m_StaleAllocations(0, FreedAllocationInfo(0,0,0), STD_ALLOCATOR_RAW_MEM(FreedAllocationInfo, Allocator, "Allocator for deque< FreedAllocationInfo>" ))
        {}

        ~VariableSizeGPUAllocationsManager()
        {
            VERIFY(m_StaleAllocations.empty(), "Not all stale allocations released");
        }

        // = default causes compiler error when instantiating std::vector::emplace_back() in Visual Studio 2015 (Version 14.0.23107.0 D14REL)
        VariableSizeGPUAllocationsManager(VariableSizeGPUAllocationsManager&& rhs) : 
            VariableSizeAllocationsManager(std::move(rhs)),
            m_StaleAllocations(std::move(rhs.m_StaleAllocations))
        {
        }

        VariableSizeGPUAllocationsManager& operator = (VariableSizeGPUAllocationsManager&& rhs) = default;
        VariableSizeGPUAllocationsManager(const VariableSizeGPUAllocationsManager&) = delete;
        VariableSizeGPUAllocationsManager& operator = (const VariableSizeGPUAllocationsManager&) = delete;

        void Free(OffsetType Offset, OffsetType Size, Uint64 FrameNumber)
        {
            // Do not release the block immediately, but add
            // it to the queue instead
            m_StaleAllocations.emplace_back(Offset, Size, FrameNumber);
        }

        void ReleaseCompletedFrames(Uint64 NumCompletedFrames)
        {
            // Free all allocations from the beginning of the queue that belong to completed frames
            while(!m_StaleAllocations.empty() && m_StaleAllocations.front().FrameNumber < NumCompletedFrames)
            {
                auto &OldestAllocation = m_StaleAllocations.front();
                VariableSizeAllocationsManager::Free(OldestAllocation.Offset, OldestAllocation.Size);
                m_StaleAllocations.pop_front();
            }
        }

    private:
        std::deque< FreedAllocationInfo, STDAllocatorRawMem<FreedAllocationInfo> > m_StaleAllocations;
    };
}
