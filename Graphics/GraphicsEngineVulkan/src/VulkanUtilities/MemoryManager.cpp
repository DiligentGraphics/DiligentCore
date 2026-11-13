/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "pch.h"
#include <sstream>
#include "VulkanUtilities/MemoryManager.hpp"

namespace VulkanUtilities
{

MemoryAllocation::~MemoryAllocation()
{
    if (Page != nullptr)
    {
        Page->Free(std::move(*this));
    }
}

MemoryPage::MemoryPage(MemoryManager&        ParentMemoryMgr,
                       VkDeviceSize          PageSize,
                       uint32_t              MemoryTypeIndex,
                       bool                  IsHostVisible,
                       VkMemoryAllocateFlags AllocateFlags) :
    // clang-format off
    m_ParentMemoryMgr{ParentMemoryMgr},
    m_AllocationMgr  {static_cast<AllocationsMgrOffsetType>(PageSize), ParentMemoryMgr.m_Allocator}
// clang-format on
{
    VERIFY(PageSize <= std::numeric_limits<AllocationsMgrOffsetType>::max(),
           "PageSize (", PageSize, ") exceeds maximum allowed value ",
           std::numeric_limits<AllocationsMgrOffsetType>::max());

    VkMemoryAllocateInfo      MemAlloc    = {};
    VkMemoryAllocateFlagsInfo MemFlagInfo = {};

    MemAlloc.pNext           = nullptr;
    MemAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize  = PageSize;
    MemAlloc.memoryTypeIndex = MemoryTypeIndex;

    if (AllocateFlags)
    {
        MemAlloc.pNext    = &MemFlagInfo;
        MemFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        MemFlagInfo.pNext = nullptr;
        MemFlagInfo.flags = AllocateFlags;
    }

    std::string MemoryName = Diligent::FormatString("Device memory page. Size: ", Diligent::FormatMemorySize(PageSize, 2), ", type: ", MemoryTypeIndex);
    m_VkMemory             = ParentMemoryMgr.m_LogicalDevice.AllocateDeviceMemory(MemAlloc, MemoryName.c_str());

    if (IsHostVisible)
    {
        VkResult err = ParentMemoryMgr.m_LogicalDevice.MapMemory(
            m_VkMemory,
            0, // offset
            PageSize,
            0, // flags, reserved for future use
            &m_CPUMemory);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to map staging memory");
    }
}

MemoryPage::~MemoryPage()
{
    if (m_CPUMemory != nullptr)
    {
        // Unmapping memory is not necessary, byt anyway
        m_ParentMemoryMgr.m_LogicalDevice.UnmapMemory(m_VkMemory);
    }

    VERIFY(IsEmpty(), "Destroying a page with not all allocations released");
}

bool MemoryPage::IsEmpty() const
{
    std::lock_guard<std::mutex> Lock{m_Mutex};
    return m_AllocationMgr.IsEmpty();
}

bool MemoryPage::IsFull() const
{
    std::lock_guard<std::mutex> Lock{m_Mutex};
    return m_AllocationMgr.IsFull();
}

VkDeviceSize MemoryPage::GetUsedSize() const
{
    std::lock_guard<std::mutex> Lock{m_Mutex};
    return m_AllocationMgr.GetUsedSize();
}

MemoryAllocation MemoryPage::Allocate(VkDeviceSize size, VkDeviceSize alignment, bool RequireEmpty)
{
    std::lock_guard<std::mutex> Lock{m_Mutex};
    if (RequireEmpty && !m_AllocationMgr.IsEmpty())
        return MemoryAllocation{};
    VERIFY(size <= std::numeric_limits<AllocationsMgrOffsetType>::max(),
           "Allocation size (", size, ") exceeds maximum allowed value ",
           std::numeric_limits<AllocationsMgrOffsetType>::max());
    Diligent::VariableSizeAllocationsManager::Allocation Allocation =
        m_AllocationMgr.Allocate(static_cast<AllocationsMgrOffsetType>(size), static_cast<AllocationsMgrOffsetType>(alignment));
    if (Allocation.IsValid())
    {
        // Offset may not necessarily be aligned, but the allocation is guaranteed to be large enough
        // to accommodate requested alignment
        VERIFY_EXPR(Diligent::AlignUp(VkDeviceSize{Allocation.UnalignedOffset}, alignment) - Allocation.UnalignedOffset + size <= Allocation.Size);
        return MemoryAllocation{this, Allocation.UnalignedOffset, Allocation.Size};
    }
    else
    {
        return MemoryAllocation{};
    }
}

void MemoryPage::Free(MemoryAllocation&& Allocation)
{
    m_ParentMemoryMgr.OnFreeAllocation(Allocation.Size, m_CPUMemory != nullptr);
    std::lock_guard<std::mutex> Lock{m_Mutex};
    VERIFY_EXPR(Allocation.UnalignedOffset <= std::numeric_limits<AllocationsMgrOffsetType>::max());
    VERIFY_EXPR(Allocation.Size <= std::numeric_limits<AllocationsMgrOffsetType>::max());
    m_AllocationMgr.Free(static_cast<AllocationsMgrOffsetType>(Allocation.UnalignedOffset), static_cast<AllocationsMgrOffsetType>(Allocation.Size));
    Allocation = MemoryAllocation{};
}

MemoryAllocation MemoryManager::Allocate(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProps, VkMemoryAllocateFlags AllocateFlags)
{
    // memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource.
    // Bit i is set if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the
    // physical device is supported for the resource.
    uint32_t MemoryTypeIndex = m_PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, MemoryProps);
    if (MemoryProps == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        // There must be at least one memory type with the DEVICE_LOCAL_BIT bit set
        DEV_CHECK_ERR(MemoryTypeIndex != PhysicalDevice::InvalidMemoryTypeIndex,
                      "Vulkan spec requires that memoryTypeBits member always contains "
                      "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has the "
                      "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit set (11.6)");
    }
    else if (MemoryProps == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        DEV_CHECK_ERR(MemoryTypeIndex != PhysicalDevice::InvalidMemoryTypeIndex,
                      "Vulkan spec requires that for a VkBuffer not created with the VK_BUFFER_CREATE_SPARSE_BINDING_BIT "
                      "bit set, or for a VkImage that was created with a VK_IMAGE_TILING_LINEAR value in the tiling member "
                      "of the VkImageCreateInfo structure passed to vkCreateImage, the memoryTypeBits member always contains "
                      "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has both the "
                      "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit AND the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit set. (11.6)");
    }
    else if (MemoryTypeIndex == PhysicalDevice::InvalidMemoryTypeIndex)
    {
        LOG_ERROR_AND_THROW("Failed to find suitable device memory type for a buffer");
    }

