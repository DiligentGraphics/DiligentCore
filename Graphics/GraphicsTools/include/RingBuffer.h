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

/// \file
/// Implementation of Diligent::RingBuffer class


#include <deque>
#include "MemoryAllocator.h"
#include "STDAllocator.h"
#include "DebugUtilities.h"

namespace Diligent
{
    /// Implementation of a ring buffer. The class is not thread-safe.
    class RingBuffer
    {
    public:
        typedef size_t OffsetType;
        typedef std::pair<Uint64,OffsetType> FrameNumOffsetPair;

        static const OffsetType InvalidOffset = static_cast<OffsetType>(-1);

        RingBuffer(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            m_CompletedFrameTails(0, FrameNumOffsetPair(), STD_ALLOCATOR_RAW_MEM(FrameNumOffsetPair, Allocator, "Allocator for vector<FrameNumOffsetPair>" )),
            m_MaxSize(MaxSize)
        {}

        RingBuffer(RingBuffer&& rhs) : 
            m_CompletedFrameTails(std::move(rhs.m_CompletedFrameTails)),
            m_Head(rhs.m_Head),
            m_Tail(rhs.m_Tail),
            m_MaxSize(rhs.m_MaxSize),
            m_UsedSize(rhs.m_UsedSize)
        {
            rhs.m_Head = 0;
            rhs.m_Tail = 0;
            rhs.m_MaxSize = 0;
            rhs.m_UsedSize = 0;
        }

        RingBuffer& operator = (RingBuffer&& rhs)
        {
            m_CompletedFrameTails = std::move(rhs.m_CompletedFrameTails);
            m_Head = rhs.m_Head;
            m_Tail = rhs.m_Tail;
            m_MaxSize = rhs.m_MaxSize;
            m_UsedSize = rhs.m_UsedSize;

            rhs.m_MaxSize = 0;
            rhs.m_Head = 0;
            rhs.m_Tail = 0;
            rhs.m_UsedSize = 0;

            return *this;
        }

        RingBuffer(const RingBuffer&) = delete;
        RingBuffer& operator = (const RingBuffer&) = delete;

        OffsetType Allocate(OffsetType Size)
        {
            if(IsFull())
            {
                return InvalidOffset;
            }

            if (m_Tail >= m_Head )
            {
                //                     Head             Tail     MaxSize
                //                     |                |        |
                //  [                  xxxxxxxxxxxxxxxxx         ]
                //                                         
                //
                if (m_Tail + Size <= m_MaxSize)
                {
                    auto Offset = m_Tail;
                    m_Tail += Size;
                    m_UsedSize += Size;
                    return Offset;
                }
                else if(Size <= m_Head)
                {
                    // Allocate from the beginning of the buffer
                    m_UsedSize += (m_MaxSize - m_Tail) + Size;
                    m_Tail = Size;
                    return 0;
                }
            }
            else if (m_Tail + Size <= m_Head )
            {
                //
                //       Tail          Head             
                //       |             |             
                //  [xxxx              xxxxxxxxxxxxxxxxxxxxxxxxxx]
                //
                auto Offset = m_Tail;
                m_Tail += Size;
                m_UsedSize += Size;
                return Offset;
            }

            return InvalidOffset;
        }

        void FinishCurrentFrame(Uint64 FrameNum)
        {
            m_CompletedFrameTails.push_back(std::make_pair(FrameNum, m_Tail) );
        }

        void ReleaseCompletedFrames(Uint64 NumCompletedFrames)
        {
            while(!m_CompletedFrameTails.empty() && m_CompletedFrameTails.front().first < NumCompletedFrames)
            {
                auto &OldestFrameTail = m_CompletedFrameTails.front().second;
                if( m_UsedSize > 0 )
                {
                    if (OldestFrameTail > m_Head)
                    {
                        //                     m_Head    OldestFrameTail    MaxSize
                        //                     |         |                  |
                        //  [                  xxxxxxxxxxxxxxxxxxxxx        ]
                        //                                        
                        //                    
                        VERIFY_EXPR(m_UsedSize >= OldestFrameTail - m_Head);
                        m_UsedSize -= OldestFrameTail - m_Head;
                    }
                    else
                    {
                        //     OldestFrameTail                  m_Head      MaxSize
                        //             |                        |           |
                        //  [xxxxxxxxxxxxxxxxxxxxxxxx           xxxxxxxxxxxx]
                        //                                        
                        
                        //                                         
                        //               m_Head,OldestFrameTail          MaxSize
                        //                         |                     |
                        //  [xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx]
                        //                                        
                        //        
                        //        
                        VERIFY_EXPR(m_UsedSize >= (m_MaxSize - m_Head) + OldestFrameTail);
                        m_UsedSize -= (m_MaxSize - m_Head);
                        m_UsedSize -= OldestFrameTail;
                    }
                }
                m_Head = OldestFrameTail;
                m_CompletedFrameTails.pop_front();
            }
        }

        OffsetType GetMaxSize()const{return m_MaxSize;}
        bool IsFull()const{ return m_UsedSize==m_MaxSize; };
        bool IsEmpty()const{ return m_UsedSize==0; };
        OffsetType GetUsedSize()const{return m_UsedSize;}

    private:
        std::deque< FrameNumOffsetPair, STDAllocatorRawMem<FrameNumOffsetPair> > m_CompletedFrameTails;
        OffsetType m_Head = 0;
        OffsetType m_Tail = 0;
        OffsetType m_MaxSize = 0;
        OffsetType m_UsedSize = 0;
    };
}
