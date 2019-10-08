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
#include <sstream>

#include "VulkanUtilities/VulkanFencePool.h"
#include "Errors.h"
#include "DebugUtilities.h"

namespace VulkanUtilities
{
    VulkanFencePool::VulkanFencePool(std::shared_ptr<const VulkanLogicalDevice> LogicalDevice)noexcept :
        m_LogicalDevice{std::move(LogicalDevice)}
    {}

    VulkanFencePool::~VulkanFencePool()
    {
#ifdef DEVELOPMENT
        for (const auto& fence : m_Fences)
        {
            DEV_CHECK_ERR(m_LogicalDevice->GetFenceStatus(fence) == VK_SUCCESS, "Destroying a fence that has not been signaled");
        }
#endif
        m_Fences.clear();
    }

    FenceWrapper VulkanFencePool::GetFence()
    {
        FenceWrapper Fence;
        if(!m_Fences.empty())
        {
            Fence = std::move(m_Fences.back());
            m_LogicalDevice->ResetFence(Fence);
            m_Fences.pop_back();
        }
        else
        {
            VkFenceCreateInfo FenceCI = {};
            FenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            FenceCI.pNext = nullptr;
            FenceCI.flags = 0; // Available flag: VK_FENCE_CREATE_SIGNALED_BIT
            Fence = m_LogicalDevice->CreateFence(FenceCI);
        }
        return Fence;
    }

    void VulkanFencePool::DisposeFence(FenceWrapper&& Fence)
    {
        DEV_CHECK_ERR(m_LogicalDevice->GetFenceStatus(Fence) == VK_SUCCESS, "Disposing a fence that has not been signaled");
        m_Fences.emplace_back(std::move(Fence));
    }
}
