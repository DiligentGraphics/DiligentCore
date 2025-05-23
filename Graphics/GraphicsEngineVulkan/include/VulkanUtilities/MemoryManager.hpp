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

#pragma once

#include <mutex>
#include <array>
#include <unordered_map>
#include <atomic>
#include <string>
#include "MemoryAllocator.h"
#include "VariableSizeAllocationsManager.hpp"
#include "VulkanUtilities/PhysicalDevice.hpp"
#include "VulkanUtilities/LogicalDevice.hpp"
#include "VulkanUtilities/ObjectWrappers.hpp"
#include "HashUtils.hpp"

namespace VulkanUtilities
{

class MemoryPage;
class MemoryManager;

struct MemoryAllocation
{
    MemoryAllocation() noexcept {}

    // clang-format off
    MemoryAllocation            (const MemoryAllocation&) = delete;
    MemoryAllocation& operator= (const MemoryAllocation&) = delete;

	MemoryAllocation(MemoryPage* _Page, VkDeviceSize _UnalignedOffset, VkDeviceSize _Size)noexcept :
        Page           {_Page           },
        UnalignedOffset{_UnalignedOffset},
        Size           {_Size           }
    {}

    MemoryAllocation(MemoryAllocation&& rhs)noexcept :
        Page           {rhs.Page           },
        UnalignedOffset{rhs.UnalignedOffset},
        Size           {rhs.Size           }
    {
        rhs.Page            = nullptr;
        rhs.UnalignedOffset = 0;
        rhs.Size            = 0;
    }

    MemoryAllocation& operator= (MemoryAllocation&& rhs)noexcept
    {
        Page            = rhs.Page;
        UnalignedOffset = rhs.UnalignedOffset;
        Size            = rhs.Size;

        rhs.Page            = nullptr;
        rhs.UnalignedOffset = 0;
        rhs.Size            = 0;

        return *this;
    }
    // clang-format on

    bool IsValid() const
    {
        return Page != nullptr;
    }
    explicit operator bool() const
    {
        return IsValid();
    }

    // Destructor immediately returns the allocation to the parent page.
    // The allocation must not be in use by the GPU.
    ~MemoryAllocation();

    MemoryPage*  Page            = nullptr; // Memory page that contains this allocation
    VkDeviceSize UnalignedOffset = 0;       // Unaligned offset from the start of the memory
    VkDeviceSize Size            = 0;       // Reserved size of this allocation
};

class MemoryPage
{
public:
    MemoryPage(MemoryManager&        ParentMemoryMgr,
               VkDeviceSize          PageSize,
               uint32_t              MemoryTypeIndex,
               bool                  IsHostVisible,
               VkMemoryAllocateFlags AllocateFlags);
    ~MemoryPage();

    // clang-format off
    MemoryPage(MemoryPage&& rhs)noexcept :
        m_ParentMemoryMgr {rhs.m_ParentMemoryMgr         },
        m_AllocationMgr   {std::move(rhs.m_AllocationMgr)},
        m_VkMemory        {std::move(rhs.m_VkMemory)     },
        m_CPUMemory       {rhs.m_CPUMemory               }
    {
        rhs.m_CPUMemory = nullptr;
    }

    MemoryPage            (const MemoryPage&) = delete;
    MemoryPage& operator= (MemoryPage&)       = delete;
    MemoryPage& operator= (MemoryPage&& rhs)  = delete;

    bool IsEmpty() const { return m_AllocationMgr.IsEmpty(); }
    bool IsFull()  const { return m_AllocationMgr.IsFull();  }
    VkDeviceSize GetPageSize() const { return m_AllocationMgr.GetMaxSize();  }
    VkDeviceSize GetUsedSize() const { return m_AllocationMgr.GetUsedSize(); }

    // clang-format on

    MemoryAllocation Allocate(VkDeviceSize size, VkDeviceSize alignment);

    VkDeviceMemory GetVkMemory() const { return m_VkMemory; }
    void*          GetCPUMemory() const { return m_CPUMemory; }

private:
    using AllocationsMgrOffsetType = Diligent::VariableSizeAllocationsManager::OffsetType;

    friend struct MemoryAllocation;

    // Memory is reclaimed immediately. The application is responsible to ensure it is not in use by the GPU
    void Free(MemoryAllocation&& Allocation);

    MemoryManager&                           m_ParentMemoryMgr;
    std::mutex                               m_Mutex;
    Diligent::VariableSizeAllocationsManager m_AllocationMgr;
    VulkanUtilities::DeviceMemoryWrapper     m_VkMemory;
    void*                                    m_CPUMemory = nullptr;
};

class MemoryManager
{
public:
    // clang-format off
	MemoryManager(std::string                  MgrName,
                  const LogicalDevice&         Device,
                  const PhysicalDevice&        PhysDevice,
                  Diligent::IMemoryAllocator&  Allocator,
                  VkDeviceSize                 DeviceLocalPageSize,
                  VkDeviceSize                 HostVisiblePageSize,
                  VkDeviceSize                 DeviceLocalReserveSize,
                  VkDeviceSize                 HostVisibleReserveSize) :
        m_MgrName               {std::move(MgrName)    },
        m_LogicalDevice         {Device                },
        m_PhysicalDevice        {PhysDevice            },
        m_Allocator             {Allocator             },
        m_DeviceLocalPageSize   {DeviceLocalPageSize   },
        m_HostVisiblePageSize   {HostVisiblePageSize   },
        m_DeviceLocalReserveSize{DeviceLocalReserveSize},
        m_HostVisibleReserveSize{HostVisibleReserveSize}
    {}


