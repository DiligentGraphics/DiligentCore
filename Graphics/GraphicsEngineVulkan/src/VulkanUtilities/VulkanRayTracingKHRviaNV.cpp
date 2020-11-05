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


// KHR ray tracing emulation through NVidia extension.
// Will be deprecated after the release of KHR extension.

#include <mutex>
#include <unordered_map>
#include <vector>

#include "VulkanErrors.hpp"
#include "VulkanUtilities/VulkanLogicalDevice.hpp"

namespace VulkanUtilities
{

#if DILIGENT_USE_VOLK
static_assert(sizeof(VkAccelerationStructureKHR) == sizeof(VkAccelerationStructureNV), "KHR is incompatible with NV extension");
static_assert(sizeof(VkDeviceAddress) == 8, "KHR is incompatible with NV extension");

namespace
{

std::mutex                                    g_BufferDeviceAddressGuard;
std::unordered_map<VkDeviceAddress, VkBuffer> g_DeviceAddressToBuffer;
std::unordered_map<VkBuffer, VkDeviceAddress> g_BufferToDeviceAddress;
uint32_t                                      g_BufferDeviceAddressCounter = 0;
constexpr VkDeviceAddress                     g_BufferMask                 = 0xFFFFFFFF00000000ull;

PFN_vkCreateBuffer              Origin_vkCreateBuffer              = nullptr;
PFN_vkDestroyBuffer             Origin_vkDestroyBuffer             = nullptr;
PFN_vkGetBufferDeviceAddressKHR Origin_vkGetBufferDeviceAddressKHR = nullptr;
PFN_vkAllocateMemory            Origin_vkAllocateMemory            = nullptr;


VKAPI_ATTR VkResult VKAPI_CALL Wrap_vkCreateBuffer(VkDevice                     device,
                                                   const VkBufferCreateInfo*    pCreateInfo,
                                                   const VkAllocationCallbacks* pAllocator,
                                                   VkBuffer*                    pBuffer)
{
    VkBufferCreateInfo CreateInfo = *pCreateInfo;
    CreateInfo.usage &= ~VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return Origin_vkCreateBuffer(device, &CreateInfo, pAllocator, pBuffer);
}

VKAPI_ATTR VkResult VKAPI_PTR Wrap_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    VkMemoryAllocateInfo AllocInfo = *pAllocateInfo;

