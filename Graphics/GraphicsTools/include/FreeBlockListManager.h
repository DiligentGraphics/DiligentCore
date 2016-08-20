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

#include <map>
#include "MemoryAllocator.h"
#include "FixedBlockMemoryAllocator.h"
#include "STDAllocator.h"
#include "DebugUtilities.h"

namespace Diligent
{
    class FreeBlockListManager
    {
    public:
        typedef size_t OffsetType;
        typedef std::pair<Uint64,OffsetType> FrameNumOffsetPair;

        struct FreeBlockInfo;
        typedef std::map<OffsetType,  
                         FreeBlockInfo, 
                         std::less<OffsetType>, // Standard ordering
                         STDAllocatorRawMem<std::pair<OffsetType,  FreeBlockInfo>> // Raw memory allocator
                        > TFreeBlocksByOffsetMap;
        typedef std::multimap<OffsetType, 
                              TFreeBlocksByOffsetMap::iterator, 
                              std::less<OffsetType>, // Standard ordering
                              STDAllocatorRawMem<std::pair<OffsetType, TFreeBlocksByOffsetMap::iterator>> // Raw memory allocator
                        > TFreeBlocksBySizeMap;

        struct FreeBlockInfo
        {
            // Block size (no reserved space)
            OffsetType Size;

            // Iterator referencing this block in the multimap sorted by the block size
            TFreeBlocksBySizeMap::iterator OrderBySizeIt;

            FreeBlockInfo(OffsetType _Size) : Size(_Size){}
        };

        static const OffsetType InvalidOffset = static_cast<OffsetType>(-1);

