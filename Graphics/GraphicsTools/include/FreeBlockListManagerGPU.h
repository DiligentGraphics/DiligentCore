/*     Copyright 2015-2016 Egor Yusov
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
#include "FreeBlockListManager.h"
#include "STDAllocator.h"

namespace Diligent
{
    class FreeBlockListManagerGPU : public FreeBlockListManager
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
        FreeBlockListManagerGPU(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            FreeBlockListManager(MaxSize, Allocator),
            m_StaleAllocations(0, FreedAllocationInfo(0,0,0), STD_ALLOCATOR_RAW_MEM(FreedAllocationInfo, Allocator, "Allocator for deque< FreedAllocationInfo>" ))
        {}

        ~FreeBlockListManagerGPU()
        {
            VERIFY(m_StaleAllocations.empty(), "Not all stale allocations released");
        }

        // = default causes compiler error when instantiating std::vector::emplace_back() in Visual Studio 2015 (Version 14.0.23107.0 D14REL)
        FreeBlockListManagerGPU(FreeBlockListManagerGPU&& rhs) : 
            FreeBlockListManager(std::move(rhs)),
            m_StaleAllocations(std::move(rhs.m_StaleAllocations))
        {
        }

        FreeBlockListManagerGPU& operator = (FreeBlockListManagerGPU&& rhs) = default;
        FreeBlockListManagerGPU(const FreeBlockListManagerGPU&) = delete;
        FreeBlockListManagerGPU& operator = (const FreeBlockListManagerGPU&) = delete;

        void Free(OffsetType Offset, OffsetType Size, Uint64 FrameNumber)
        {
            m_StaleAllocations.emplace_back(Offset, Size, FrameNumber);
        }

        void ReleaseCompletedFrames(Uint64 NumCompletedFrames)
        {
            while(!m_StaleAllocations.empty() && m_StaleAllocations.front().FrameNumber < NumCompletedFrames)
            {
                auto &OldestAllocation = m_StaleAllocations.front();
                FreeBlockListManager::Free(OldestAllocation.Offset, OldestAllocation.Size);
                m_StaleAllocations.pop_front();
            }
        }

    private:
        std::deque< FreedAllocationInfo, STDAllocatorRawMem<FreedAllocationInfo> > m_StaleAllocations;
    };
}
