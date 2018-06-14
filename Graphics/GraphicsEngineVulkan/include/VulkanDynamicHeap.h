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

#include "Vulkan.h"
#include "RingBuffer.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

class RenderDeviceVkImpl;
class VulkanDynamicHeap;

struct VulkanDynamicAllocation
{
    VulkanDynamicAllocation(){}

    VulkanDynamicAllocation(VulkanDynamicHeap& _ParentHeap, size_t _Offset, size_t _Size) :
        pParentDynamicHeap(&_ParentHeap),
        Offset           (_Offset), 
        Size             (_Size)
    {}

    VulkanDynamicAllocation             (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation& operator = (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation             (VulkanDynamicAllocation&& rhs)noexcept :
        pParentDynamicHeap(rhs.pParentDynamicHeap),
        Offset            (rhs.Offset),
        Size              (rhs.Size)
#ifdef _DEBUG
        , dbgFrameNumber(rhs.dbgFrameNumber)
#endif
    {
        rhs.pParentDynamicHeap = nullptr;
        rhs.Offset = 0;
        rhs.Size = 0;
#ifdef _DEBUG
        rhs.dbgFrameNumber = 0;
#endif
    }

    VulkanDynamicAllocation& operator = (VulkanDynamicAllocation&& rhs)noexcept // Must be noexcept on MSVC, so can't use = default
    {
        pParentDynamicHeap = rhs.pParentDynamicHeap;
        Offset             = rhs.Offset;
        Size               = rhs.Size;
        rhs.pParentDynamicHeap = nullptr;
        rhs.Offset             = 0;
        rhs.Size               = 0;
#ifdef _DEBUG
        dbgFrameNumber = rhs.dbgFrameNumber;
        rhs.dbgFrameNumber = 0;
#endif
        return *this;
    }

    VulkanDynamicHeap* pParentDynamicHeap = nullptr;
    size_t             Offset             = 0;		// Offset from the start of the buffer resource
    size_t             Size               = 0;	    // Reserved size of this allocation
#ifdef _DEBUG
    Uint64             dbgFrameNumber     = 0;
#endif
};

class VulkanDynamicHeap
{
public:
    VulkanDynamicHeap(IMemoryAllocator&         Allocator, 
                      class RenderDeviceVkImpl* pDeviceVk, 
                      Uint32                    ImmediateCtxHeapSize, 
                      Uint32                    DeferredCtxHeapSize,
                      Uint32                    DeferredCtxCount);
    ~VulkanDynamicHeap();

    VulkanDynamicHeap            (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap            (VulkanDynamicHeap&&)      = delete;
    VulkanDynamicHeap& operator= (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap& operator= (VulkanDynamicHeap&&)      = delete;

    VulkanDynamicAllocation Allocate( Uint32 CtxId, size_t SizeInBytes, size_t Alignment = 0);

    void FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue);
    void Destroy();

    VkBuffer GetVkBuffer()  const{return m_VkBuffer;}
    Uint8*   GetCPUAddress()const{return m_CPUAddress;}

private:
    struct VulkanRingBuffer
    {
        VulkanRingBuffer(Uint32 Size, IMemoryAllocator &Allocator, Uint32 _BaseOffset) :
            RingBuff(Size, Allocator),
            BaseOffset(_BaseOffset)
        {}
        RingBuffer   RingBuff;
        const Uint32 BaseOffset;
    };
    std::vector<VulkanRingBuffer>   m_RingBuffers;
    RenderDeviceVkImpl* const       m_pDeviceVk;

    VulkanUtilities::BufferWrapper       m_VkBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
    Uint8*                               m_CPUAddress;
    const uint32_t                       m_DefaultAlignment;
};

}
