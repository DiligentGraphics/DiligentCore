/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <thread>
#include <stdexcept>

#include "volk.h"

#include "GPUTestingEnvironment.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DefaultRawMemoryAllocator.hpp"
#include "gtest/gtest.h"

namespace Diligent
{
namespace Testing
{
namespace
{

class TrackingMemoryManager : public VulkanUtilities::MemoryManager
{
public:
    using MemoryManager::MemoryManager;
    size_t       LivePages     = 0;
    size_t       CreatedPages  = 0;
    size_t       PeakLivePages = 0;
    VkDeviceSize AllocatedSize(bool HostVisible) const { return m_CurrAllocatedSize[HostVisible ? 1 : 0]; }
    VkDeviceSize PeakAllocatedSize(bool HostVisible) const { return m_PeakAllocatedSize[HostVisible ? 1 : 0]; }

private:
    void OnNewPageCreated(VulkanUtilities::MemoryPage&) override
    {
        ++LivePages;
        ++CreatedPages;
        PeakLivePages = std::max(PeakLivePages, LivePages);
    }
    void OnPageDestroy(VulkanUtilities::MemoryPage&) override { --LivePages; }
};

class FailingMemoryAllocator : public DefaultRawMemoryAllocator
{
public:
    bool  Fail = false;
    void* Allocate(size_t Size, const Char* Description, const char* File, Int32 Line) override
    {
        if (Fail)
            throw std::bad_alloc{};
        return DefaultRawMemoryAllocator::Allocate(Size, Description, File, Line);
    }
    void* AllocateAligned(size_t Size, size_t Alignment, const Char* Description, const char* File, Int32 Line) override
    {
        if (Fail)
            throw std::bad_alloc{};
        return DefaultRawMemoryAllocator::AllocateAligned(Size, Alignment, Description, File, Line);
    }
};

TEST(VulkanMemoryManagerTest, OversizedPages)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();

    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible ? "host-visible" : "device-local");
        TrackingMemoryManager       Mgr{"Oversized page test", pDevice->GetLogicalDevice(),
                                  pDevice->GetPhysicalDevice(), DefaultRawMemoryAllocator::GetAllocator(),
                                  PageSize, PageSize, 16 * PageSize, 16 * PageSize};
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);

        {
            auto Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Large);
            EXPECT_EQ(Large.Page->GetPageSize(), 3 * PageSize / 2);
            auto* LargePage = Large.Page;
            {
                auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
                ASSERT_TRUE(Small);
                EXPECT_NE(Small.Page, LargePage);
                {
                    auto OtherSmall = Mgr.Allocate(256, 256, Type, HostVisible, 0);
                    ASSERT_TRUE(OtherSmall);
                    EXPECT_EQ(OtherSmall.Page, Small.Page);
                }
                Mgr.ShrinkMemory();
                EXPECT_EQ(Mgr.LivePages, 2u); // Live allocations must survive collection.
                {
                    auto Released = std::move(Large);
                }
                Mgr.ShrinkMemory();
                EXPECT_EQ(Mgr.LivePages, 1u); // Oversized pages bypass the reserve.
                {
                    auto Larger = Mgr.Allocate(2 * PageSize, 256, Type, HostVisible, 0);
                    ASSERT_TRUE(Larger);
                    EXPECT_EQ(Larger.Page->GetPageSize(), 2 * PageSize);
                    EXPECT_NE(Larger.Page, Small.Page);
                }
                Mgr.ShrinkMemory();
                EXPECT_EQ(Mgr.LivePages, 1u);
            }
        }
        Mgr.ShrinkMemory();
        EXPECT_EQ(Mgr.LivePages, 1u); // Empty shared pages still obey the reserve.
        {
            auto Boundary = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Boundary);
            EXPECT_EQ(Mgr.LivePages, 1u);
            EXPECT_EQ(Boundary.Page->GetPageSize(), PageSize);
        }
    }
}

TEST(VulkanMemoryManagerTest, ReuseEmptyOversizedPage)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();

    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible ? "host-visible" : "device-local");
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        TrackingMemoryManager        Mgr{"Oversized reuse test", pDevice->GetLogicalDevice(),
                                  pDevice->GetPhysicalDevice(), DefaultRawMemoryAllocator::GetAllocator(),
                                  PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        VulkanUtilities::MemoryPage* OriginalPage = nullptr;
        {
            auto Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Large);
            OriginalPage = Large.Page;
            // Match collection before completed deferred releases are purged.
            Mgr.ShrinkMemory();
        }
        {
            auto Replacement = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Replacement);
            EXPECT_EQ(Replacement.Page, OriginalPage);
            EXPECT_EQ(Mgr.CreatedPages, 1u);
            EXPECT_EQ(Mgr.LivePages, 1u);
            auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Small);
            EXPECT_NE(Small.Page, Replacement.Page);
        }
        Mgr.ShrinkMemory();
        EXPECT_EQ(Mgr.LivePages, 1u);
    }
}