        FreeBlockListManager(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            m_MaxSize(MaxSize),
            m_FreeSize(MaxSize),
            m_FreeBlocks( STD_ALLOCATOR_RAW_MEM(TFreeBlocksByOffsetMap::value_type, Allocator, "Allocator for std::map<OffsetType, FreeBlockInfo>") ),
            m_FreeBlocksBySize( STD_ALLOCATOR_RAW_MEM(TFreeBlocksBySizeMap::value_type, Allocator, "multimap<OffsetType, TFreeBlocksByOffsetMap::iterator>") )
        {
            AddNewBlock(0, m_MaxSize);

#ifdef _DEBUG
            DbgVerifyList();
#endif
        }

        ~FreeBlockListManager()
        {
            if( !m_FreeBlocks.empty() || !m_FreeBlocksBySize.empty() )
            {
                VERIFY(m_FreeBlocks.size() == 1, "Single free block is expected");
                VERIFY(m_FreeBlocks.begin()->first == 0, "Head chunk offset is expected to be 0");
                VERIFY(m_FreeBlocks.begin()->second.Size == m_MaxSize, "Head chunk size is expected to be ", m_MaxSize);

                VERIFY(m_FreeBlocksBySize.size() == 1, "Single free block is expected");
                VERIFY(m_FreeBlocksBySize.begin()->first == m_MaxSize, "Head chunk size is expected to be ", m_MaxSize);
                VERIFY(m_FreeBlocksBySize.begin()->second == m_FreeBlocks.begin(), "Incorrect first block");
            }
        }

        FreeBlockListManager(FreeBlockListManager&& rhs) : 
            m_FreeBlocks(std::move(rhs.m_FreeBlocks)),
            m_FreeBlocksBySize(std::move(rhs.m_FreeBlocksBySize)),
            m_MaxSize(rhs.m_MaxSize),
            m_FreeSize(rhs.m_FreeSize)
        {
            rhs.m_MaxSize = 0;
            rhs.m_FreeSize = 0;
        }
        FreeBlockListManager& operator = (FreeBlockListManager&& rhs) = default;
        FreeBlockListManager(const FreeBlockListManager&) = delete;
        FreeBlockListManager& operator = (const FreeBlockListManager&) = delete;

        OffsetType Allocate(OffsetType Size)
        {
            VERIFY_EXPR(Size != 0);
            if(m_FreeSize < Size)
                return InvalidOffset;

            // Get the first block that is >= Size
            auto SmallestBlockItIt = m_FreeBlocksBySize.lower_bound(Size);
            if(SmallestBlockItIt == m_FreeBlocksBySize.end())
                return InvalidOffset;

            auto SmallestBlockIt = SmallestBlockItIt->second;
            VERIFY_EXPR(Size <= SmallestBlockIt->second.Size);
            
            auto Offset = SmallestBlockIt->first;
            auto NewOffset = Offset + Size;
            auto NewSize = SmallestBlockIt->second.Size - Size;
            VERIFY_EXPR(SmallestBlockItIt == SmallestBlockIt->second.OrderBySizeIt);
            m_FreeBlocksBySize.erase(SmallestBlockItIt);
            m_FreeBlocks.erase(SmallestBlockIt);
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

            auto NextBlockIt = m_FreeBlocks.upper_bound(Offset); // First element >
#ifdef _DEBUG
            {
                auto LowBnd = m_FreeBlocks.lower_bound(Offset); // First element >=
                // Since zero-size allocations are not allowed, lower bound must always be equal upper bound
                VERIFY_EXPR(LowBnd == NextBlockIt);
            }
#endif
            VERIFY_EXPR(NextBlockIt == m_FreeBlocks.end() || Offset <= NextBlockIt->first + NextBlockIt->second.Size);
            auto PrevBlockIt = NextBlockIt;
            if(PrevBlockIt != m_FreeBlocks.begin())
            {
                --PrevBlockIt;
                VERIFY_EXPR(Offset >=  PrevBlockIt->first + PrevBlockIt->second.Size);
            }
            else
                PrevBlockIt = m_FreeBlocks.end();

            OffsetType NewSize, NewOffset;
            if(PrevBlockIt != m_FreeBlocks.end() && Offset == PrevBlockIt->first + PrevBlockIt->second.Size)
            {
                //  PrevBlock.Offset             Offset
                //       |                          |
                //       |<-----PrevBlock.Size----->|<------Size-------->|
                //
                NewSize = PrevBlockIt->second.Size + Size;
                NewOffset = PrevBlockIt->first;

                if (NextBlockIt != m_FreeBlocks.end() && Offset + Size == NextBlockIt->first)
                {
                    //   PrevBlock.Offset           Offset            NextBlock.Offset      
                    //     |                          |                    |
                    //     |<-----PrevBlock.Size----->|<------Size-------->|<-----NextBlock.Size----->|
                    //
                    NewSize += NextBlockIt->second.Size;
                    m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
                    m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
                    ++NextBlockIt;
                    m_FreeBlocks.erase(PrevBlockIt, NextBlockIt);
                }
                else
                {
                    //   PrevBlock.Offset           Offset                     NextBlock.Offset      
                    //     |                          |                             |
                    //     |<-----PrevBlock.Size----->|<------Size-------->| ~ ~ ~  |<-----NextBlock.Size----->|
                    //
                    m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
                    m_FreeBlocks.erase(PrevBlockIt);
                }
            }
            else if (NextBlockIt != m_FreeBlocks.end() && Offset + Size == NextBlockIt->first)
            {
                //   PrevBlock.Offset                   Offset            NextBlock.Offset      
                //     |                                  |                    |
                //     |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->|<-----NextBlock.Size----->|
                //
                NewSize = Size + NextBlockIt->second.Size;
                NewOffset = Offset;
                m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
                m_FreeBlocks.erase(NextBlockIt);
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
        size_t DbgGetNumFreeBlocks()const{return m_FreeBlocks.size();}
#endif

    private:
        void AddNewBlock(OffsetType Offset, OffsetType Size)
        {
            auto NewBlockIt = m_FreeBlocks.emplace(Offset, Size);
            VERIFY_EXPR(NewBlockIt.second);
            auto OrderIt = m_FreeBlocksBySize.emplace(Size, NewBlockIt.first);
            NewBlockIt.first->second.OrderBySizeIt = OrderIt;
        }


#ifdef _DEBUG
        void DbgVerifyList()
        {
            OffsetType TotalFreeSize = 0;

            auto BlockIt = m_FreeBlocks.begin();
            auto PrevBlockIt = m_FreeBlocks.end();
            VERIFY_EXPR(m_FreeBlocks.size() == m_FreeBlocksBySize.size());
            while (BlockIt != m_FreeBlocks.end())
            {
                VERIFY_EXPR(BlockIt->first >= 0 && BlockIt->first + BlockIt->second.Size <= m_MaxSize);
                VERIFY_EXPR(BlockIt == BlockIt->second.OrderBySizeIt->second);
                VERIFY(PrevBlockIt == m_FreeBlocks.end() || BlockIt->first > PrevBlockIt->first + PrevBlockIt->second.Size, "Adjoint blocks detected" );
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

        TFreeBlocksByOffsetMap m_FreeBlocks;
        TFreeBlocksBySizeMap m_FreeBlocksBySize;
        
        OffsetType m_MaxSize = 0;
        OffsetType m_FreeSize = 0;
    };
}
