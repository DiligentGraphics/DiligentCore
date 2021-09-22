/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "DeviceMemoryVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "GraphicsAccessories.hpp"
#include "VulkanUtilities/VulkanDebug.hpp"

namespace Diligent
{

DeviceMemoryVkImpl::DeviceMemoryVkImpl(IReferenceCounters*           pRefCounters,
                                       RenderDeviceVkImpl*           pDeviceVk,
                                       const DeviceMemoryCreateInfo& MemCI) :
    TDeviceMemoryBase{pRefCounters, pDeviceVk, MemCI}
{
#define DEVMEM_CHECK_CREATE_INFO(...) \
    LOG_ERROR_AND_THROW("Device memory create info is not valid: ", __VA_ARGS__);

    const auto& PhysicalDevice = m_pDevice->GetPhysicalDevice();
    const auto& LogicalDevice  = m_pDevice->GetLogicalDevice();

    // AZ TODO: we can create temporary buffer and texture to detect memory type
    if (MemCI.NumResources == 0)
        DEVMEM_CHECK_CREATE_INFO("Vulkan requires at least one resource to choose memory type");

    if (MemCI.ppCompatibleResources == nullptr)
        DEVMEM_CHECK_CREATE_INFO("ppCompatibleResources must not be null");

    uint32_t MemoryTypeBits = ~0u;
    for (Uint32 i = 0; i < MemCI.NumResources; ++i)
    {
        auto* pResource = MemCI.ppCompatibleResources[i];

        if (RefCntAutoPtr<TextureVkImpl> pTexture{pResource, IID_TextureVk})
        {
            if (pTexture->GetDesc().Usage != USAGE_SPARSE)
                DEVMEM_CHECK_CREATE_INFO("ppCompatibleResources[", i, "] must be created with USAGE_SPARSE");

            MemoryTypeBits &= LogicalDevice.GetImageMemoryRequirements(pTexture->GetVkImage()).memoryTypeBits;
        }
        else if (RefCntAutoPtr<BufferVkImpl> pBuffer{pResource, IID_BufferVk})
        {
            if (pBuffer->GetDesc().Usage != USAGE_SPARSE)
                DEVMEM_CHECK_CREATE_INFO("ppCompatibleResources[", i, "] must be created with USAGE_SPARSE");

            MemoryTypeBits &= LogicalDevice.GetBufferMemoryRequirements(pBuffer->GetVkBuffer()).memoryTypeBits;
        }
        else
        {
            UNEXPECTED("unsupported resource type");
        }
    }

    if (MemoryTypeBits == 0)
        DEVMEM_CHECK_CREATE_INFO("ppCompatibleResources contains incompatible resources");

    static constexpr auto InvalidMemoryTypeIndex = VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex;

    uint32_t MemoryTypeIndex = PhysicalDevice.GetMemoryTypeIndex(MemoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (MemoryTypeIndex == InvalidMemoryTypeIndex)
        DEVMEM_CHECK_CREATE_INFO("Failed to find memory type for resources in ppCompatibleResources");

    m_MemoryTypeIndex = MemoryTypeIndex;

    VkMemoryAllocateInfo MemAlloc{};
    MemAlloc.pNext           = nullptr;
    MemAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize  = m_Desc.PageSize;
    MemAlloc.memoryTypeIndex = m_MemoryTypeIndex;

    const auto PageCount = StaticCast<size_t>(MemCI.InitialSize / m_Desc.PageSize);
    m_Pages.reserve(PageCount);

    for (size_t i = 0; i < PageCount; ++i)
        m_Pages.emplace_back(LogicalDevice.AllocateDeviceMemory(MemAlloc, m_Desc.Name)); // throw on error
}

DeviceMemoryVkImpl::~DeviceMemoryVkImpl()
{
    // AZ TODO: use release queue
}

IMPLEMENT_QUERY_INTERFACE(DeviceMemoryVkImpl, IID_DeviceMemoryVk, TDeviceMemoryBase)

Bool DeviceMemoryVkImpl::Resize(Uint64 NewSize)
{
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
    const auto  NewPageCount  = StaticCast<size_t>(NewSize / m_Desc.PageSize);
    const auto  OldPageCount  = m_Pages.size();

    VkMemoryAllocateInfo MemAlloc{};
    MemAlloc.pNext           = nullptr;
    MemAlloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize  = m_Desc.PageSize;
    MemAlloc.memoryTypeIndex = m_MemoryTypeIndex;

    m_Pages.reserve(NewPageCount);

    for (size_t i = OldPageCount; i < NewPageCount; ++i)
    {
        try
        {
            m_Pages.emplace_back(LogicalDevice.AllocateDeviceMemory(MemAlloc, m_Desc.Name)); // throw on error
        }
        catch (...)
        {
            return false;
        }
    }

    if (NewPageCount < OldPageCount)
    {
        // AZ TODO: use release queue
        m_Pages.resize(NewPageCount);
    }

    return true;
}

Uint64 DeviceMemoryVkImpl::GetCapacity()
{
    return m_Desc.PageSize * m_Pages.size();
}

Bool DeviceMemoryVkImpl::IsCompatible(IDeviceObject* pResource) const
{
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    if (RefCntAutoPtr<TextureVkImpl> pTexture{pResource, IID_TextureVk})
    {
        return (LogicalDevice.GetImageMemoryRequirements(pTexture->GetVkImage()).memoryTypeBits & (1u << m_MemoryTypeIndex)) != 0;
    }
    else if (RefCntAutoPtr<BufferVkImpl> pBuffer{pResource, IID_BufferVk})
    {
        return (LogicalDevice.GetBufferMemoryRequirements(pBuffer->GetVkBuffer()).memoryTypeBits & (1u << m_MemoryTypeIndex)) != 0;
    }
    else
    {
        UNEXPECTED("unsupported resource type");
        return false;
    }
}

DeviceMemoryRangeVk DeviceMemoryVkImpl::GetRange(Uint64 Offset, Uint64 Size) const
{
    const auto          PageIdx = static_cast<size_t>(Offset / m_Desc.PageSize);
    DeviceMemoryRangeVk Result{};

    if (PageIdx >= m_Pages.size())
    {
        LOG_ERROR_MESSAGE("DeviceMemoryVkImpl::GetRange(): Offset is greater than allocated space");
        return Result;
    }

    const auto OffsetInPage = Offset % m_Desc.PageSize;
    if (OffsetInPage + Size > m_Desc.PageSize)
    {
        LOG_ERROR_MESSAGE("DeviceMemoryVkImpl::GetRange(): Offset and Size must be inside single page");
        return Result;
    }

    Result.Offset = OffsetInPage;
    Result.Handle = m_Pages[PageIdx];
    Result.Size   = std::min(m_Desc.PageSize - OffsetInPage, Size);

    return Result;
}

} // namespace Diligent
