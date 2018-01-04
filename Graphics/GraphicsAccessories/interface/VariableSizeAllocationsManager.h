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

// Helper class that handles free memory block management to accommodate variable-size allocation requests
// See http://diligentgraphics.com/diligent-engine/architecture/d3d12/variable-size-memory-allocations-manager/

#pragma once

#include <map>
#include "MemoryAllocator.h"
#include "STDAllocator.h"
#include "DebugUtilities.h"

namespace Diligent
{
    // The class handles free memory block management to accommodate variable-size allocation requests. 
    // It keeps track of free blocks only and does not record allocation sizes. The class uses two ordered maps 
    // to facilitate operations. The first map keeps blocks sorted by their offsets. The second multimap keeps blocks 
    // sorted by their sizes. The elements of the two maps reference each other, which enables efficient block 
    // insertion, removal and merging.
    //
    //   8                 32                       64                           104
    //   |<---16--->|       |<-----24------>|        |<---16--->|                 |<-----32----->|
    //
    //
    //        m_FreeBlocksBySize      m_FreeBlocksByOffset                    
    //           size->offset            offset->size
    //                   
    //                16 ------------------>  8  ---------->  {size = 16, &m_FreeBlocksBySize[0]}
    //
    //                16 ------.   .-------> 32  ---------->  {size = 24, &m_FreeBlocksBySize[2]}
    //                          '.'
    //                24 -------' '--------> 64  ---------->  {size = 16, &m_FreeBlocksBySize[1]}
    //      
    //                32 ------------------> 104 ---------->  {size = 32, &m_FreeBlocksBySize[3]}
    //
    class VariableSizeAllocationsManager
    {
    public:
        typedef size_t OffsetType;
        static constexpr OffsetType InvalidOffset = static_cast<OffsetType>(-1);

    private:
        struct FreeBlockInfo;

        // Type of the map that keeps memory blocks sorted by their offsets
        using TFreeBlocksByOffsetMap = 
            std::map<OffsetType,    
                     FreeBlockInfo, 
                     std::less<OffsetType>, // Standard ordering
                     STDAllocatorRawMem<std::pair<const OffsetType,  FreeBlockInfo>> // Raw memory allocator
                     >;

        // Type of the map that keeps memory blocks sorted by their sizes
        using TFreeBlocksBySizeMap = 
            std::multimap<OffsetType, 
                          TFreeBlocksByOffsetMap::iterator, 
                          std::less<OffsetType>, // Standard ordering
                          STDAllocatorRawMem<std::pair<const OffsetType, TFreeBlocksByOffsetMap::iterator>> // Raw memory allocator
                          >;

        struct FreeBlockInfo
        {
            // Block size (no reserved space for the size of the allocation)
            OffsetType Size;

            // Iterator referencing this block in the multimap sorted by the block size
            TFreeBlocksBySizeMap::iterator OrderBySizeIt;

            FreeBlockInfo(OffsetType _Size) : Size(_Size){}
        };

