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

#include <limits>
#include "VulkanErrors.hpp"
#include "VulkanUtilities/VulkanLogicalDevice.hpp"
#include "VulkanUtilities/VulkanDebug.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"

// for KHR ray tracing emulation
#include <mutex>
#include <unordered_map>

namespace VulkanUtilities
{

// KHR ray tracing emulation.
// Will be deprecated after the release of KHR extension.
#if DILIGENT_USE_VOLK
static_assert(sizeof(VkAccelerationStructureKHR) == sizeof(VkAccelerationStructureNV), "not compatible with NV extension");
static_assert(sizeof(VkDeviceAddress) == 8, "not compatible with NV extension");

static std::mutex                                    g_BufferDeviceAddressGuard;
static std::unordered_map<VkDeviceAddress, VkBuffer> g_DeviceAddressToBuffer;
static std::unordered_map<VkBuffer, VkDeviceAddress> g_BufferToDeviceAddress;
static uint32_t                                      g_BufferDeviceAddressCounter = 0;
static const VkDeviceAddress                         g_BufferMask                 = 0xFFFFFFFF00000000ull;

static PFN_vkCreateBuffer              Origin_vkCreateBuffer              = nullptr;
static PFN_vkDestroyBuffer             Origin_vkDestroyBuffer             = nullptr;
static PFN_vkGetBufferDeviceAddressKHR Origin_vkGetBufferDeviceAddressKHR = nullptr;

static VkResult VKAPI_CALL Wrap_vkCreateBuffer(VkDevice                     device,
                                               const VkBufferCreateInfo*    pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkBuffer*                    pBuffer)
{
    const_cast<VkBufferCreateInfo*>(pCreateInfo)->usage &= ~VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return Origin_vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

static void VKAPI_CALL Wrap_vkDestroyBuffer(VkDevice                     device,
                                            VkBuffer                     buffer,
                                            const VkAllocationCallbacks* pAllocator)
{
    Origin_vkDestroyBuffer(device, buffer, pAllocator);

    std::unique_lock<std::mutex> lock{g_BufferDeviceAddressGuard};

    auto iter = g_BufferToDeviceAddress.find(buffer);
    if (iter != g_BufferToDeviceAddress.end())
    {
        g_DeviceAddressToBuffer.erase(iter->second);
        g_BufferToDeviceAddress.erase(iter);
    }
}

static VkDeviceAddress VKAPI_CALL Wrap_vkGetBufferDeviceAddressKHR(VkDevice                         device,
                                                                   const VkBufferDeviceAddressInfo* pInfo)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);

    std::unique_lock<std::mutex> lock{g_BufferDeviceAddressGuard};

    // find in existing buffers
    auto iter = g_BufferToDeviceAddress.find(pInfo->buffer);
    if (iter != g_BufferToDeviceAddress.end())
        return iter->second;

    // create new device address
    VkDeviceAddress Addr = VkDeviceAddress(++g_BufferDeviceAddressCounter) << 32;
    g_BufferToDeviceAddress.insert_or_assign(pInfo->buffer, Addr);
    g_DeviceAddressToBuffer.insert_or_assign(Addr, pInfo->buffer);
    return Addr;
}

struct BufferAndOffset
{
    VkBuffer     Buffer;
    VkDeviceSize Offset;
};
static BufferAndOffset DeviceAddressToBuffer(VkDeviceAddress Addr)
{
    if (Addr == 0)
        return {VK_NULL_HANDLE, 0};

    std::unique_lock<std::mutex> lock{g_BufferDeviceAddressGuard};

    auto iter = g_DeviceAddressToBuffer.find(Addr & g_BufferMask);
    if (iter == g_DeviceAddressToBuffer.end())
    {
        VERIFY(false, "Failed to map device address to buffer");
        return {VK_NULL_HANDLE, 0};
    }

    return {iter->second, Addr & ~g_BufferMask};
}

static BufferAndOffset DeviceAddressToBuffer(const VkDeviceOrHostAddressConstKHR& Addr)
{
    return DeviceAddressToBuffer(Addr.deviceAddress);
}

static BufferAndOffset DeviceAddressToBuffer(const VkDeviceOrHostAddressKHR& Addr)
{
    return DeviceAddressToBuffer(Addr.deviceAddress);
}


