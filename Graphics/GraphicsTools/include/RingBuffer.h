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
        struct FrameTailAttribs
        {
            FrameTailAttribs(Uint64 fn, OffsetType off, OffsetType sz) : 
                FrameNum(fn),
                Offset(off),
                Size(sz)
            {}
            Uint64 FrameNum;
            OffsetType Offset;
            OffsetType Size;
        };
        static const OffsetType InvalidOffset = static_cast<OffsetType>(-1);

        RingBuffer(OffsetType MaxSize, IMemoryAllocator &Allocator) : 
            m_CompletedFrameTails(0, FrameTailAttribs(0,0,0), STD_ALLOCATOR_RAW_MEM(FrameTailAttribs, Allocator, "Allocator for vector<FrameNumOffsetPair>" )),
            m_MaxSize(MaxSize)
        {}

        RingBuffer(RingBuffer&& rhs) : 
            m_CompletedFrameTails(std::move(rhs.m_CompletedFrameTails)),
            m_Head(rhs.m_Head),
            m_Tail(rhs.m_Tail),
            m_MaxSize(rhs.m_MaxSize),
            m_UsedSize(rhs.m_UsedSize),
            m_CurrFrameSize(rhs.m_CurrFrameSize)
        {
            rhs.m_Head = 0;
            rhs.m_Tail = 0;
            rhs.m_MaxSize = 0;
            rhs.m_UsedSize = 0;
            rhs.m_CurrFrameSize = 0;
        }

        ~RingBuffer()
        {
            VERIFY(m_UsedSize==0, "All space in the ring buffer must be released")
        }

        RingBuffer& operator = (RingBuffer&& rhs)
        {
            m_CompletedFrameTails = std::move(rhs.m_CompletedFrameTails);
            m_Head = rhs.m_Head;
            m_Tail = rhs.m_Tail;
            m_MaxSize = rhs.m_MaxSize;
            m_UsedSize = rhs.m_UsedSize;
            m_CurrFrameSize = rhs.m_CurrFrameSize;

            rhs.m_MaxSize = 0;
            rhs.m_Head = 0;
            rhs.m_Tail = 0;
            rhs.m_UsedSize = 0;
            rhs.m_CurrFrameSize = 0;

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
                    m_CurrFrameSize += Size;
                    return Offset;
                }
                else if(Size <= m_Head)
                {
                    // Allocate from the beginning of the buffer
                    OffsetType AddSize = (m_MaxSize - m_Tail) + Size;
                    m_UsedSize += AddSize;
                    m_CurrFrameSize += AddSize;
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
                m_CurrFrameSize += Size;
                return Offset;
            }

            return InvalidOffset;
        }

        // FrameNum is the number of the frame (or command list) in which the tail
        // could have been referenced last time
        // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
        void FinishCurrentFrame(Uint64 FrameNum)
        {
            m_CompletedFrameTails.emplace_back(FrameNum, m_Tail, m_CurrFrameSize);
            m_CurrFrameSize = 0;
        }

        // NumCompletedFrames is the number of completed frames (or command lists)
        // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
        void ReleaseCompletedFrames(Uint64 NumCompletedFrames)
        {
            // Command list is completed when its number is strictly less than the number of completed frames
            while(!m_CompletedFrameTails.empty() && m_CompletedFrameTails.front().FrameNum < NumCompletedFrames)
            {
                const auto &OldestFrameTail = m_CompletedFrameTails.front();
                VERIFY_EXPR(OldestFrameTail.Size <= m_MaxSize);
                m_UsedSize -= OldestFrameTail.Size;
                m_Head = OldestFrameTail.Offset;
                m_CompletedFrameTails.pop_front();
            }
        }

        OffsetType GetMaxSize()const{return m_MaxSize;}
        bool IsFull()const{ return m_UsedSize==m_MaxSize; };
        bool IsEmpty()const{ return m_UsedSize==0; };
        OffsetType GetUsedSize()const{return m_UsedSize;}

    private:
        // Consider the following scenario for 1024 buffer:
        // Allocate(512)
        //
        //  h     t     m
        //  |xxxxx|     |
        
        // FinishCurrentFrame(0)
        //
        //        t0
        //  h     t     m
        //  |xxxxx|     |
        
        // ReleaseCompletedFrames(1)
        //
        //        h 
        //        t     m
        //  |     |     |

        // FinishCurrentFrame(1)
        //
        //        t1 
        //        h 
        //        t     m
        //  |     |     |

        // Allocate(512)
        //
        //        t1    t 
        //        h     m
        //  |     |xxxxx|

        // Allocate(512)
        //
        //        t 
        //        t1     
        //        h     m
        //  |xxxxx|xxxxx|

        // FinishCurrentFrame(2)
        // 
        //        t 
        //        t1 
        //        t2     
        //        h     m
        //  |xxxxx|xxxxx|

        // At this point there will be two tails in the queue, both at 512. m_UsedSize will be 0. When
        // ReleaseCompletedFrames(2) is called, there wil be no way to find out if the current frame is 0 
        // or the entire buffer if we don't store the frame size

        std::deque< FrameTailAttribs, STDAllocatorRawMem<FrameTailAttribs> > m_CompletedFrameTails;
        OffsetType m_Head = 0;
        OffsetType m_Tail = 0;
        OffsetType m_MaxSize = 0;
        OffsetType m_UsedSize = 0;
        OffsetType m_CurrFrameSize = 0;
    };
}
