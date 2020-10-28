/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
#include "BottomLevelASVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

BottomLevelASVkImpl::BottomLevelASVkImpl(IReferenceCounters*      pRefCounters,
                                         RenderDeviceVkImpl*      pRenderDeviceVk,
                                         const BottomLevelASDesc& Desc,
                                         bool                     bIsDeviceInternal) :
    TBottomLevelASBase{pRefCounters, pRenderDeviceVk, Desc, bIsDeviceInternal}
{
    const auto& LogicalDevice  = pRenderDeviceVk->GetLogicalDevice();
    const auto& PhysicalDevice = pRenderDeviceVk->GetPhysicalDevice();
    const auto& Limits         = PhysicalDevice.GetExtProperties().RayTracing;

    VkAccelerationStructureCreateInfoKHR                          CreateInfo = {};
    std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> Geometries;

    CreateInfo.sType            = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    CreateInfo.type             = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    CreateInfo.flags            = BuildASFlagsToVkBuildAccelerationStructureFlags(m_Desc.Flags);
    CreateInfo.maxGeometryCount = std::max(m_Desc.BoxCount, m_Desc.TriangleCount);
    CreateInfo.compactedSize    = 0; // AZ TODO

    VERIFY_EXPR(CreateInfo.maxGeometryCount <= Limits.maxGeometryCount);

    Geometries.resize(CreateInfo.maxGeometryCount);
    CreateInfo.pGeometryInfos = Geometries.data();

    // Specs says: the geometryType member of each geometry in pGeometries must be the same.
    if (m_Desc.pTriangles != nullptr)
    {
        Uint32 MaxPrimitiveCount = 0;
        for (uint32_t i = 0; i < m_Desc.TriangleCount; ++i)
        {
            auto& src = m_Desc.pTriangles[i];
            auto& dst = Geometries[i];

            dst.sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
            dst.pNext             = nullptr;
            dst.geometryType      = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            dst.maxPrimitiveCount = (src.IndexType == VT_UNDEFINED ? src.MaxVertexCount : src.MaxIndexCount) / 3;
            dst.indexType         = TypeToVkIndexType(src.IndexType);
            dst.maxVertexCount    = src.MaxVertexCount;
            dst.vertexFormat      = TypeToVkFormat(src.VertexValueType, src.VertexComponentCount, src.VertexValueType < VT_FLOAT16);
            dst.allowsTransforms  = src.AllowsTransforms;

            MaxPrimitiveCount += dst.maxPrimitiveCount;
        }
        VERIFY_EXPR(MaxPrimitiveCount <= Limits.maxPrimitiveCount);
    }
    else if (m_Desc.pBoxes != nullptr)
    {
        Uint32 MaxBoxCount = 0;
        for (uint32_t i = 0; i < m_Desc.BoxCount; ++i)
        {
            auto& src = m_Desc.pBoxes[i];
            auto& dst = Geometries[i];

            dst.sType             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
            dst.pNext             = nullptr;
            dst.geometryType      = VK_GEOMETRY_TYPE_AABBS_KHR;
            dst.maxPrimitiveCount = src.MaxBoxCount;
            dst.indexType         = VK_INDEX_TYPE_NONE_KHR;
            dst.maxVertexCount    = 0;
            dst.vertexFormat      = VK_FORMAT_UNDEFINED;
            dst.allowsTransforms  = VK_FALSE;

            MaxBoxCount += dst.maxPrimitiveCount;
        }
        VERIFY_EXPR(MaxBoxCount <= Limits.maxPrimitiveCount);
    }
    else
    {
        UNEXPECTED("Either pTriangles or pBoxes must not be null");
    }

    m_VulkanBLAS = LogicalDevice.CreateAccelStruct(CreateInfo, m_Desc.Name);

    VkAccelerationStructureMemoryRequirementsInfoKHR MemInfo = {};
    VkMemoryRequirements                             MemReqs = {};

    MemInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    MemInfo.accelerationStructure = m_VulkanBLAS;
    MemInfo.buildType             = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
    MemInfo.type                  = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;

    MemReqs = LogicalDevice.GetASMemoryRequirements(MemInfo);

    uint32_t MemoryTypeIndex = PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (MemoryTypeIndex == VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex)
        LOG_ERROR_AND_THROW("Failed to find suitable memory type for BLAS '", m_Desc.Name, '\'');

    VERIFY(IsPowerOfTwo(MemReqs.alignment), "Alignment is not power of 2!");
    m_MemoryAllocation    = pRenderDeviceVk->AllocateMemory(MemReqs.size, MemReqs.alignment, MemoryTypeIndex);
    m_MemoryAlignedOffset = Align(VkDeviceSize{m_MemoryAllocation.UnalignedOffset}, MemReqs.alignment);
    VERIFY(m_MemoryAllocation.Size >= MemReqs.size + (m_MemoryAlignedOffset - m_MemoryAllocation.UnalignedOffset), "Size of memory allocation is too small");

    auto Memory = m_MemoryAllocation.Page->GetVkMemory();
    auto err    = LogicalDevice.BindASMemory(m_VulkanBLAS, Memory, m_MemoryAlignedOffset);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to bind AS memory");

    m_DeviceAddress = LogicalDevice.GetAccelerationStructureDeviceAddress(m_VulkanBLAS);

    MemInfo.type        = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
    MemReqs             = LogicalDevice.GetASMemoryRequirements(MemInfo);
    m_ScratchSize.Build = static_cast<Uint32>(MemReqs.size);

    MemInfo.type         = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_KHR;
    MemReqs              = LogicalDevice.GetASMemoryRequirements(MemInfo);
    m_ScratchSize.Update = static_cast<Uint32>(MemReqs.size);
}

BottomLevelASVkImpl::~BottomLevelASVkImpl()
{
    // Vk object can only be destroyed when it is no longer used by the GPU
    if (m_VulkanBLAS != VK_NULL_HANDLE)
        m_pDevice->SafeReleaseDeviceObject(std::move(m_VulkanBLAS), m_Desc.CommandQueueMask);
    if (m_MemoryAllocation.Page != nullptr)
        m_pDevice->SafeReleaseDeviceObject(std::move(m_MemoryAllocation), m_Desc.CommandQueueMask);
}

} // namespace Diligent