    for (auto* pNext = static_cast<VkBaseOutStructure*>(const_cast<void*>(AllocInfo.pNext)); pNext;)
    {
        // remove VkMemoryAllocateFlagsInfo because VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is removed from buffer create info.
        if (pNext->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
            pNext->pNext = pNext->pNext;

        pNext = pNext->pNext;
    }

    return Origin_vkAllocateMemory(device, &AllocInfo, pAllocator, pMemory);
}

VKAPI_ATTR void VKAPI_CALL Wrap_vkDestroyBuffer(VkDevice                     device,
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

VKAPI_ATTR VkDeviceAddress VKAPI_CALL Wrap_vkGetBufferDeviceAddressKHR(VkDevice                         device,
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
    VkDeviceAddress Addr                   = VkDeviceAddress{++g_BufferDeviceAddressCounter} << 32;
    g_BufferToDeviceAddress[pInfo->buffer] = Addr;
    g_DeviceAddressToBuffer[Addr]          = pInfo->buffer;
    return Addr;
}

struct BufferAndOffset
{
    VkBuffer     Buffer;
    VkDeviceSize Offset;
};
BufferAndOffset DeviceAddressToBuffer(VkDeviceAddress Addr)
{
    if (Addr == 0)
        return {VK_NULL_HANDLE, 0};

    std::unique_lock<std::mutex> lock{g_BufferDeviceAddressGuard};

    auto iter = g_DeviceAddressToBuffer.find(Addr & g_BufferMask);
    if (iter == g_DeviceAddressToBuffer.end())
    {
        UNEXPECTED("Failed to map device address to buffer");
        return {VK_NULL_HANDLE, 0};
    }

    return {iter->second, Addr & ~g_BufferMask};
}

BufferAndOffset DeviceAddressToBuffer(const VkDeviceOrHostAddressConstKHR& Addr)
{
    return DeviceAddressToBuffer(Addr.deviceAddress);
}

BufferAndOffset DeviceAddressToBuffer(const VkDeviceOrHostAddressKHR& Addr)
{
    return DeviceAddressToBuffer(Addr.deviceAddress);
}


VKAPI_ATTR VkResult VKAPI_CALL Redirect_vkCreateAccelerationStructureKHR(VkDevice                                    device,
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
        VERIFY_EXPR(pCreateInfo->maxGeometryCount == 1);

        CreateInfo.info.instanceCount = pCreateInfo->pGeometryInfos->maxPrimitiveCount;
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
                    dst.geometry.triangles.indexCount  = src.maxPrimitiveCount * 3;
                    dst.geometry.triangles.vertexCount = std::max(src.maxPrimitiveCount * 6, src.maxVertexCount);
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
        UNEXPECTED("unknown AS type");
        return VK_RESULT_MAX_ENUM;
    }

    return vkCreateAccelerationStructureNV(device, &CreateInfo, pAllocator, reinterpret_cast<VkAccelerationStructureNV*>(pAccelerationStructure));
}

VKAPI_ATTR void VKAPI_CALL Redirect_vkGetAccelerationStructureMemoryRequirementsKHR(VkDevice                                                device,
                                                                                    const VkAccelerationStructureMemoryRequirementsInfoKHR* pInfo,
                                                                                    VkMemoryRequirements2*                                  pMemoryRequirements)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR);
    VERIFY_EXPR(pMemoryRequirements->sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
    VERIFY_EXPR(pInfo->pNext == nullptr);
    VERIFY_EXPR(pInfo->buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);

    VkAccelerationStructureMemoryRequirementsInfoNV Info = {};

    Info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    Info.type                  = pInfo->type;
    Info.accelerationStructure = pInfo->accelerationStructure;

    return vkGetAccelerationStructureMemoryRequirementsNV(device, &Info, pMemoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL Redirect_vkBindAccelerationStructureMemoryKHR(VkDevice                                        device,
                                                                             uint32_t                                        bindInfoCount,
                                                                             const VkBindAccelerationStructureMemoryInfoKHR* pBindInfos)
{
    VERIFY_EXPR(pBindInfos->sType == VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV);
    return vkBindAccelerationStructureMemoryNV(device, bindInfoCount, pBindInfos);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL Redirect_vkGetAccelerationStructureDeviceAddressKHR(VkDevice                                           device,
                                                                                          const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);

    VkDeviceAddress result = 0;
    vkGetAccelerationStructureHandleNV(device, pInfo->accelerationStructure, sizeof(result), &result);
    return result;
}

VKAPI_ATTR void VKAPI_CALL Redirect_vkCmdBuildAccelerationStructureKHR(VkCommandBuffer                                         commandBuffer,
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

                dst.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
                dst.geometry.triangles.pNext = nullptr;

                dst.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
                dst.geometry.aabbs.pNext = nullptr;

                if (dst.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
                {
                    VERIFY_EXPR(src.geometry.triangles.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR);
                    VERIFY_EXPR(src.geometry.triangles.pNext == nullptr);
                    VERIFY_EXPR(off.firstVertex == 0);

                    BufferAndOffset VB = DeviceAddressToBuffer(src.geometry.triangles.vertexData);
                    BufferAndOffset IB = DeviceAddressToBuffer(src.geometry.triangles.indexData);
                    BufferAndOffset TB = DeviceAddressToBuffer(src.geometry.triangles.transformData);

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
                        dst.geometry.triangles.indexCount  = off.primitiveCount * 3;
                        dst.geometry.triangles.vertexCount = off.primitiveCount * 6;
                    }
                }
                else
                {
                    VERIFY_EXPR(src.geometry.aabbs.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR);
                    VERIFY_EXPR(src.geometry.aabbs.pNext == nullptr);
                    VERIFY_EXPR(src.geometry.aabbs.stride <= std::numeric_limits<uint32_t>::max());

                    BufferAndOffset Data = DeviceAddressToBuffer(src.geometry.aabbs.data);

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
            UNEXPECTED("unknown AS type");
        }
    }
}

VKAPI_ATTR void VKAPI_CALL Redirect_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer                           commandBuffer,
                                                                      const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    VERIFY_EXPR(pInfo->sType == VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR);
    VERIFY_EXPR(pInfo->pNext == nullptr);

    vkCmdCopyAccelerationStructureNV(commandBuffer, pInfo->dst, pInfo->src, pInfo->mode);
}

VKAPI_ATTR void VKAPI_CALL Redirect_vkCmdTraceRaysKHR(VkCommandBuffer                 commandBuffer,
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

VKAPI_ATTR VkResult VKAPI_CALL Redirect_vkGetRayTracingShaderGroupHandlesKHR(VkDevice   device,
                                                                             VkPipeline pipeline,
                                                                             uint32_t   firstGroup,
                                                                             uint32_t   groupCount,
                                                                             size_t     dataSize,
                                                                             void*      pData)
{
    return vkGetRayTracingShaderGroupHandlesNV(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VKAPI_ATTR void VKAPI_CALL Redirect_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
    return vkDestroyAccelerationStructureNV(device, accelerationStructure, pAllocator);
}


VKAPI_ATTR VkResult VKAPI_CALL Redirect_vkCreateRayTracingPipelinesKHR(VkDevice                                 device,
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

} // namespace

void EnableRayTracingKHRviaNV()
{
    LOG_WARNING_MESSAGE("This is fallback implementation, you should use VK_KHR_ray_tracing instead");

    vkCreateAccelerationStructureKHR                = &Redirect_vkCreateAccelerationStructureKHR;
    vkGetAccelerationStructureMemoryRequirementsKHR = &Redirect_vkGetAccelerationStructureMemoryRequirementsKHR;
    vkBindAccelerationStructureMemoryKHR            = &Redirect_vkBindAccelerationStructureMemoryKHR;
    vkGetAccelerationStructureDeviceAddressKHR      = &Redirect_vkGetAccelerationStructureDeviceAddressKHR;
    vkCmdBuildAccelerationStructureKHR              = &Redirect_vkCmdBuildAccelerationStructureKHR;
    vkCmdCopyAccelerationStructureKHR               = &Redirect_vkCmdCopyAccelerationStructureKHR;
    vkGetRayTracingShaderGroupHandlesKHR            = &Redirect_vkGetRayTracingShaderGroupHandlesKHR;
    vkCreateRayTracingPipelinesKHR                  = &Redirect_vkCreateRayTracingPipelinesKHR;
    vkCmdTraceRaysKHR                               = &Redirect_vkCmdTraceRaysKHR;
    vkDestroyAccelerationStructureKHR               = &Redirect_vkDestroyAccelerationStructureKHR;

    Origin_vkGetBufferDeviceAddressKHR = vkGetBufferDeviceAddressKHR;
    Origin_vkCreateBuffer              = vkCreateBuffer;
    Origin_vkDestroyBuffer             = vkDestroyBuffer;
    Origin_vkAllocateMemory            = vkAllocateMemory;
    vkCreateBuffer                     = &Wrap_vkCreateBuffer;
    vkDestroyBuffer                    = &Wrap_vkDestroyBuffer;
    vkAllocateMemory                   = Wrap_vkAllocateMemory;
    vkGetBufferDeviceAddressKHR        = &Wrap_vkGetBufferDeviceAddressKHR;
    vkGetBufferDeviceAddress           = &Wrap_vkGetBufferDeviceAddressKHR;
    vkGetBufferDeviceAddressEXT        = &Wrap_vkGetBufferDeviceAddressKHR;
}
#endif // DILIGENT_USE_VOLK

} // namespace VulkanUtilities