static VkResult VKAPI_CALL Redirect_vkCreateAccelerationStructureKHR(VkDevice                                    device,
                                                                     const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
                                                                     const VkAllocationCallbacks*                pAllocator,
                                                                     VkAccelerationStructureKHR*                 pAccelerationStructure)
{
    VERIFY_EXPR(pCreateInfo->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
    VERIFY_EXPR(pCreateInfo->pNext == nullptr);
    VERIFY_EXPR(pCreateInfo->deviceAddress == 0);

    VkAccelerationStructureCreateInfoNV CreateInfo = {};
    std::vector<VkGeometryNV>           Geometries;

    CreateInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    CreateInfo.compactedSize = pCreateInfo->compactedSize;
    CreateInfo.info.sType    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    CreateInfo.info.type     = pCreateInfo->type;
    CreateInfo.info.flags    = pCreateInfo->flags;

    if (CreateInfo.info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
    {
        CreateInfo.info.instanceCount = pCreateInfo->maxGeometryCount;
    }
    else if (CreateInfo.info.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
    {
        Geometries.resize(pCreateInfo->maxGeometryCount);

        for (uint32_t i = 0; i < pCreateInfo->maxGeometryCount; ++i)
        {
            auto& src = pCreateInfo->pGeometryInfos[i];
            auto& dst = Geometries[i];

            VERIFY_EXPR(src.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR);
            VERIFY_EXPR(src.pNext == nullptr);

            dst.sType        = VK_STRUCTURE_TYPE_GEOMETRY_NV;
            dst.pNext        = nullptr;
            dst.geometryType = src.geometryType;
            dst.flags        = 0;

            dst.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
            dst.geometry.triangles.pNext = nullptr;

            dst.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
            dst.geometry.aabbs.pNext = nullptr;

            if (dst.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
            {
                dst.geometry.triangles.vertexData      = VK_NULL_HANDLE;
                dst.geometry.triangles.vertexOffset    = 0;
                dst.geometry.triangles.vertexCount     = src.maxVertexCount;
                dst.geometry.triangles.vertexStride    = 0;
                dst.geometry.triangles.vertexFormat    = src.vertexFormat;
                dst.geometry.triangles.indexData       = VK_NULL_HANDLE;
                dst.geometry.triangles.indexOffset     = 0;
                dst.geometry.triangles.indexCount      = 0;
                dst.geometry.triangles.indexType       = src.indexType;
                dst.geometry.triangles.transformData   = VK_NULL_HANDLE;
                dst.geometry.triangles.transformOffset = 0;

                if (dst.geometry.triangles.indexType == VK_INDEX_TYPE_NONE_KHR)
                {
                    VERIFY_EXPR(src.maxVertexCount == src.maxPrimitiveCount * 3);
                    dst.geometry.triangles.vertexCount = src.maxPrimitiveCount * 3;
                }
                else
                {
                    dst.geometry.triangles.indexCount = src.maxPrimitiveCount * 3;
                }
            }
            else if (dst.geometryType == VK_GEOMETRY_TYPE_AABBS_KHR)
            {
                dst.geometry.aabbs.aabbData = VK_NULL_HANDLE;
                dst.geometry.aabbs.numAABBs = src.maxPrimitiveCount;
                dst.geometry.aabbs.stride   = 0;
                dst.geometry.aabbs.offset   = 0;
            }
        }

        CreateInfo.info.geometryCount = static_cast<uint32_t>(Geometries.size());
        CreateInfo.info.pGeometries   = Geometries.data();
    }
    else
    {
        VERIFY(false, "unknown AS type");
        return VK_RESULT_MAX_ENUM;
    }

    return vkCreateAccelerationStructureNV(device, &CreateInfo, pAllocator, reinterpret_cast<VkAccelerationStructureNV*>(pAccelerationStructure));
}

static void VKAPI_CALL Redirect_vkGetAccelerationStructureMemoryRequirementsKHR(VkDevice                                                device,
                                                                                const VkAccelerationStructureMemoryRequirementsInfoKHR* pInfo,
                                                                                VkMemoryRequirements2*                                  pMemoryRequirements)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);
    VERIFY_EXPR(pInfo->buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);

    VkAccelerationStructureMemoryRequirementsInfoNV Info = {};

    Info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    Info.type                  = pInfo->type;
    Info.accelerationStructure = pInfo->accelerationStructure;

    return vkGetAccelerationStructureMemoryRequirementsNV(device, &Info, pMemoryRequirements);
}

static VkResult VKAPI_CALL Redirect_vkBindAccelerationStructureMemoryKHR(VkDevice                                        device,
                                                                         uint32_t                                        bindInfoCount,
                                                                         const VkBindAccelerationStructureMemoryInfoKHR* pBindInfos)
{
    VERIFY_EXPR(pBindInfos->sType == VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV);
    return vkBindAccelerationStructureMemoryNV(device, bindInfoCount, pBindInfos);
}

static VkDeviceAddress VKAPI_CALL Redirect_vkGetAccelerationStructureDeviceAddressKHR(VkDevice                                           device,
                                                                                      const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);

    VkDeviceAddress result = 0;
    vkGetAccelerationStructureHandleNV(device, pInfo->accelerationStructure, sizeof(result), &result);
    return result;
}

static void VKAPI_CALL Redirect_vkCmdBuildAccelerationStructureKHR(VkCommandBuffer                                         commandBuffer,
                                                                   uint32_t                                                infoCount,
                                                                   const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
                                                                   const VkAccelerationStructureBuildOffsetInfoKHR* const* ppOffsetInfos)
{
    std::vector<VkGeometryNV> Geometries;

    for (uint32_t i = 0; i < infoCount; ++i)
    {
        auto& SrcInfo   = pInfos[i];
        auto& SrcOffset = ppOffsetInfos[i];

        VERIFY_EXPR(SrcInfo.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
        VERIFY_EXPR(SrcInfo.pNext == nullptr);

        BufferAndOffset Scratch = DeviceAddressToBuffer(SrcInfo.scratchData);

        VkAccelerationStructureInfoNV Info = {};

        Info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        Info.pNext = nullptr;
        Info.type  = SrcInfo.type;
        Info.flags = SrcInfo.flags;

        if (Info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
        {
            VERIFY_EXPR(SrcInfo.geometryCount == 1);
            VERIFY_EXPR(SrcInfo.geometryArrayOfPointers == VK_FALSE);
            VERIFY_EXPR((*SrcInfo.ppGeometries)[0].geometry.instances.arrayOfPointers == VK_FALSE);

            Info.instanceCount = SrcOffset->primitiveCount;

            BufferAndOffset Instance = DeviceAddressToBuffer((*SrcInfo.ppGeometries)[0].geometry.instances.data);

            vkCmdBuildAccelerationStructureNV(commandBuffer, &Info,
                                              Instance.Buffer, Instance.Offset + SrcOffset[0].primitiveOffset,
                                              SrcInfo.update,
                                              SrcInfo.dstAccelerationStructure, SrcInfo.srcAccelerationStructure,
                                              Scratch.Buffer, Scratch.Offset);
        }
        else if (Info.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
        {
            VERIFY_EXPR(SrcInfo.geometryArrayOfPointers == VK_FALSE);

            Geometries.resize(SrcInfo.geometryCount);

            for (uint32_t j = 0; j < SrcInfo.geometryCount; ++j)
            {
                auto& src = (*SrcInfo.ppGeometries)[j];
                auto& dst = Geometries[j];
                auto& off = SrcOffset[j];

                VERIFY_EXPR(src.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
                VERIFY_EXPR(src.pNext == nullptr);
                VERIFY_EXPR(src.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR || src.geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);

                dst.sType        = VK_STRUCTURE_TYPE_GEOMETRY_NV;
                dst.pNext        = nullptr;
                dst.flags        = src.flags;
                dst.geometryType = src.geometryType;

                if (dst.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
                {
                    VERIFY_EXPR(src.geometry.triangles.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR);
                    VERIFY_EXPR(src.geometry.triangles.pNext == nullptr);
                    VERIFY_EXPR(off.firstVertex == 0);

                    BufferAndOffset VB = DeviceAddressToBuffer(src.geometry.triangles.vertexData);
                    BufferAndOffset IB = DeviceAddressToBuffer(src.geometry.triangles.indexData);
                    BufferAndOffset TB = DeviceAddressToBuffer(src.geometry.triangles.transformData);

                    dst.geometry.triangles.sType           = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
                    dst.geometry.triangles.pNext           = nullptr;
                    dst.geometry.triangles.vertexData      = VB.Buffer;
                    dst.geometry.triangles.vertexOffset    = VB.Offset;
                    dst.geometry.triangles.vertexCount     = 0;
                    dst.geometry.triangles.vertexStride    = src.geometry.triangles.vertexStride;
                    dst.geometry.triangles.vertexFormat    = src.geometry.triangles.vertexFormat;
                    dst.geometry.triangles.indexData       = IB.Buffer;
                    dst.geometry.triangles.indexOffset     = IB.Offset;
                    dst.geometry.triangles.indexCount      = 0;
                    dst.geometry.triangles.indexType       = src.geometry.triangles.indexType;
                    dst.geometry.triangles.transformData   = TB.Buffer;
                    dst.geometry.triangles.transformOffset = TB.Offset + off.transformOffset;

                    if (dst.geometry.triangles.indexType == VK_INDEX_TYPE_NONE_KHR)
                    {
                        dst.geometry.triangles.vertexOffset += off.primitiveOffset;
                        dst.geometry.triangles.vertexCount = off.primitiveCount * 3;
                    }
                    else
                    {
                        dst.geometry.triangles.indexOffset += off.primitiveOffset;
                        dst.geometry.triangles.indexCount = off.primitiveCount * 3;
                    }
                }
                else
                {
                    VERIFY_EXPR(src.geometry.aabbs.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR);
                    VERIFY_EXPR(src.geometry.aabbs.pNext == nullptr);
                    VERIFY_EXPR(src.geometry.aabbs.stride <= std::numeric_limits<uint32_t>::max());

                    BufferAndOffset Data = DeviceAddressToBuffer(src.geometry.aabbs.data);

                    dst.geometry.aabbs.sType    = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
                    dst.geometry.aabbs.pNext    = nullptr;
                    dst.geometry.aabbs.aabbData = Data.Buffer;
                    dst.geometry.aabbs.numAABBs = off.primitiveCount;
                    dst.geometry.aabbs.stride   = static_cast<uint32_t>(src.geometry.aabbs.stride);
                    dst.geometry.aabbs.offset   = Data.Offset + off.primitiveOffset;
                }
            }

            Info.geometryCount = static_cast<uint32_t>(Geometries.size());
            Info.pGeometries   = Geometries.data();

            vkCmdBuildAccelerationStructureNV(commandBuffer, &Info,
                                              VK_NULL_HANDLE, 0,
                                              SrcInfo.update,
                                              SrcInfo.dstAccelerationStructure, SrcInfo.srcAccelerationStructure,
                                              Scratch.Buffer, Scratch.Offset);
        }
        else
        {
            VERIFY(false, "unknown AS type");
        }
    }
}

static void VKAPI_CALL Redirect_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer                           commandBuffer,
                                                                  const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);

    vkCmdCopyAccelerationStructureNV(commandBuffer, pInfo->dst, pInfo->src, pInfo->mode);
}

static void VKAPI_CALL Redirect_vkCmdTraceRaysKHR(VkCommandBuffer                 commandBuffer,
                                                  const VkStridedBufferRegionKHR* pRaygenShaderBindingTable,
                                                  const VkStridedBufferRegionKHR* pMissShaderBindingTable,
                                                  const VkStridedBufferRegionKHR* pHitShaderBindingTable,
                                                  const VkStridedBufferRegionKHR* pCallableShaderBindingTable,
                                                  uint32_t                        width,
                                                  uint32_t                        height,
                                                  uint32_t                        depth)
{
    vkCmdTraceRaysNV(commandBuffer,
                     pRaygenShaderBindingTable->buffer, pRaygenShaderBindingTable->offset,
                     pMissShaderBindingTable->buffer, pMissShaderBindingTable->offset, pMissShaderBindingTable->stride,
                     pHitShaderBindingTable->buffer, pHitShaderBindingTable->offset, pHitShaderBindingTable->stride,
                     pCallableShaderBindingTable->buffer, pCallableShaderBindingTable->offset, pCallableShaderBindingTable->stride,
                     width, height, depth);
}

static VkResult VKAPI_CALL Redirect_vkGetRayTracingShaderGroupHandlesKHR(VkDevice   device,
                                                                         VkPipeline pipeline,
                                                                         uint32_t   firstGroup,
                                                                         uint32_t   groupCount,
                                                                         size_t     dataSize,
                                                                         void*      pData)
{
    return vkGetRayTracingShaderGroupHandlesNV(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

static VkResult VKAPI_CALL Redirect_vkCreateRayTracingPipelinesKHR(VkDevice                                 device,
                                                                   VkPipelineCache                          pipelineCache,
                                                                   uint32_t                                 createInfoCount,
                                                                   const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
                                                                   const VkAllocationCallbacks*             pAllocator,
                                                                   VkPipeline*                              pPipelines)
{
    std::vector<VkRayTracingPipelineCreateInfoNV>    Infos;
    std::vector<VkRayTracingShaderGroupCreateInfoNV> Groups;
    Infos.resize(createInfoCount);

    size_t GroupCount = 0;
    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        GroupCount += pCreateInfos[i].groupCount;
    }
    Groups.resize(GroupCount);
    GroupCount = 0;

    for (uint32_t i = 0; i < createInfoCount; ++i)
    {
        auto& src = pCreateInfos[i];
        auto& dst = Infos[i];

        VERIFY_EXPR(src.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);
        VERIFY_EXPR(src.pNext == nullptr);
        VERIFY_EXPR(src.libraries.libraryCount == 0);
        VERIFY_EXPR(src.libraries.pLibraries == nullptr);
        VERIFY_EXPR(src.pLibraryInterface == nullptr);

        // copy groups
        for (uint32_t j = 0; j < src.groupCount; ++j)
        {
            auto& srcg = src.pGroups[j];
            auto& dstg = Groups[GroupCount + j];

            VERIFY_EXPR(srcg.sType == VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
            VERIFY_EXPR(srcg.pNext == nullptr);

            dstg.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
            dstg.pNext              = nullptr;
            dstg.type               = srcg.type;
            dstg.generalShader      = srcg.generalShader;
            dstg.closestHitShader   = srcg.closestHitShader;
            dstg.anyHitShader       = srcg.anyHitShader;
            dstg.intersectionShader = srcg.intersectionShader;

            VERIFY_EXPR(srcg.pNext == nullptr);
        }

        dst.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
        dst.pNext              = nullptr;
        dst.flags              = src.flags;
        dst.stageCount         = src.stageCount;
        dst.pStages            = src.pStages;
        dst.groupCount         = src.groupCount;
        dst.pGroups            = Groups.data() + GroupCount;
        dst.maxRecursionDepth  = src.maxRecursionDepth;
        dst.layout             = src.layout;
        dst.basePipelineHandle = src.basePipelineHandle;
        dst.basePipelineIndex  = src.basePipelineIndex;

        GroupCount += src.groupCount;
    }

    return vkCreateRayTracingPipelinesNV(device, pipelineCache, createInfoCount, Infos.data(), pAllocator, pPipelines);
}

static void OverrideRayTracingKHRbyNV()
{
    vkCreateAccelerationStructureKHR                = &Redirect_vkCreateAccelerationStructureKHR;
    vkGetAccelerationStructureMemoryRequirementsKHR = &Redirect_vkGetAccelerationStructureMemoryRequirementsKHR;
    vkBindAccelerationStructureMemoryKHR            = &Redirect_vkBindAccelerationStructureMemoryKHR;
    vkGetAccelerationStructureDeviceAddressKHR      = &Redirect_vkGetAccelerationStructureDeviceAddressKHR;
    vkCmdBuildAccelerationStructureKHR              = &Redirect_vkCmdBuildAccelerationStructureKHR;
    vkCmdCopyAccelerationStructureKHR               = &Redirect_vkCmdCopyAccelerationStructureKHR;
    vkGetRayTracingShaderGroupHandlesKHR            = &Redirect_vkGetRayTracingShaderGroupHandlesKHR;
    vkCreateRayTracingPipelinesKHR                  = &Redirect_vkCreateRayTracingPipelinesKHR;
    vkCmdTraceRaysKHR                               = &Redirect_vkCmdTraceRaysKHR;

    Origin_vkGetBufferDeviceAddressKHR = vkGetBufferDeviceAddressKHR;
    Origin_vkCreateBuffer              = vkCreateBuffer;
    Origin_vkDestroyBuffer             = vkDestroyBuffer;
    vkCreateBuffer                     = &Wrap_vkCreateBuffer;
    vkDestroyBuffer                    = &Wrap_vkDestroyBuffer;
    vkGetBufferDeviceAddressKHR        = &Wrap_vkGetBufferDeviceAddressKHR;
    vkGetBufferDeviceAddress           = &Wrap_vkGetBufferDeviceAddressKHR;
    vkGetBufferDeviceAddressEXT        = &Wrap_vkGetBufferDeviceAddressKHR;
}
#endif // DILIGENT_USE_VOLK


std::shared_ptr<VulkanLogicalDevice> VulkanLogicalDevice::Create(const VulkanPhysicalDevice&  PhysicalDevice,
                                                                 const VkDeviceCreateInfo&    DeviceCI,
                                                                 const VkAllocationCallbacks* vkAllocator)
{
    auto* LogicalDevice = new VulkanLogicalDevice{PhysicalDevice, DeviceCI, vkAllocator};
    return std::shared_ptr<VulkanLogicalDevice>{LogicalDevice};
}

VulkanLogicalDevice::~VulkanLogicalDevice()
{
    vkDestroyDevice(m_VkDevice, m_VkAllocator);
}

VulkanLogicalDevice::VulkanLogicalDevice(const VulkanPhysicalDevice&  PhysicalDevice,
                                         const VkDeviceCreateInfo&    DeviceCI,
                                         const VkAllocationCallbacks* vkAllocator) :
    m_VkAllocator{vkAllocator},
    m_EnabledFeatures{*DeviceCI.pEnabledFeatures}
{
    auto res = vkCreateDevice(PhysicalDevice.GetVkDeviceHandle(), &DeviceCI, vkAllocator, &m_VkDevice);
    CHECK_VK_ERROR_AND_THROW(res, "Failed to create logical device");

#if DILIGENT_USE_VOLK
    // Since we only use one device at this time, load device function entries
    // https://github.com/zeux/volk#optimizing-device-calls
    volkLoadDevice(m_VkDevice);

    if (PhysicalDevice.GetExtFeatures().RayTracingNV)
        OverrideRayTracingKHRbyNV();
#endif

    m_EnabledGraphicsShaderStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (DeviceCI.pEnabledFeatures->geometryShader)
        m_EnabledGraphicsShaderStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    if (DeviceCI.pEnabledFeatures->tessellationShader)
        m_EnabledGraphicsShaderStages |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
}

VkQueue VulkanLogicalDevice::GetQueue(uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    VkQueue vkQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_VkDevice,
                     queueFamilyIndex, // Index of the queue family to which the queue belongs
                     0,                // Index within this queue family of the queue to retrieve
                     &vkQueue);
    VERIFY_EXPR(vkQueue != VK_NULL_HANDLE);
    return vkQueue;
}

void VulkanLogicalDevice::WaitIdle() const
{
    auto err = vkDeviceWaitIdle(m_VkDevice);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to idle device");
    (void)err;
}

template <typename VkObjectType,
          VulkanHandleTypeId VkTypeId,
          typename VkCreateObjectFuncType,
          typename VkObjectCreateInfoType>
VulkanObjectWrapper<VkObjectType, VkTypeId> VulkanLogicalDevice::CreateVulkanObject(VkCreateObjectFuncType        VkCreateObject,
                                                                                    const VkObjectCreateInfoType& CreateInfo,
                                                                                    const char*                   DebugName,
                                                                                    const char*                   ObjectType) const
{
    if (DebugName == nullptr)
        DebugName = "";

    VkObjectType VkObject = VK_NULL_HANDLE;

    auto err = VkCreateObject(m_VkDevice, &CreateInfo, m_VkAllocator, &VkObject);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create Vulkan ", ObjectType, " '", DebugName, '\'');

    if (*DebugName != 0)
        SetVulkanObjectName<VkObjectType, VkTypeId>(m_VkDevice, VkObject, DebugName);

    return VulkanObjectWrapper<VkObjectType, VkTypeId>{GetSharedPtr(), std::move(VkObject)};
}

CommandPoolWrapper VulkanLogicalDevice::CreateCommandPool(const VkCommandPoolCreateInfo& CmdPoolCI,
                                                          const char*                    DebugName) const
{
    VERIFY_EXPR(CmdPoolCI.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    return CreateVulkanObject<VkCommandPool, VulkanHandleTypeId::CommandPool>(vkCreateCommandPool, CmdPoolCI, DebugName, "command pool");
}

BufferWrapper VulkanLogicalDevice::CreateBuffer(const VkBufferCreateInfo& BufferCI,
                                                const char*               DebugName) const
{
    VERIFY_EXPR(BufferCI.sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    return CreateVulkanObject<VkBuffer, VulkanHandleTypeId::Buffer>(vkCreateBuffer, BufferCI, DebugName, "buffer");
}

BufferViewWrapper VulkanLogicalDevice::CreateBufferView(const VkBufferViewCreateInfo& BuffViewCI,
                                                        const char*                   DebugName) const
{
    VERIFY_EXPR(BuffViewCI.sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
    return CreateVulkanObject<VkBufferView, VulkanHandleTypeId::BufferView>(vkCreateBufferView, BuffViewCI, DebugName, "buffer view");
}

ImageWrapper VulkanLogicalDevice::CreateImage(const VkImageCreateInfo& ImageCI,
                                              const char*              DebugName) const
{
    VERIFY_EXPR(ImageCI.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    return CreateVulkanObject<VkImage, VulkanHandleTypeId::Image>(vkCreateImage, ImageCI, DebugName, "image");
}

ImageViewWrapper VulkanLogicalDevice::CreateImageView(const VkImageViewCreateInfo& ImageViewCI,
                                                      const char*                  DebugName) const
{
    VERIFY_EXPR(ImageViewCI.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    return CreateVulkanObject<VkImageView, VulkanHandleTypeId::ImageView>(vkCreateImageView, ImageViewCI, DebugName, "image view");
}

SamplerWrapper VulkanLogicalDevice::CreateSampler(const VkSamplerCreateInfo& SamplerCI, const char* DebugName) const
{
    VERIFY_EXPR(SamplerCI.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    return CreateVulkanObject<VkSampler, VulkanHandleTypeId::Sampler>(vkCreateSampler, SamplerCI, DebugName, "sampler");
}

FenceWrapper VulkanLogicalDevice::CreateFence(const VkFenceCreateInfo& FenceCI, const char* DebugName) const
{
    VERIFY_EXPR(FenceCI.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    return CreateVulkanObject<VkFence, VulkanHandleTypeId::Fence>(vkCreateFence, FenceCI, DebugName, "fence");
}

RenderPassWrapper VulkanLogicalDevice::CreateRenderPass(const VkRenderPassCreateInfo& RenderPassCI, const char* DebugName) const
{
    VERIFY_EXPR(RenderPassCI.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    return CreateVulkanObject<VkRenderPass, VulkanHandleTypeId::RenderPass>(vkCreateRenderPass, RenderPassCI, DebugName, "render pass");
}

DeviceMemoryWrapper VulkanLogicalDevice::AllocateDeviceMemory(const VkMemoryAllocateInfo& AllocInfo,
                                                              const char*                 DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkDeviceMemory vkDeviceMem = VK_NULL_HANDLE;

    auto err = vkAllocateMemory(m_VkDevice, &AllocInfo, m_VkAllocator, &vkDeviceMem);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to allocate device memory '", DebugName, '\'');

    if (*DebugName != 0)
        SetDeviceMemoryName(m_VkDevice, vkDeviceMem, DebugName);

    return DeviceMemoryWrapper{GetSharedPtr(), std::move(vkDeviceMem)};
}

PipelineWrapper VulkanLogicalDevice::CreateComputePipeline(const VkComputePipelineCreateInfo& PipelineCI,
                                                           VkPipelineCache                    cache,
                                                           const char*                        DebugName) const
{
    VERIFY_EXPR(PipelineCI.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    auto err = vkCreateComputePipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create compute pipeline '", DebugName, '\'');

    if (*DebugName != 0)
        SetPipelineName(m_VkDevice, vkPipeline, DebugName);

    return PipelineWrapper{GetSharedPtr(), std::move(vkPipeline)};
}

PipelineWrapper VulkanLogicalDevice::CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& PipelineCI,
                                                            VkPipelineCache                     cache,
                                                            const char*                         DebugName) const
{
    VERIFY_EXPR(PipelineCI.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    auto err = vkCreateGraphicsPipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create graphics pipeline '", DebugName, '\'');

    if (*DebugName != 0)
        SetPipelineName(m_VkDevice, vkPipeline, DebugName);

    return PipelineWrapper{GetSharedPtr(), std::move(vkPipeline)};
}

ShaderModuleWrapper VulkanLogicalDevice::CreateShaderModule(const VkShaderModuleCreateInfo& ShaderModuleCI, const char* DebugName) const
{
    VERIFY_EXPR(ShaderModuleCI.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    return CreateVulkanObject<VkShaderModule, VulkanHandleTypeId::ShaderModule>(vkCreateShaderModule, ShaderModuleCI, DebugName, "shader module");
}

PipelineLayoutWrapper VulkanLogicalDevice::CreatePipelineLayout(const VkPipelineLayoutCreateInfo& PipelineLayoutCI, const char* DebugName) const
{
    VERIFY_EXPR(PipelineLayoutCI.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    return CreateVulkanObject<VkPipelineLayout, VulkanHandleTypeId::PipelineLayout>(vkCreatePipelineLayout, PipelineLayoutCI, DebugName, "pipeline layout");
}

FramebufferWrapper VulkanLogicalDevice::CreateFramebuffer(const VkFramebufferCreateInfo& FramebufferCI, const char* DebugName) const
{
    VERIFY_EXPR(FramebufferCI.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    return CreateVulkanObject<VkFramebuffer, VulkanHandleTypeId::Framebuffer>(vkCreateFramebuffer, FramebufferCI, DebugName, "framebuffer");
}

DescriptorPoolWrapper VulkanLogicalDevice::CreateDescriptorPool(const VkDescriptorPoolCreateInfo& DescrPoolCI, const char* DebugName) const
{
    VERIFY_EXPR(DescrPoolCI.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    return CreateVulkanObject<VkDescriptorPool, VulkanHandleTypeId::DescriptorPool>(vkCreateDescriptorPool, DescrPoolCI, DebugName, "descriptor pool");
}

DescriptorSetLayoutWrapper VulkanLogicalDevice::CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& LayoutCI, const char* DebugName) const
{
    VERIFY_EXPR(LayoutCI.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    return CreateVulkanObject<VkDescriptorSetLayout, VulkanHandleTypeId::DescriptorSetLayout>(vkCreateDescriptorSetLayout, LayoutCI, DebugName, "descriptor set layout");
}

SemaphoreWrapper VulkanLogicalDevice::CreateSemaphore(const VkSemaphoreCreateInfo& SemaphoreCI, const char* DebugName) const
{
    VERIFY_EXPR(SemaphoreCI.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    return CreateVulkanObject<VkSemaphore, VulkanHandleTypeId::Semaphore>(vkCreateSemaphore, SemaphoreCI, DebugName, "semaphore");
}

QueryPoolWrapper VulkanLogicalDevice::CreateQueryPool(const VkQueryPoolCreateInfo& QueryPoolCI, const char* DebugName) const
{
    VERIFY_EXPR(QueryPoolCI.sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
    return CreateVulkanObject<VkQueryPool, VulkanHandleTypeId::QueryPool>(vkCreateQueryPool, QueryPoolCI, DebugName, "query pool");
}

AccelStructWrapper VulkanLogicalDevice::CreateAccelStruct(const VkAccelerationStructureCreateInfoKHR& CI, const char* DebugName) const
{
    VERIFY_EXPR(CI.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
    return CreateVulkanObject<VkAccelerationStructureKHR, VulkanHandleTypeId::AccelerationStructureKHR>(vkCreateAccelerationStructureKHR, CI, DebugName, "acceleration structure");
}

VkCommandBuffer VulkanLogicalDevice::AllocateVkCommandBuffer(const VkCommandBufferAllocateInfo& AllocInfo, const char* DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkCommandBuffer CmdBuff = VK_NULL_HANDLE;

    auto err = vkAllocateCommandBuffers(m_VkDevice, &AllocInfo, &CmdBuff);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to allocate command buffer '", DebugName, '\'');
    (void)err;

    if (*DebugName != 0)
        SetCommandBufferName(m_VkDevice, CmdBuff, DebugName);

    return CmdBuff;
}

VkDescriptorSet VulkanLogicalDevice::AllocateVkDescriptorSet(const VkDescriptorSetAllocateInfo& AllocInfo, const char* DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    VERIFY_EXPR(AllocInfo.descriptorSetCount == 1);

    if (DebugName == nullptr)
        DebugName = "";

    VkDescriptorSet DescrSet = VK_NULL_HANDLE;

    auto err = vkAllocateDescriptorSets(m_VkDevice, &AllocInfo, &DescrSet);
    if (err != VK_SUCCESS)
        return VK_NULL_HANDLE;

    if (*DebugName != 0)
        SetDescriptorSetName(m_VkDevice, DescrSet, DebugName);

    return DescrSet;
}

void VulkanLogicalDevice::ReleaseVulkanObject(CommandPoolWrapper&& CmdPool) const
{
    vkDestroyCommandPool(m_VkDevice, CmdPool.m_VkObject, m_VkAllocator);
    CmdPool.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(BufferWrapper&& Buffer) const
{
    vkDestroyBuffer(m_VkDevice, Buffer.m_VkObject, m_VkAllocator);
    Buffer.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(BufferViewWrapper&& BufferView) const
{
    vkDestroyBufferView(m_VkDevice, BufferView.m_VkObject, m_VkAllocator);
    BufferView.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(ImageWrapper&& Image) const
{
    vkDestroyImage(m_VkDevice, Image.m_VkObject, m_VkAllocator);
    Image.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(ImageViewWrapper&& ImageView) const
{
    vkDestroyImageView(m_VkDevice, ImageView.m_VkObject, m_VkAllocator);
    ImageView.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(SamplerWrapper&& Sampler) const
{
    vkDestroySampler(m_VkDevice, Sampler.m_VkObject, m_VkAllocator);
    Sampler.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(FenceWrapper&& Fence) const
{
    vkDestroyFence(m_VkDevice, Fence.m_VkObject, m_VkAllocator);
    Fence.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(RenderPassWrapper&& RenderPass) const
{
    vkDestroyRenderPass(m_VkDevice, RenderPass.m_VkObject, m_VkAllocator);
    RenderPass.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(DeviceMemoryWrapper&& Memory) const
{
    vkFreeMemory(m_VkDevice, Memory.m_VkObject, m_VkAllocator);
    Memory.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(PipelineWrapper&& Pipeline) const
{
    vkDestroyPipeline(m_VkDevice, Pipeline.m_VkObject, m_VkAllocator);
    Pipeline.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(ShaderModuleWrapper&& ShaderModule) const
{
    vkDestroyShaderModule(m_VkDevice, ShaderModule.m_VkObject, m_VkAllocator);
    ShaderModule.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(PipelineLayoutWrapper&& PipelineLayout) const
{
    vkDestroyPipelineLayout(m_VkDevice, PipelineLayout.m_VkObject, m_VkAllocator);
    PipelineLayout.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(FramebufferWrapper&& Framebuffer) const
{
    vkDestroyFramebuffer(m_VkDevice, Framebuffer.m_VkObject, m_VkAllocator);
    Framebuffer.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(DescriptorPoolWrapper&& DescriptorPool) const
{
    vkDestroyDescriptorPool(m_VkDevice, DescriptorPool.m_VkObject, m_VkAllocator);
    DescriptorPool.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(DescriptorSetLayoutWrapper&& DescriptorSetLayout) const
{
    vkDestroyDescriptorSetLayout(m_VkDevice, DescriptorSetLayout.m_VkObject, m_VkAllocator);
    DescriptorSetLayout.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(SemaphoreWrapper&& Semaphore) const
{
    vkDestroySemaphore(m_VkDevice, Semaphore.m_VkObject, m_VkAllocator);
    Semaphore.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(QueryPoolWrapper&& QueryPool) const
{
    vkDestroyQueryPool(m_VkDevice, QueryPool.m_VkObject, m_VkAllocator);
    QueryPool.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::ReleaseVulkanObject(AccelStructWrapper&& AccelStruct) const
{
    vkDestroyAccelerationStructureKHR(m_VkDevice, AccelStruct.m_VkObject, m_VkAllocator);
    AccelStruct.m_VkObject = VK_NULL_HANDLE;
}

void VulkanLogicalDevice::FreeDescriptorSet(VkDescriptorPool Pool, VkDescriptorSet Set) const
{
    VERIFY_EXPR(Pool != VK_NULL_HANDLE && Set != VK_NULL_HANDLE);
    vkFreeDescriptorSets(m_VkDevice, Pool, 1, &Set);
}




VkMemoryRequirements VulkanLogicalDevice::GetBufferMemoryRequirements(VkBuffer vkBuffer) const
{
    VkMemoryRequirements MemReqs = {};
    vkGetBufferMemoryRequirements(m_VkDevice, vkBuffer, &MemReqs);
    return MemReqs;
}

VkMemoryRequirements VulkanLogicalDevice::GetImageMemoryRequirements(VkImage vkImage) const
{
    VkMemoryRequirements MemReqs = {};
    vkGetImageMemoryRequirements(m_VkDevice, vkImage, &MemReqs);
    return MemReqs;
}

VkMemoryRequirements VulkanLogicalDevice::GetASMemoryRequirements(const VkAccelerationStructureMemoryRequirementsInfoKHR& Info) const
{
    VkMemoryRequirements2 MemReqs = {};
    vkGetAccelerationStructureMemoryRequirementsKHR(m_VkDevice, &Info, &MemReqs);
    return MemReqs.memoryRequirements;
}

VkResult VulkanLogicalDevice::BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
    return vkBindBufferMemory(m_VkDevice, buffer, memory, memoryOffset);
}

VkResult VulkanLogicalDevice::BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
    return vkBindImageMemory(m_VkDevice, image, memory, memoryOffset);
}

VkResult VulkanLogicalDevice::BindASMemory(VkAccelerationStructureKHR AS, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
    VkBindAccelerationStructureMemoryInfoKHR Info = {};

    Info.sType                 = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
    Info.memory                = memory;
    Info.memoryOffset          = memoryOffset;
    Info.deviceIndexCount      = 0;
    Info.pDeviceIndices        = nullptr;
    Info.accelerationStructure = AS;

    return vkBindAccelerationStructureMemoryKHR(m_VkDevice, 1, &Info);
}

VkDeviceAddress VulkanLogicalDevice::GetAccelerationStructureDeviceAddress(VkAccelerationStructureKHR AS) const
{
    VkAccelerationStructureDeviceAddressInfoKHR Info = {};

    Info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    Info.accelerationStructure = AS;

    return vkGetAccelerationStructureDeviceAddressKHR(m_VkDevice, &Info);
}

VkResult VulkanLogicalDevice::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
    return vkMapMemory(m_VkDevice, memory, offset, size, flags, ppData);
}

void VulkanLogicalDevice::UnmapMemory(VkDeviceMemory memory) const
{
    vkUnmapMemory(m_VkDevice, memory);
}

VkResult VulkanLogicalDevice::InvalidateMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
    return vkInvalidateMappedMemoryRanges(m_VkDevice, memoryRangeCount, pMemoryRanges);
}

VkResult VulkanLogicalDevice::FlushMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
    return vkFlushMappedMemoryRanges(m_VkDevice, memoryRangeCount, pMemoryRanges);
}

VkResult VulkanLogicalDevice::GetFenceStatus(VkFence fence) const
{
    return vkGetFenceStatus(m_VkDevice, fence);
}

VkResult VulkanLogicalDevice::ResetFence(VkFence fence) const
{
    auto err = vkResetFences(m_VkDevice, 1, &fence);
    DEV_CHECK_ERR(err == VK_SUCCESS, "vkResetFences() failed");
    return err;
}

VkResult VulkanLogicalDevice::WaitForFences(uint32_t       fenceCount,
                                            const VkFence* pFences,
                                            VkBool32       waitAll,
                                            uint64_t       timeout) const
{
    return vkWaitForFences(m_VkDevice, fenceCount, pFences, waitAll, timeout);
}

void VulkanLogicalDevice::UpdateDescriptorSets(uint32_t                    descriptorWriteCount,
                                               const VkWriteDescriptorSet* pDescriptorWrites,
                                               uint32_t                    descriptorCopyCount,
                                               const VkCopyDescriptorSet*  pDescriptorCopies) const
{
    vkUpdateDescriptorSets(m_VkDevice, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult VulkanLogicalDevice::ResetCommandPool(VkCommandPool           vkCmdPool,
                                               VkCommandPoolResetFlags flags) const
{
    auto err = vkResetCommandPool(m_VkDevice, vkCmdPool, flags);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to reset command pool");
    return err;
}

VkResult VulkanLogicalDevice::ResetDescriptorPool(VkDescriptorPool           vkDescriptorPool,
                                                  VkDescriptorPoolResetFlags flags) const
{
    auto err = vkResetDescriptorPool(m_VkDevice, vkDescriptorPool, flags);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to reset descriptor pool");
    return err;
}

} // namespace VulkanUtilities
