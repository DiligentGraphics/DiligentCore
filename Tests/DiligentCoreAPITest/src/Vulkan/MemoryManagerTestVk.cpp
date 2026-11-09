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
    size_t LivePages = 0;

private:
    void OnNewPageCreated(VulkanUtilities::MemoryPage&) override { ++LivePages; }
    void OnPageDestroy(VulkanUtilities::MemoryPage&) override { --LivePages; }
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

} // namespace
} // namespace Testing
} // namespace Diligent