TEST(VulkanMemoryManagerTest, OversizedPagesPreserveSharedReserve)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();

    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible ? "host-visible" : "device-local");
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        for (bool LargeFirst : {false, true})
        {
            SCOPED_TRACE(LargeFirst);
            TrackingMemoryManager             Mgr{"Shared reserve test", pDevice->GetLogicalDevice(),
                                      pDevice->GetPhysicalDevice(), DefaultRawMemoryAllocator::GetAllocator(),
                                      PageSize, PageSize, 2 * PageSize, 2 * PageSize};
            VulkanUtilities::MemoryAllocation Large;
            if (LargeFirst)
                Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
            {
                auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
                ASSERT_TRUE(Small);
                if (!LargeFirst)
                    Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
                ASSERT_TRUE(Large);
            }
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 2u);
            {
                auto Released = std::move(Large);
            }
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 1u);
            const auto CreatedPages = Mgr.CreatedPages;
            {
                auto Small = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
                ASSERT_TRUE(Small);
                EXPECT_EQ(Mgr.CreatedPages, CreatedPages);
            }
            // The reserve remains a limit: excess empty shared pages are collected.
            {
                auto First  = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
                auto Second = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
                auto Third  = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
                ASSERT_TRUE(First);
                ASSERT_TRUE(Second);
                ASSERT_TRUE(Third);
                EXPECT_EQ(Mgr.LivePages, 3u);
            }
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 2u);
        }
    }
}

TEST(VulkanMemoryManagerTest, PreferSharedPages)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        for (bool LargeFirst : {false, true})
        {
            SCOPED_TRACE(LargeFirst);
            TrackingMemoryManager             Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                      DefaultRawMemoryAllocator::GetAllocator(), PageSize, PageSize, 2 * PageSize, 2 * PageSize};
            VulkanUtilities::MemoryAllocation Large;
            if (LargeFirst)
                Large = Mgr.Allocate(4 * PageSize, 256, Type, HostVisible, 0);
            auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Small);
            auto* SharedPage = Small.Page;
            if (!LargeFirst)
                Large = Mgr.Allocate(4 * PageSize, 256, Type, HostVisible, 0);
            {
                auto Released = std::move(Large);
            }
            auto OtherSmall = Mgr.Allocate(256, 256, Type, HostVisible, 0);
            ASSERT_TRUE(OtherSmall);
            EXPECT_EQ(OtherSmall.Page, SharedPage);
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 1u);
        }
    }
}

TEST(VulkanMemoryManagerTest, ReclaimUnusableOversizedPagesBeforeGrowth)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        TrackingMemoryManager Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                  DefaultRawMemoryAllocator::GetAllocator(), PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        {
            auto Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Large);
        }
        {
            auto Larger = Mgr.Allocate(2 * PageSize, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Larger);
            EXPECT_EQ(Mgr.LivePages, 1u);
            EXPECT_EQ(Mgr.PeakLivePages, 1u);
            EXPECT_EQ(Mgr.AllocatedSize(HostVisible), 2 * PageSize);
        }
        {
            auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Small);
            EXPECT_EQ(Small.Page->GetPageSize(), PageSize);
            EXPECT_EQ(Mgr.LivePages, 1u);
            EXPECT_EQ(Mgr.PeakLivePages, 1u);
        }
    }
}

TEST(VulkanMemoryManagerTest, FailedPageCreationPreservesAccounting)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        FailingMemoryAllocator Allocator;
        TrackingMemoryManager  Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                  Allocator, PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        Allocator.Fail = true;
        EXPECT_THROW(Mgr.Allocate(2 * PageSize, 256, Type, HostVisible, 0), std::bad_alloc);
        Allocator.Fail = false;
        EXPECT_EQ(Mgr.AllocatedSize(HostVisible), 0u);
        EXPECT_EQ(Mgr.PeakAllocatedSize(HostVisible), 0u);
        EXPECT_EQ(Mgr.LivePages, 0u);
        {
            auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Small);
        }
        Mgr.ShrinkMemory();
        EXPECT_EQ(Mgr.AllocatedSize(HostVisible), PageSize);
        EXPECT_EQ(Mgr.LivePages, 1u);
    }
}

TEST(VulkanMemoryManagerTest, ConcurrentReleaseAndCollection)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        TrackingMemoryManager Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                  DefaultRawMemoryAllocator::GetAllocator(), PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        std::atomic<bool>     Done{false};
        std::thread           Collector{[&] {
            while (!Done.load())
            {
                Mgr.ShrinkMemory();
                std::this_thread::yield();
            }
        }};
        bool                  Succeeded = true;
        try
        {
            for (size_t i = 0; i < 500; ++i)
            {
                auto Large = Mgr.Allocate(3 * PageSize / 2, 256, Type, HostVisible, 0);
                auto Small = Mgr.Allocate(256, 256, Type, HostVisible, 0);
                Succeeded  = Succeeded && Large && Small;
                if ((i % 8) == 0)
                    std::this_thread::yield();
            }
        }
        catch (...)
        {
            Succeeded = false;
        }
        Done.store(true);
        Collector.join();
        EXPECT_TRUE(Succeeded);
        Mgr.ShrinkMemory();
        EXPECT_EQ(Mgr.LivePages, 1u);
        EXPECT_EQ(Mgr.AllocatedSize(HostVisible), PageSize);
    }
}