    // We have to write this constructor because on msvc default
    // constructor is not labeled with noexcept, which makes all
    // std containers use copy instead of move
    MemoryManager(MemoryManager&& rhs)noexcept :
        m_MgrName         {std::move(rhs.m_MgrName)},
        m_LogicalDevice   {rhs.m_LogicalDevice     },
        m_PhysicalDevice  {rhs.m_PhysicalDevice    },
        m_Allocator       {rhs.m_Allocator         },
        m_Pages           {std::move(rhs.m_Pages)  },

        m_DeviceLocalPageSize    {rhs.m_DeviceLocalPageSize   },
        m_HostVisiblePageSize    {rhs.m_HostVisiblePageSize   },
        m_DeviceLocalReserveSize {rhs.m_DeviceLocalReserveSize},
        m_HostVisibleReserveSize {rhs.m_HostVisibleReserveSize},

        //m_CurrUsedSize      {rhs.m_CurrUsedSize},
        m_PeakUsedSize      {rhs.m_PeakUsedSize     },
        m_CurrAllocatedSize {rhs.m_CurrAllocatedSize},
        m_PeakAllocatedSize {rhs.m_PeakAllocatedSize}
    {
        // clang-format on
        for (size_t i = 0; i < m_CurrUsedSize.size(); ++i)
            m_CurrUsedSize[i].store(rhs.m_CurrUsedSize[i].load());
    }

    ~MemoryManager();

    // clang-format off
    MemoryManager            (const MemoryManager&) = delete;
    MemoryManager& operator= (const MemoryManager&) = delete;
    MemoryManager& operator= (MemoryManager&&)      = delete;
    // clang-format on

    MemoryAllocation Allocate(VkDeviceSize Size, VkDeviceSize Alignment, uint32_t MemoryTypeIndex, bool HostVisible, VkMemoryAllocateFlags AllocateFlags);
    MemoryAllocation Allocate(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProps, VkMemoryAllocateFlags AllocateFlags);
    void             ShrinkMemory();

protected:
    friend class MemoryPage;

    virtual void OnNewPageCreated(MemoryPage& NewPage) {}
    virtual void OnPageDestroy(MemoryPage& Page) {}

    std::string m_MgrName;

    const LogicalDevice&  m_LogicalDevice;
    const PhysicalDevice& m_PhysicalDevice;

    Diligent::IMemoryAllocator& m_Allocator;

    std::mutex m_PagesMtx;
    struct MemoryPageIndex
    {
        const uint32_t              MemoryTypeIndex;
        const VkMemoryAllocateFlags AllocateFlags;
        const bool                  IsHostVisible;

        // clang-format off
        MemoryPageIndex(uint32_t              _MemoryTypeIndex,
                        bool                  _IsHostVisible,
                        VkMemoryAllocateFlags _AllocateFlags) :
            MemoryTypeIndex{_MemoryTypeIndex},
            AllocateFlags  {_AllocateFlags},
            IsHostVisible  {_IsHostVisible}
        {}

        bool operator == (const MemoryPageIndex& rhs)const
        {
            return MemoryTypeIndex == rhs.MemoryTypeIndex &&
                   AllocateFlags   == rhs.AllocateFlags   &&
                   IsHostVisible   == rhs.IsHostVisible;
        }
        // clang-format on

        struct Hasher
        {
            size_t operator()(const MemoryPageIndex& PageIndex) const
            {
                return Diligent::ComputeHash(PageIndex.MemoryTypeIndex, PageIndex.AllocateFlags, PageIndex.IsHostVisible);
            }
        };
    };
    std::unordered_multimap<MemoryPageIndex, MemoryPage, MemoryPageIndex::Hasher> m_Pages;

    const VkDeviceSize m_DeviceLocalPageSize;
    const VkDeviceSize m_HostVisiblePageSize;
    const VkDeviceSize m_DeviceLocalReserveSize;
    const VkDeviceSize m_HostVisibleReserveSize;

    void OnFreeAllocation(VkDeviceSize Size, bool IsHostVisible);

    // 0 == Device local, 1 == Host-visible
    std::array<std::atomic<int64_t>, 2> m_CurrUsedSize      = {};
    std::array<VkDeviceSize, 2>         m_PeakUsedSize      = {};
    std::array<VkDeviceSize, 2>         m_CurrAllocatedSize = {};
    std::array<VkDeviceSize, 2>         m_PeakAllocatedSize = {};

    // If adding new member, do not forget to update move ctor
};

} // namespace VulkanUtilities
