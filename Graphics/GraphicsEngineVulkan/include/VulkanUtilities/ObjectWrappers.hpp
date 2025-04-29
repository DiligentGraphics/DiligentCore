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

#include <memory>
#include "LogicalDevice.hpp"

namespace VulkanUtilities
{

// In 32-bit version, all Vulkan handles are typedefed as uint64_t, so we have to
// use another way to distinguish objects.
enum class VulkanHandleTypeId : uint32_t;

template <typename VulkanObjectType, VulkanHandleTypeId>
class ObjectWrapper
{
public:
    using VkObjectType = VulkanObjectType;

    ObjectWrapper() :
        m_Device{nullptr},
        m_VkObject{VK_NULL_HANDLE}
    {}

    ObjectWrapper(std::shared_ptr<const LogicalDevice> pDevice, VulkanObjectType&& vkObject) :
        m_Device{pDevice},
        m_VkObject{vkObject}
    {
        vkObject = VK_NULL_HANDLE;
    }
    // This constructor does not take ownership of the vulkan object
    explicit ObjectWrapper(VulkanObjectType vkObject) :
        m_VkObject{vkObject}
    {
    }

    ObjectWrapper(const ObjectWrapper&) = delete;
    ObjectWrapper& operator=(const ObjectWrapper&) = delete;

    ObjectWrapper(ObjectWrapper&& rhs) noexcept :
        m_Device{std::move(rhs.m_Device)},
        m_VkObject{rhs.m_VkObject}
    {
        rhs.m_VkObject = VK_NULL_HANDLE;
    }

    ObjectWrapper& operator=(ObjectWrapper&& rhs) noexcept
    {
        Release();
        m_Device       = std::move(rhs.m_Device);
        m_VkObject     = rhs.m_VkObject;
        rhs.m_VkObject = VK_NULL_HANDLE;
        return *this;
    }

    operator VulkanObjectType() const
    {
        return m_VkObject;
    }

    const VulkanObjectType* operator&() const
    {
        return &m_VkObject;
    }

    void Release()
    {
        // For externally managed objects, m_Device is null
        if (m_Device && m_VkObject != VK_NULL_HANDLE)
        {
            m_Device->ReleaseVulkanObject(std::move(*this));
        }
        m_VkObject = VK_NULL_HANDLE;
        m_Device.reset();
    }

    ~ObjectWrapper()
    {
        Release();
    }

private:
    friend class LogicalDevice;

    std::shared_ptr<const LogicalDevice> m_Device;
    VulkanObjectType                     m_VkObject;
};

} // namespace VulkanUtilities