    public:
        VariableSizeAllocationsManager(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            m_MaxSize(MaxSize),
            m_FreeSize(MaxSize),
            m_FreeBlocksByOffset( STD_ALLOCATOR_RAW_MEM(TFreeBlocksByOffsetMap::value_type, Allocator, "Allocator for map<OffsetType, FreeBlockInfo>") ),
            m_FreeBlocksBySize( STD_ALLOCATOR_RAW_MEM(TFreeBlocksBySizeMap::value_type, Allocator, "Allocator for multimap<OffsetType, TFreeBlocksByOffsetMap::iterator>") )
        {
            // Insert single maximum-size block
            AddNewBlock(0, m_MaxSize);

#ifdef _DEBUG
            DbgVerifyList();
#endif
        }

        ~VariableSizeAllocationsManager()
        {
#ifdef _DEBUG
            if( !m_FreeBlocksByOffset.empty() || !m_FreeBlocksBySize.empty() )
            {
                VERIFY(m_FreeBlocksByOffset.size() == 1, "Single free block is expected");
                VERIFY(m_FreeBlocksByOffset.begin()->first == 0, "Head chunk offset is expected to be 0");
                VERIFY(m_FreeBlocksByOffset.begin()->second.Size == m_MaxSize, "Head chunk size is expected to be ", m_MaxSize);
                VERIFY_EXPR(m_FreeBlocksByOffset.begin()->second.OrderBySizeIt == m_FreeBlocksBySize.begin());
                VERIFY(m_FreeBlocksBySize.size() == m_FreeBlocksByOffset.size(), "Sizes of the two maps must be equal");

                VERIFY(m_FreeBlocksBySize.size() == 1, "Single free block is expected");
                VERIFY(m_FreeBlocksBySize.begin()->first == m_MaxSize, "Head chunk size is expected to be ", m_MaxSize);
                VERIFY(m_FreeBlocksBySize.begin()->second == m_FreeBlocksByOffset.begin(), "Incorrect first block");
            }
#endif
        }

        VariableSizeAllocationsManager(VariableSizeAllocationsManager&& rhs) : 
            m_FreeBlocksByOffset(std::move(rhs.m_FreeBlocksByOffset)),
            m_FreeBlocksBySize(std::move(rhs.m_FreeBlocksBySize)),
            m_MaxSize(rhs.m_MaxSize),
            m_FreeSize(rhs.m_FreeSize)
        {
            //rhs.m_MaxSize = 0; - const
            rhs.m_FreeSize = 0;
        }

        VariableSizeAllocationsManager& operator = (VariableSizeAllocationsManager&& rhs) = default;
        VariableSizeAllocationsManager(const VariableSizeAllocationsManager&) = delete;
        VariableSizeAllocationsManager& operator = (const VariableSizeAllocationsManager&) = delete;

        OffsetType Allocate(OffsetType Size)
        {
            VERIFY_EXPR(Size != 0);
            if(m_FreeSize < Size)
                return InvalidOffset;

            // Get the first block that is large enough to encompass Size bytes
            // lower_bound() returns an iterator pointing to the first element that 
            // is not less (i.e. >= ) than key
            auto SmallestBlockItIt = m_FreeBlocksBySize.lower_bound(Size);
            if(SmallestBlockItIt == m_FreeBlocksBySize.end())
                return InvalidOffset;

            auto SmallestBlockIt = SmallestBlockItIt->second;
            VERIFY_EXPR(Size <= SmallestBlockIt->second.Size);
            VERIFY_EXPR(SmallestBlockIt->second.Size == SmallestBlockItIt->first);
            
            //     SmallestBlockIt.Offset      
            //        |                                  |
            //        |<------SmallestBlockIt.Size------>|
            //        |<------Size------>|<---NewSize--->|
            //        |                  |
            //      Offset              NewOffset
            //
            auto Offset = SmallestBlockIt->first;
            auto NewOffset = Offset + Size;
            auto NewSize = SmallestBlockIt->second.Size - Size;
            VERIFY_EXPR(SmallestBlockItIt == SmallestBlockIt->second.OrderBySizeIt);
            m_FreeBlocksBySize.erase(SmallestBlockItIt);
            m_FreeBlocksByOffset.erase(SmallestBlockIt);
            if (NewSize > 0)
            {
                AddNewBlock(NewOffset, NewSize);
            }

            m_FreeSize -= Size;

#ifdef _DEBUG
            DbgVerifyList();
#endif
            return Offset;
        }

        void Free(OffsetType Offset, OffsetType Size)
        {
            VERIFY_EXPR(Offset+Size <= m_MaxSize);

            // Find the first element whose offset is greater than the specified offset.
            // upper_bound() returns an iterator pointing to the first element in the 
            // container whose key is considered to go after k.
            auto NextBlockIt = m_FreeBlocksByOffset.upper_bound(Offset); 
#ifdef _DEBUG
            {
                auto LowBnd = m_FreeBlocksByOffset.lower_bound(Offset); // First element whose offset is  >=
                // Since zero-size allocations are not allowed, lower bound must always be equal to the upper bound
                VERIFY_EXPR(LowBnd == NextBlockIt);
            }
#endif
            // Block being deallocated must not overlap with the next block
            VERIFY_EXPR(NextBlockIt == m_FreeBlocksByOffset.end() || Offset+Size <= NextBlockIt->first);
            auto PrevBlockIt = NextBlockIt;
            if(PrevBlockIt != m_FreeBlocksByOffset.begin())
            {
                --PrevBlockIt;
                // Block being deallocated must not overlap with the previous block
                VERIFY_EXPR(Offset >= PrevBlockIt->first + PrevBlockIt->second.Size);
            }
            else
                PrevBlockIt = m_FreeBlocksByOffset.end();

            OffsetType NewSize, NewOffset;
            if(PrevBlockIt != m_FreeBlocksByOffset.end() && Offset == PrevBlockIt->first + PrevBlockIt->second.Size)
            {
                //  PrevBlock.Offset             Offset
                //       |                          |
                //       |<-----PrevBlock.Size----->|<------Size-------->|
                //
                NewSize = PrevBlockIt->second.Size + Size;
                NewOffset = PrevBlockIt->first;

                if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
                {
                    //   PrevBlock.Offset           Offset            NextBlock.Offset      
                    //     |                          |                    |
                    //     |<-----PrevBlock.Size----->|<------Size-------->|<-----NextBlock.Size----->|
                    //
                    NewSize += NextBlockIt->second.Size;
                    m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
                    m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
                    // Delete the range of two blocks
                    ++NextBlockIt;
                    m_FreeBlocksByOffset.erase(PrevBlockIt, NextBlockIt);
                }
                else
                {
                    //   PrevBlock.Offset           Offset                     NextBlock.Offset      
                    //     |                          |                             |
                    //     |<-----PrevBlock.Size----->|<------Size-------->| ~ ~ ~  |<-----NextBlock.Size----->|
                    //
                    m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
                    m_FreeBlocksByOffset.erase(PrevBlockIt);
                }
            }
            else if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
            {
                //   PrevBlock.Offset                   Offset            NextBlock.Offset      
                //     |                                  |                    |
                //     |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->|<-----NextBlock.Size----->|
                //
                NewSize = Size + NextBlockIt->second.Size;
                NewOffset = Offset;
                m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
                m_FreeBlocksByOffset.erase(NextBlockIt);
            }
            else
            {
                //   PrevBlock.Offset                   Offset                     NextBlock.Offset      
                //     |                                  |                            |
                //     |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
                //
                NewSize = Size;
                NewOffset = Offset;
            }

            AddNewBlock(NewOffset, NewSize);

            m_FreeSize += Size;
#ifdef _DEBUG
            DbgVerifyList();
#endif
        }

        OffsetType GetMaxSize()const{return m_MaxSize;}
        bool IsFull()const{ return m_FreeSize==0; };
        bool IsEmpty()const{ return m_FreeSize==m_MaxSize; };
        OffsetType GetFreeSize()const{return m_FreeSize;}

#ifdef _DEBUG
        size_t DbgGetNumFreeBlocks()const{return m_FreeBlocksByOffset.size();}
#endif

    private:
        void AddNewBlock(OffsetType Offset, OffsetType Size)
        {
            auto NewBlockIt = m_FreeBlocksByOffset.emplace(Offset, Size);
            VERIFY_EXPR(NewBlockIt.second);
            auto OrderIt = m_FreeBlocksBySize.emplace(Size, NewBlockIt.first);
            NewBlockIt.first->second.OrderBySizeIt = OrderIt;
        }


#ifdef _DEBUG
        void DbgVerifyList()
        {
            OffsetType TotalFreeSize = 0;

            auto BlockIt = m_FreeBlocksByOffset.begin();
            auto PrevBlockIt = m_FreeBlocksByOffset.end();
            VERIFY_EXPR(m_FreeBlocksByOffset.size() == m_FreeBlocksBySize.size());
            while (BlockIt != m_FreeBlocksByOffset.end())
            {
                VERIFY_EXPR(BlockIt->first >= 0 && BlockIt->first + BlockIt->second.Size <= m_MaxSize);
                VERIFY_EXPR(BlockIt == BlockIt->second.OrderBySizeIt->second);
                VERIFY_EXPR(BlockIt->second.Size == BlockIt->second.OrderBySizeIt->first);
                //   PrevBlock.Offset                   BlockIt.first                     
                //     |                                  |                            
                // ~ ~ |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->| ~ ~ ~
                //
                VERIFY(PrevBlockIt == m_FreeBlocksByOffset.end() || BlockIt->first > PrevBlockIt->first + PrevBlockIt->second.Size, "Unmerged adjacent or overlapping blocks detected" );
                TotalFreeSize += BlockIt->second.Size;

                PrevBlockIt = BlockIt;
                ++BlockIt;
            }

            auto OrderIt = m_FreeBlocksBySize.begin();
            while (OrderIt != m_FreeBlocksBySize.end())
            {
                VERIFY_EXPR(OrderIt->first == OrderIt->second->second.Size);
                ++OrderIt;
            }

            VERIFY_EXPR(TotalFreeSize == m_FreeSize);
        }
#endif

        TFreeBlocksByOffsetMap m_FreeBlocksByOffset;
        TFreeBlocksBySizeMap   m_FreeBlocksBySize;
        
        const OffsetType m_MaxSize = 0;
              OffsetType m_FreeSize = 0;
    };
}
