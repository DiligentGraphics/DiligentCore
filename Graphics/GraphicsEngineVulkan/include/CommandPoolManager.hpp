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

#include <deque>
#include <mutex>
#include <atomic>
#include "STDAllocator.hpp"
#include "VulkanUtilities/ObjectWrappers.hpp"
#include "VulkanUtilities/LogicalDevice.hpp"

namespace Diligent
{

class CommandPoolManager
{
public:
    struct CreateInfo
    {
        const VulkanUtilities::LogicalDevice& LogicalDevice;
        std::string                           Name;
        const HardwareQueueIndex              queueFamilyIndex;
        const VkCommandPoolCreateFlags        flags;
    };

    CommandPoolManager(const CreateInfo& CI) noexcept;

    // clang-format off
    CommandPoolManager             (const CommandPoolManager&)  = delete;
    CommandPoolManager             (      CommandPoolManager&&) = delete;
    CommandPoolManager& operator = (const CommandPoolManager&)  = delete;
    CommandPoolManager& operator = (      CommandPoolManager&&) = delete;
    // clang-format on

    ~CommandPoolManager();

    // Allocates Vulkan command pool.
    VulkanUtilities::CommandPoolWrapper AllocateCommandPool(const char* DebugName = nullptr);

    void DestroyPools();

#ifdef DILIGENT_DEVELOPMENT
    int32_t GetAllocatedPoolCount() const
    {
        return m_AllocatedPoolCounter;
    }
#endif

    // Returns command pool to the list of available pools. The GPU must have finished using the pool
    void RecycleCommandPool(VulkanUtilities::CommandPoolWrapper&& CmdPool);

private:
    const VulkanUtilities::LogicalDevice& m_LogicalDevice;

    std::string                    m_Name;
    const HardwareQueueIndex       m_QueueFamilyIndex;
    const VkCommandPoolCreateFlags m_CmdPoolFlags;

    std::mutex                                                                                               m_Mutex;
    std::deque<VulkanUtilities::CommandPoolWrapper, STDAllocatorRawMem<VulkanUtilities::CommandPoolWrapper>> m_CmdPools;

#ifdef DILIGENT_DEVELOPMENT
    std::atomic<Int32> m_AllocatedPoolCounter{0};
#endif
};

} // namespace Diligent