TEST(VulkanMemoryManagerTest, BestFitOversizedReuse)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        TrackingMemoryManager Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                  DefaultRawMemoryAllocator::GetAllocator(), PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        for (bool LargerFirst : {false, true})
        {
            VulkanUtilities::MemoryAllocation First  = Mgr.Allocate((LargerFirst ? 4 : 2) * PageSize, 256, Type, HostVisible, 0);
            auto                              Second = Mgr.Allocate((LargerFirst ? 2 : 4) * PageSize, 256, Type, HostVisible, 0);
            ASSERT_TRUE(First);
            ASSERT_TRUE(Second);
            {
                auto ReleasedFirst  = std::move(First);
                auto ReleasedSecond = std::move(Second);
            }
            const auto CreatedPages = Mgr.CreatedPages;
            {
                auto Replacement = Mgr.Allocate(2 * PageSize, 256, Type, HostVisible, 0);
                ASSERT_TRUE(Replacement);
                EXPECT_EQ(Replacement.Page->GetPageSize(), 2 * PageSize);
                EXPECT_EQ(Mgr.CreatedPages, CreatedPages);
            }
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 0u);
        }
    }
}

TEST(VulkanMemoryManagerTest, RejectInvalidAllocationRequests)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();
    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible);
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);
        TrackingMemoryManager Mgr{"Safety test", pDevice->GetLogicalDevice(), pDevice->GetPhysicalDevice(),
                                  DefaultRawMemoryAllocator::GetAllocator(), PageSize, PageSize, 2 * PageSize, 2 * PageSize};
        const VkDeviceSize    MaxSize      = std::numeric_limits<VariableSizeAllocationsManager::OffsetType>::max();
        const VkDeviceSize    Sizes[]      = {0, 1, 1, MaxSize, MaxSize / 2 + 1};
        const VkDeviceSize    Alignments[] = {256, 0, 3, 256, 1};
        for (size_t i = 0; i < 5; ++i)
        {
            SCOPED_TRACE(i);
            TestingEnvironment::ErrorScope ExpectedError{"Invalid Vulkan memory allocation size or alignment"};
            EXPECT_THROW(Mgr.Allocate(Sizes[i], Alignments[i], Type, HostVisible, 0), std::runtime_error);
        }
        EXPECT_EQ(Mgr.CreatedPages, 0u);
        EXPECT_EQ(Mgr.AllocatedSize(HostVisible), 0u);
        {
            auto Valid = Mgr.Allocate(PageSize, 256, Type, HostVisible, 0);
            ASSERT_TRUE(Valid);
        }
    }
}

TEST(VulkanMemoryManagerTest, AllocationAlignment)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
        GTEST_SKIP();

    auto*                  pDevice  = static_cast<RenderDeviceVkImpl*>(pEnv->GetDevice());
    constexpr VkDeviceSize PageSize = 64 << 10;
    for (bool HostVisible : {false, true})
    {
        SCOPED_TRACE(HostVisible ? "host-visible" : "device-local");
        TrackingMemoryManager       Mgr{"Alignment test", pDevice->GetLogicalDevice(),
                                  pDevice->GetPhysicalDevice(), DefaultRawMemoryAllocator::GetAllocator(),
                                  PageSize, PageSize, 0, 0};
        const VkMemoryPropertyFlags Props = HostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t              Type  = pDevice->GetPhysicalDevice().GetMemoryTypeIndex(~0u, Props);
        ASSERT_NE(Type, VulkanUtilities::PhysicalDevice::InvalidMemoryTypeIndex);

        const VkDeviceSize Sizes[]      = {PageSize + 1, 1};
        const VkDeviceSize Alignments[] = {256, 2 * PageSize};
        for (size_t i = 0; i < 2; ++i)
        {
            SCOPED_TRACE(i);
            {
                auto Allocation = Mgr.Allocate(Sizes[i], Alignments[i], Type, HostVisible, 0);
                ASSERT_TRUE(Allocation);
                EXPECT_EQ(Allocation.UnalignedOffset, 0u);
                EXPECT_GE(Allocation.Size, AlignUp(Sizes[i], Alignments[i]));
                EXPECT_GE(Allocation.Page->GetPageSize(), Allocation.Size);
            }
            Mgr.ShrinkMemory();
            EXPECT_EQ(Mgr.LivePages, 0u);
        }
    }
}

} // namespace
} // namespace Testing
} // namespace Diligent
