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

#include <unordered_map>
#include "VulkanUtilities/VulkanMemoryManager.h"

namespace Diligent
{

class RenderDeviceVkImpl;

struct VulkanUploadAllocation
{
    VulkanUploadAllocation(){}
    VulkanUploadAllocation(void* _CPUAddress, VkDeviceSize _Size, VkDeviceSize _Offset, VkBuffer _vkBuffer) :
        CPUAddress(_CPUAddress),
        Size      (_Size),
        Offset    (_Offset),
        vkBuffer  (_vkBuffer)
    {}
    VulkanUploadAllocation             (const VulkanUploadAllocation&) = delete;
    VulkanUploadAllocation& operator = (const VulkanUploadAllocation&) = delete;
    VulkanUploadAllocation             (VulkanUploadAllocation&&) = default;
    VulkanUploadAllocation& operator = (VulkanUploadAllocation&&) = default;

    VkBuffer     vkBuffer   = VK_NULL_HANDLE;	    // Vulkan buffer associated with this memory.
    void*        CPUAddress = nullptr;
    VkDeviceSize Size       = 0;
    VkDeviceSize Offset     = 0;
};

class VulkanUploadHeap
{
public:
    VulkanUploadHeap(RenderDeviceVkImpl& RenderDevice,
                     std::string         HeapName,
                     VkDeviceSize        PageSize);
    
    VulkanUploadHeap            (VulkanUploadHeap&&)      = delete;
    VulkanUploadHeap            (const VulkanUploadHeap&) = delete;
    VulkanUploadHeap& operator= (VulkanUploadHeap&)       = delete;
    VulkanUploadHeap& operator= (VulkanUploadHeap&& rhs)  = delete;

    ~VulkanUploadHeap();

    VulkanUploadAllocation Allocate(size_t SizeInBytes);
    void DiscardAllocations(uint64_t FenceValue);

    size_t GetStaleAllocationsCount()const
    {
        return m_Pages.size();
    }

private:
    RenderDeviceVkImpl&                   m_RenderDevice;
    std::string                           m_HeapName;
    struct UploadPageInfo
    {
        VulkanUtilities::VulkanMemoryAllocation MemAllocation;
        VulkanUtilities::BufferWrapper          Buffer;
        Uint8*                                  CPUAddress = nullptr;
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

    const VkDeviceSize m_PageSize;

    UploadPageInfo CreateNewPage(VkDeviceSize SizeInBytes);
};

}