    bool HostVisible = (MemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    return Allocate(MemReqs.size, MemReqs.alignment, MemoryTypeIndex, HostVisible, AllocateFlags);
}

MemoryAllocation MemoryManager::Allocate(VkDeviceSize Size, VkDeviceSize Alignment, uint32_t MemoryTypeIndex, bool HostVisible, VkMemoryAllocateFlags AllocateFlags)
{
    // The suballocator doubles its alignment while initializing a page. Keep sizes below
    // half its offset range to avoid overflow there as well as in alignment arithmetic.
    constexpr VkDeviceSize MaxSize = std::numeric_limits<Diligent::VariableSizeAllocationsManager::OffsetType>::max() / 2;
    if (Size == 0 || Alignment == 0 || (Alignment & (Alignment - 1)) != 0 ||
        Alignment > MaxSize || Size > MaxSize - (Alignment - 1))
        LOG_ERROR_AND_THROW("Invalid Vulkan memory allocation size or alignment");

    const VkDeviceSize AlignedSize = Diligent::AlignUp(Size, Alignment);
    MemoryAllocation   Allocation;

    // On integrated GPUs, there is no difference between host-visible and GPU-only
    // memory, so MemoryTypeIndex is the same. As GPU-only pages do not have CPU address,
    // we need to use HostVisible flag to differentiate the two.
    // It is likely a good idea to always keep staging pages separate to reduce fragmentation
    // even though on integrated GPUs same pages can be used for both GPU-only and staging
    // allocations. Staging allocations are short-living and will be released when upload is
    // complete, while GPU-only allocations are expected to be long-living.
    MemoryPageIndex             PageIdx{MemoryTypeIndex, HostVisible, AllocateFlags};
    std::lock_guard<std::mutex> Lock{m_PagesMtx};

    const VkDeviceSize BasePageSize = HostVisible ? m_HostVisiblePageSize : m_DeviceLocalPageSize;

    auto range             = m_Pages.equal_range(PageIdx);
    auto BestOversizedPage = range.second;
    for (auto page_it = range.first; page_it != range.second; ++page_it)
    {
        MemoryPage& Page = page_it->second;
        if (Page.GetPageSize() > BasePageSize)
        {
            // Small requests must not pin large pages or bypass available shared space.
            if (AlignedSize > BasePageSize && Page.GetPageSize() >= AlignedSize && Page.IsEmpty() &&
                (BestOversizedPage == range.second || Page.GetPageSize() < BestOversizedPage->second.GetPageSize()))
                BestOversizedPage = page_it;
            continue;
        }
        Allocation = Page.Allocate(Size, Alignment);
        if (Allocation.Page != nullptr)
            break;
    }
    if (Allocation.Page == nullptr && BestOversizedPage != range.second)
        Allocation = BestOversizedPage->second.Allocate(Size, Alignment, true);

    size_t stat_ind = HostVisible ? 1 : 0;
    if (Allocation.Page == nullptr)
    {
        // Collection may precede deferred releases. Reclaim unusable isolated pages
        // before asking Vulkan for more memory, even if the next collection has not run.
        for (auto page_it = range.first; page_it != range.second;)
        {
            MemoryPage& Page = page_it->second;
            if (Page.GetPageSize() > BasePageSize && Page.IsEmpty())
            {
                m_CurrAllocatedSize[stat_ind] -= Page.GetPageSize();
                OnPageDestroy(Page);
                page_it = m_Pages.erase(page_it);
            }
            else
                ++page_it;
        }

        // The suballocator rounds the requested size up to Alignment, even at offset zero.
        const VkDeviceSize PageSize = std::max(BasePageSize, AlignedSize);
        auto               it       = m_Pages.emplace(PageIdx, MemoryPage{*this, PageSize, MemoryTypeIndex, HostVisible, AllocateFlags});

        // Page construction, mapping and map insertion may throw. Count only resident pages.
        m_CurrAllocatedSize[stat_ind] += PageSize;
        m_PeakAllocatedSize[stat_ind] = std::max(m_PeakAllocatedSize[stat_ind], m_CurrAllocatedSize[stat_ind]);

        LOG_INFO_MESSAGE("MemoryManager '", m_MgrName, "': created new ", (HostVisible ? "host-visible" : "device-local"),
                         " page. (", Diligent::FormatMemorySize(PageSize, 2), ", type idx: ", MemoryTypeIndex,
                         "). Current allocated size: ", Diligent::FormatMemorySize(m_CurrAllocatedSize[stat_ind], 2));
        OnNewPageCreated(it->second);
        Allocation = it->second.Allocate(Size, Alignment);
        DEV_CHECK_ERR(Allocation.Page != nullptr, "Failed to allocate new memory page");
    }

    if (Allocation.Page != nullptr)
    {
        VERIFY_EXPR(Size + Diligent::AlignUp(Allocation.UnalignedOffset, Alignment) - Allocation.UnalignedOffset <= Allocation.Size);
    }

    m_CurrUsedSize[stat_ind].fetch_add(Allocation.Size);
    m_PeakUsedSize[stat_ind] = std::max(m_PeakUsedSize[stat_ind], static_cast<VkDeviceSize>(m_CurrUsedSize[stat_ind].load()));

    return Allocation;
}

void MemoryManager::ShrinkMemory()
{
    std::lock_guard<std::mutex> Lock{m_PagesMtx};

    // Isolated pages must not consume the reserve for shared pages.
    std::array<VkDeviceSize, 2> SharedAllocatedSize = {};
    for (const auto& Entry : m_Pages)
    {
        const bool         HostVisible  = Entry.first.IsHostVisible;
        const VkDeviceSize BasePageSize = HostVisible ? m_HostVisiblePageSize : m_DeviceLocalPageSize;
        if (Entry.second.GetPageSize() <= BasePageSize)
            SharedAllocatedSize[HostVisible ? 1 : 0] += Entry.second.GetPageSize();
    }

    auto it = m_Pages.begin();
    while (it != m_Pages.end())
    {
        auto curr_it = it;
        ++it;
        MemoryPage&  Page          = curr_it->second;
        bool         IsHostVisible = Page.GetCPUMemory() != nullptr;
        VkDeviceSize ReserveSize   = IsHostVisible ? m_HostVisibleReserveSize : m_DeviceLocalReserveSize;
        VkDeviceSize BasePageSize  = IsHostVisible ? m_HostVisiblePageSize : m_DeviceLocalPageSize;
        if (Page.IsEmpty() && (Page.GetPageSize() > BasePageSize || SharedAllocatedSize[IsHostVisible ? 1 : 0] > ReserveSize))
        {
            VkDeviceSize PageSize = Page.GetPageSize();
            if (PageSize <= BasePageSize)
                SharedAllocatedSize[IsHostVisible ? 1 : 0] -= PageSize;
            m_CurrAllocatedSize[IsHostVisible ? 1 : 0] -= PageSize;
            LOG_INFO_MESSAGE("MemoryManager '", m_MgrName, "': destroying ", (IsHostVisible ? "host-visible" : "device-local"),
                             " page (", Diligent::FormatMemorySize(PageSize, 2),
                             "). Current allocated size: ",
                             Diligent::FormatMemorySize(m_CurrAllocatedSize[IsHostVisible ? 1 : 0], 2));
            OnPageDestroy(Page);
            m_Pages.erase(curr_it);
        }
    }
}

void MemoryManager::OnFreeAllocation(VkDeviceSize Size, bool IsHostVisible)
{
    m_CurrUsedSize[IsHostVisible ? 1 : 0].fetch_add(-static_cast<int64_t>(Size));
}

MemoryManager::~MemoryManager()
{
    VkDeviceSize PeakDeviceLocalPages = m_PeakAllocatedSize[0] / m_DeviceLocalPageSize;
    VkDeviceSize PeakHostVisiblePages = m_PeakAllocatedSize[1] / m_HostVisiblePageSize;
    LOG_INFO_MESSAGE("MemoryManager '", m_MgrName, "' stats:\n"
                                                   "                       Peak used/allocated device-local memory size: ",
                     Diligent::FormatMemorySize(m_PeakUsedSize[0], 2, m_PeakAllocatedSize[0]), " / ",
                     Diligent::FormatMemorySize(m_PeakAllocatedSize[0], 2, m_PeakAllocatedSize[0]),
                     " (", PeakDeviceLocalPages, (PeakDeviceLocalPages == 1 ? " page)" : " pages)"),
                     "\n                       Peak used/allocated host-visible memory size: ",
                     Diligent::FormatMemorySize(m_PeakUsedSize[1], 2, m_PeakAllocatedSize[1]), " / ",
                     Diligent::FormatMemorySize(m_PeakAllocatedSize[1], 2, m_PeakAllocatedSize[1]),
                     " (", PeakHostVisiblePages, (PeakHostVisiblePages == 1 ? " page)" : " pages)"));

    for (auto it = m_Pages.begin(); it != m_Pages.end(); ++it)
        VERIFY(it->second.IsEmpty(), "The page contains outstanding allocations");
    VERIFY(m_CurrUsedSize[0] == 0 && m_CurrUsedSize[1] == 0, "Not all allocations have been released");
}

} // namespace VulkanUtilities
