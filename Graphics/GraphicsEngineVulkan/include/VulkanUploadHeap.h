/*     Copyright 2019 Diligent Graphics LLC
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

#include <unordered_map>
#include "VulkanUtilities/VulkanMemoryManager.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

// Upload heap is used by a device context to update texture and buffer regions through 
// UpdateBufferRegion() and UpdateTextureRegion().
// 
// The heap allocates pages from the global memory manager.
// The pages are released and returned to the manager at the end of every frame.
// 
//   _______________________________________________________________________________________________________________________________
//  |                                                                                                                               |
//  |                                                  VulkanUploadHeap                                                             |
//  |                                                                                                                               |
//  |  || - - - - - - - - - - Page[0] - - - - - - - - - - -||    || - - - - - - - - - - Page[1] - - - - - - - - - - -||             |
//  |  || Allocation0 | Allocation1 |  ...   | AllocationN ||    || Allocation0 | Allocation1 |  ...   | AllocationM ||   ...       |
//  |__________|____________________________________________________________________________________________________________________|
//             |                                      A                   |
//             |                                      |                   |
//             |Allocate()             CreateNewPage()|                   |ReleaseAllocatedPages()
//             |                                ______|___________________V____     
//             V                               |                              |
//   VulkanUploadAllocation                    |    Global Memory Manager     |
//                                             |    (VulkanMemoryManager)     |
//                                             |                              |
//                                             |______________________________|
//
class RenderDeviceVkImpl;

struct VulkanUploadAllocation
{
    VulkanUploadAllocation() noexcept {}
    VulkanUploadAllocation(void*        _CPUAddress,
                           VkDeviceSize _Size,
                           VkDeviceSize _AlignedOffset,
                           VkBuffer     _vkBuffer) noexcept :
        vkBuffer     (_vkBuffer),
        CPUAddress   (_CPUAddress),
        Size         (_Size),
        AlignedOffset(_AlignedOffset)
    {}
    VulkanUploadAllocation             (const VulkanUploadAllocation&)  = delete;
    VulkanUploadAllocation& operator = (const VulkanUploadAllocation&)  = delete;
    VulkanUploadAllocation             (      VulkanUploadAllocation&&) = default;
    VulkanUploadAllocation& operator = (      VulkanUploadAllocation&&) = default;

    VkBuffer     vkBuffer      = VK_NULL_HANDLE; // Vulkan buffer associated with this memory.
    void*        CPUAddress    = nullptr;
    VkDeviceSize Size          = 0;
    VkDeviceSize AlignedOffset = 0;
};

class VulkanUploadHeap
{
public:
    VulkanUploadHeap(RenderDeviceVkImpl& RenderDevice,
                     std::string         HeapName,
                     VkDeviceSize        PageSize);
    
    VulkanUploadHeap            (const VulkanUploadHeap&)  = delete;
    VulkanUploadHeap            (      VulkanUploadHeap&&) = delete;
    VulkanUploadHeap& operator= (const VulkanUploadHeap&)  = delete;
    VulkanUploadHeap& operator= (      VulkanUploadHeap&&) = delete;

    ~VulkanUploadHeap();

    VulkanUploadAllocation Allocate(size_t SizeInBytes, size_t Alignment);
    
    // Releases all allocated pages that are later returned to the global memory manager by the release queues.
    // As global memory manager is hosted by the render device, the upload heap can be destroyed before the 
    // pages are actually returned to the manager.
    void ReleaseAllocatedPages(Uint64 CmdQueueMask);

    size_t GetStalePagesCount()const
    {
        return m_Pages.size();
    }

private:
    RenderDeviceVkImpl& m_RenderDevice;
    std::string         m_HeapName;
    const VkDeviceSize  m_PageSize;

    struct UploadPageInfo
    {
        UploadPageInfo(VulkanUtilities::VulkanMemoryAllocation&& _MemAllocation, 
                       VulkanUtilities::BufferWrapper&&          _Buffer,
                       Uint8*                                    _CPUAddress) :
            MemAllocation(std::move(_MemAllocation)),
            Buffer       (std::move(_Buffer)),
            CPUAddress   (_CPUAddress)
        {
        }
        VulkanUtilities::VulkanMemoryAllocation MemAllocation;
        VulkanUtilities::BufferWrapper          Buffer;
        Uint8* const                            CPUAddress = nullptr;
    };
    std::vector<UploadPageInfo> m_Pages;

    struct CurrPageInfo
    {
        VkBuffer vkBuffer       = VK_NULL_HANDLE;
        Uint8*   CurrCPUAddress = nullptr;
        size_t   CurrOffset     = 0;
        size_t   AvailableSize  = 0;
        void Reset(UploadPageInfo& NewPage, size_t PageSize)
        {
            vkBuffer       = NewPage.Buffer;
            CurrCPUAddress = NewPage.CPUAddress;
            CurrOffset     = 0;
            AvailableSize  = PageSize;
        }
        void Advance(size_t SizeInBytes)
        {
            CurrCPUAddress+= SizeInBytes;
            CurrOffset    += SizeInBytes;
            AvailableSize -= SizeInBytes;
        }
    }m_CurrPage;

    size_t   m_CurrFrameSize   = 0;
    size_t   m_PeakFrameSize   = 0;
    size_t   m_CurrAllocatedSize   = 0;
    size_t   m_PeakAllocatedSize   = 0;

    UploadPageInfo CreateNewPage(VkDeviceSize SizeInBytes)const;
};

}
