/*     Copyright 2015-2019 Egor Yusov
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

#include "pch.h"
#include "BufferVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "VulkanTypeConversions.h"
#include "BufferViewVkImpl.h"
#include "GraphicsAccessories.h"
#include "EngineMemory.h"
#include "StringTools.h"
#include "VulkanUtilities/VulkanDebug.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"

namespace Diligent
{

BufferVkImpl :: BufferVkImpl(IReferenceCounters*        pRefCounters, 
                             FixedBlockMemoryAllocator& BuffViewObjMemAllocator, 
                             RenderDeviceVkImpl*        pRenderDeviceVk, 
                             const BufferDesc&          BuffDesc, 
                             const BufferData*          pBuffData /*= nullptr*/) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceVk, BuffDesc, false),
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(VulkanDynamicAllocation, GetRawAllocator(), "Allocator for vector<VulkanDynamicAllocation>"))
{
#define LOG_BUFFER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\": ", ##__VA_ARGS__);

    if( m_Desc.Usage == USAGE_STATIC && (pBuffData == nullptr || pBuffData->pData == nullptr) )
        LOG_BUFFER_ERROR_AND_THROW("Static buffer must be initialized with data at creation time")

    if( m_Desc.Usage == USAGE_DYNAMIC && pBuffData != nullptr && pBuffData->pData != nullptr )
        LOG_BUFFER_ERROR_AND_THROW("Dynamic buffer must be initialized via Map()")

    if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
    {
        if (m_Desc.CPUAccessFlags != CPU_ACCESS_WRITE && m_Desc.CPUAccessFlags != CPU_ACCESS_READ)
            LOG_BUFFER_ERROR_AND_THROW("Exactly one of the CPU_ACCESS_WRITE or CPU_ACCESS_READ flags must be specified for a cpu-accessible buffer")

        if (m_Desc.CPUAccessFlags == CPU_ACCESS_WRITE)
        {
            if (pBuffData != nullptr && pBuffData->pData != nullptr)
                LOG_BUFFER_ERROR_AND_THROW("CPU-writable staging buffers must be updated via map")
        }
    }

    const auto& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();
    const auto& DeviceLimits = pRenderDeviceVk->GetPhysicalDevice().GetProperties().limits;
    m_DynamicOffsetAlignment = std::max(Uint32{4}, static_cast<Uint32>(DeviceLimits.optimalBufferCopyOffsetAlignment));

    VkBufferCreateInfo VkBuffCI = {};
    VkBuffCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    VkBuffCI.pNext = nullptr;
    VkBuffCI.flags = 0; // VK_BUFFER_CREATE_SPARSE_BINDING_BIT, VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT, VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
    VkBuffCI.size = m_Desc.uiSizeInBytes;
    VkBuffCI.usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | // The buffer can be used as the source of a transfer command 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;  // The buffer can be used as the destination of a transfer command
    if (m_Desc.BindFlags & BIND_UNORDERED_ACCESS)
    {
        // VkBuffCI.usage |= m_Desc.Mode == BUFFER_MODE_FORMATTED ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        // HLSL formatted buffers are mapped to GLSL storage buffers:
        //
        //     RWBuffer<uint4> RWBuff
        //     
        //                 |
        //                 V
        //     
        //     layout(std140, binding = 3) buffer RWBuff
        //     {
        //         uvec4 data[];
        //     }g_RWBuff;
        // 
        // So we have to set both VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT and VK_BUFFER_USAGE_STORAGE_BUFFER_BIT bits
        VkBuffCI.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        // Each element of pDynamicOffsets of vkCmdBindDescriptorSets function which corresponds to a descriptor
        // binding with type VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC must be a multiple of
        // VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment (13.2.5)
        m_DynamicOffsetAlignment = std::max(m_DynamicOffsetAlignment, static_cast<Uint32>(DeviceLimits.minTexelBufferOffsetAlignment));
        m_DynamicOffsetAlignment = std::max(m_DynamicOffsetAlignment, static_cast<Uint32>(DeviceLimits.minStorageBufferOffsetAlignment));
    }
    if (m_Desc.BindFlags & BIND_SHADER_RESOURCE)
    {
        // VkBuffCI.usage |= m_Desc.Mode == BUFFER_MODE_FORMATTED ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        // HLSL buffer SRV are mapped to storge buffers in GLSL, so we need to set both 
        // VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER and VK_BUFFER_USAGE_STORAGE_BUFFER_BIT flags
        VkBuffCI.usage |= VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        m_DynamicOffsetAlignment = std::max(m_DynamicOffsetAlignment, static_cast<Uint32>(DeviceLimits.minTexelBufferOffsetAlignment));
        m_DynamicOffsetAlignment = std::max(m_DynamicOffsetAlignment, static_cast<Uint32>(DeviceLimits.minStorageBufferOffsetAlignment));
    }
    if (m_Desc.BindFlags & BIND_VERTEX_BUFFER)
        VkBuffCI.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (m_Desc.BindFlags & BIND_INDEX_BUFFER)
        VkBuffCI.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (m_Desc.BindFlags & BIND_INDIRECT_DRAW_ARGS)
        VkBuffCI.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (m_Desc.BindFlags & BIND_UNIFORM_BUFFER)
    {
        VkBuffCI.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        // Each element of pDynamicOffsets parameter of vkCmdBindDescriptorSets function which corresponds to a descriptor
        // binding with type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC must be a multiple of 
        // VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment (13.2.5)
        m_DynamicOffsetAlignment = std::max(m_DynamicOffsetAlignment, static_cast<Uint32>(DeviceLimits.minUniformBufferOffsetAlignment));
    }

    if(m_Desc.Usage == USAGE_DYNAMIC)
    {
        auto CtxCount = 1 + pRenderDeviceVk->GetNumDeferredContexts();
        m_DynamicAllocations.reserve(CtxCount);
        for(Uint32 ctx=0; ctx < CtxCount; ++ctx)
            m_DynamicAllocations.emplace_back();
    }

    if (m_Desc.Usage == USAGE_DYNAMIC && (VkBuffCI.usage & (VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) == 0)
    {
        // Dynamic constant/vertex/index buffers are suballocated in the upload heap when Map() is called.
        // Dynamic buffers with SRV or UAV flags need to be allocated in GPU-only memory
        constexpr RESOURCE_STATE State = static_cast<RESOURCE_STATE>(
                 RESOURCE_STATE_VERTEX_BUFFER   | 
                 RESOURCE_STATE_INDEX_BUFFER    |
                 RESOURCE_STATE_CONSTANT_BUFFER |
                 RESOURCE_STATE_SHADER_RESOURCE |
                 RESOURCE_STATE_COPY_SOURCE     |
                 RESOURCE_STATE_INDIRECT_ARGUMENT);
        SetState(State);

#ifdef _DEBUG
        {
            VkAccessFlags AccessFlags = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | 
                                        VK_ACCESS_INDEX_READ_BIT            | 
                                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | 
                                        VK_ACCESS_UNIFORM_READ_BIT          | 
                                        VK_ACCESS_SHADER_READ_BIT           |
                                        VK_ACCESS_TRANSFER_READ_BIT;
            VERIFY_EXPR(ResourceStateFlagsToVkAccessFlags(State) == AccessFlags);
        }
#endif
    }
    else
    {
        VkBuffCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // sharing mode of the buffer when it will be accessed by multiple queue families.
        VkBuffCI.queueFamilyIndexCount = 0; // number of entries in the pQueueFamilyIndices array
        VkBuffCI.pQueueFamilyIndices = nullptr; // list of queue families that will access this buffer 
                                                // (ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT).

        m_VulkanBuffer = LogicalDevice.CreateBuffer(VkBuffCI, m_Desc.Name);

        VkMemoryRequirements MemReqs = LogicalDevice.GetBufferMemoryRequirements(m_VulkanBuffer);

        VkMemoryPropertyFlags BufferMemoryFlags = 0;
        if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
            BufferMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        else
            BufferMemoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VERIFY( IsPowerOfTwo(MemReqs.alignment), "Alignment is not power of 2!");
        m_MemoryAllocation = pRenderDeviceVk->AllocateMemory(MemReqs, BufferMemoryFlags);

        auto AlignedOffset = Align(VkDeviceSize{m_MemoryAllocation.UnalignedOffset}, MemReqs.alignment);
        VERIFY(m_MemoryAllocation.Size >= MemReqs.size + (AlignedOffset - m_MemoryAllocation.UnalignedOffset), "Size of memory allocation is too small");
        auto Memory = m_MemoryAllocation.Page->GetVkMemory();
        auto err = LogicalDevice.BindBufferMemory(m_VulkanBuffer, Memory, AlignedOffset);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to bind buffer memory");

        bool bInitializeBuffer = (pBuffData != nullptr && pBuffData->pData != nullptr && pBuffData->DataSize > 0);
        RESOURCE_STATE InitialState = RESOURCE_STATE_UNDEFINED;
        if( bInitializeBuffer )
        {
            VkBufferCreateInfo VkStaginBuffCI = VkBuffCI;
            VkStaginBuffCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            std::string StagingBufferName = "Staging buffer for '";
            StagingBufferName += m_Desc.Name;
            StagingBufferName += '\'';
            VulkanUtilities::BufferWrapper StagingBuffer = LogicalDevice.CreateBuffer(VkStaginBuffCI, StagingBufferName.c_str());

            VkMemoryRequirements StagingBufferMemReqs = LogicalDevice.GetBufferMemoryRequirements(StagingBuffer);
            VERIFY( IsPowerOfTwo(StagingBufferMemReqs.alignment), "Alignment is not power of 2!");

            // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands vkFlushMappedMemoryRanges 
            // and vkInvalidateMappedMemoryRanges are NOT needed to flush host writes to the device or make device writes visible
            // to the host (10.2)
            auto StagingMemoryAllocation = pRenderDeviceVk->AllocateMemory(StagingBufferMemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            auto StagingBufferMemory = StagingMemoryAllocation.Page->GetVkMemory();
            auto AlignedStagingMemOffset = Align(VkDeviceSize{StagingMemoryAllocation.UnalignedOffset}, StagingBufferMemReqs.alignment);
            VERIFY_EXPR(StagingMemoryAllocation.Size >= StagingBufferMemReqs.size + (AlignedStagingMemOffset - StagingMemoryAllocation.UnalignedOffset));

            auto* StagingData = reinterpret_cast<uint8_t*>(StagingMemoryAllocation.Page->GetCPUMemory());
            if (StagingData == nullptr)
                LOG_BUFFER_ERROR_AND_THROW("Failed to allocate staging data");
            memcpy(StagingData + AlignedStagingMemOffset, pBuffData->pData, pBuffData->DataSize);

            err = LogicalDevice.BindBufferMemory(StagingBuffer, StagingBufferMemory, AlignedStagingMemOffset);
            CHECK_VK_ERROR_AND_THROW(err, "Failed to bind staging bufer memory");

            VulkanUtilities::CommandPoolWrapper CmdPool;
            VkCommandBuffer vkCmdBuff;
            pRenderDeviceVk->AllocateTransientCmdPool(CmdPool, vkCmdBuff, "Transient command pool to copy staging data to a device buffer");

            auto EnabledGraphicsShaderStages = LogicalDevice.GetEnabledGraphicsShaderStages();
            VulkanUtilities::VulkanCommandBuffer::BufferMemoryBarrier(vkCmdBuff, StagingBuffer, 0, VK_ACCESS_TRANSFER_READ_BIT, EnabledGraphicsShaderStages);
            InitialState = RESOURCE_STATE_COPY_DEST;
            VkAccessFlags AccessFlags = ResourceStateFlagsToVkAccessFlags(InitialState);
            VERIFY_EXPR(AccessFlags == VK_ACCESS_TRANSFER_WRITE_BIT);
            VulkanUtilities::VulkanCommandBuffer::BufferMemoryBarrier(vkCmdBuff, m_VulkanBuffer, 0, AccessFlags, EnabledGraphicsShaderStages);

            // Copy commands MUST be recorded outside of a render pass instance. This is OK here
            // as copy will be the only command in the cmd buffer
            VkBufferCopy BuffCopy = {};
            BuffCopy.srcOffset = 0;
            BuffCopy.dstOffset = 0;
            BuffCopy.size = VkBuffCI.size;
            vkCmdCopyBuffer(vkCmdBuff, StagingBuffer, m_VulkanBuffer, 1, &BuffCopy);

            Uint32 QueueIndex = 0;
	        pRenderDeviceVk->ExecuteAndDisposeTransientCmdBuff(QueueIndex, vkCmdBuff, std::move(CmdPool));


            // After command buffer is submitted, safe-release staging resources. This strategy
            // is little overconservative as the resources will only be released after the 
            // first command buffer submitted through the immediate context is complete

            // Next Cmd Buff| Next Fence |               This Thread                      |           Immediate Context
            //              |            |                                                |
            //      N       |     F      |                                                |
            //              |            |                                                |
            //              |            |  ExecuteAndDisposeTransientCmdBuff(vkCmdBuff)  |
            //              |            |  - SubmittedCmdBuffNumber = N                  |
            //              |            |  - SubmittedFenceValue = F                     |
            //     N+1 -  - | -  F+1  -  |                                                |  
            //              |            |  Release(StagingBuffer)                        |
            //              |            |  - {N+1, StagingBuffer} -> Stale Objects       |
            //              |            |                                                |
            //              |            |                                                |
            //              |            |                                                | ExecuteCommandBuffer()                                      
            //              |            |                                                | - SubmittedCmdBuffNumber = N+1
            //              |            |                                                | - SubmittedFenceValue = F+1
            //     N+2 -  - | -  F+2  -  |  -   -   -   -   -   -   -   -   -   -   -   - | 
            //              |            |                                                | - DiscardStaleVkObjects(N+1, F+1)  
            //              |            |                                                |   - {F+1, StagingBuffer} -> Release Queue 
            //              |            |                                                | 

            pRenderDeviceVk->SafeReleaseDeviceObject(std::move(StagingBuffer),           Uint64{1} << Uint64{QueueIndex});
            pRenderDeviceVk->SafeReleaseDeviceObject(std::move(StagingMemoryAllocation), Uint64{1} << Uint64{QueueIndex});
        }

        SetState(InitialState);
    }

    VERIFY_EXPR(IsInKnownState());
}


BufferVkImpl :: BufferVkImpl(IReferenceCounters*        pRefCounters, 
                             FixedBlockMemoryAllocator& BuffViewObjMemAllocator, 
                             RenderDeviceVkImpl*        pRenderDeviceVk, 
                             const BufferDesc&          BuffDesc, 
                             RESOURCE_STATE             InitialState,
                             VkBuffer                   vkBuffer) : 
    TBufferBase(pRefCounters, BuffViewObjMemAllocator, pRenderDeviceVk, BuffDesc, false),
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(VulkanDynamicAllocation, GetRawAllocator(), "Allocator for vector<VulkanDynamicAllocation>")),
    m_VulkanBuffer(vkBuffer)
{
    SetState(InitialState);
}

BufferVkImpl :: ~BufferVkImpl()
{
    // Vk object can only be destroyed when it is no longer used by the GPU
    if(m_VulkanBuffer != VK_NULL_HANDLE)
        m_pDevice->SafeReleaseDeviceObject(std::move(m_VulkanBuffer), m_Desc.CommandQueueMask);
    if(m_MemoryAllocation.Page != nullptr)
        m_pDevice->SafeReleaseDeviceObject(std::move(m_MemoryAllocation), m_Desc.CommandQueueMask);
}

IMPLEMENT_QUERY_INTERFACE( BufferVkImpl, IID_BufferVk, TBufferBase )


void BufferVkImpl::CreateViewInternal( const BufferViewDesc& OrigViewDesc, IBufferView** ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;

    try
    {
        auto& BuffViewAllocator = m_pDevice->GetBuffViewObjAllocator();
        VERIFY( &BuffViewAllocator == &m_dbgBuffViewAllocator, "Buff view allocator does not match allocator provided at buffer initialization" );

        BufferViewDesc ViewDesc = OrigViewDesc;
        if( ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS || ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE )
        {
            auto View = CreateView(ViewDesc);
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewVkImpl instance", BufferViewVkImpl, bIsDefaultView ? this : nullptr)
                                (GetDevice(), ViewDesc, this, std::move(View), bIsDefaultView );
        }

        if( !bIsDefaultView && *ppView )
            (*ppView)->AddRef();
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name, "\"" );
    }
}


VulkanUtilities::BufferViewWrapper BufferVkImpl::CreateView(struct BufferViewDesc& ViewDesc)
{
    VulkanUtilities::BufferViewWrapper BuffView;
    CorrectBufferViewDesc(ViewDesc);
    if( (ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE || ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS) && 
        (m_Desc.Mode == BUFFER_MODE_FORMATTED || m_Desc.Mode == BUFFER_MODE_RAW))
    {
        VkBufferViewCreateInfo ViewCI = {};
        ViewCI.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        ViewCI.pNext = nullptr;
        ViewCI.flags = 0; // reserved for future use
        ViewCI.buffer = m_VulkanBuffer;
        if (m_Desc.Mode == BUFFER_MODE_RAW && ViewDesc.Format.ValueType == VT_UNDEFINED)
        {
            ViewCI.format = VK_FORMAT_R32_UINT;
        }
        else
        {
            DEV_CHECK_ERR(ViewDesc.Format.ValueType != VT_UNDEFINED, "Undefined format");
            ViewCI.format = TypeToVkFormat(ViewDesc.Format.ValueType, ViewDesc.Format.NumComponents, ViewDesc.Format.IsNormalized);
        }
        ViewCI.offset = ViewDesc.ByteOffset; // offset in bytes from the base address of the buffer
        ViewCI.range  = ViewDesc.ByteWidth; // size in bytes of the buffer view

        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
        BuffView = LogicalDevice.CreateBufferView(ViewCI, ViewDesc.Name);
    }
    return BuffView;
}

VkBuffer BufferVkImpl::GetVkBuffer()const
{
    if (m_VulkanBuffer != VK_NULL_HANDLE)
        return m_VulkanBuffer;
    else
    {
        VERIFY(m_Desc.Usage == USAGE_DYNAMIC, "Dynamic buffer expected");
        return m_pDevice->GetDynamicMemoryManager().GetVkBuffer();
    }
}

void BufferVkImpl::SetAccessFlags(VkAccessFlags AccessFlags)
{
    SetState(VkAccessFlagsToResourceStates(AccessFlags));
}

VkAccessFlags BufferVkImpl::GetAccessFlags()const
{
    return ResourceStateFlagsToVkAccessFlags(GetState());
}

#ifdef DEVELOPMENT
void BufferVkImpl::DvpVerifyDynamicAllocation(DeviceContextVkImpl* pCtx)const
{
    auto ContextId = pCtx->GetContextId();
    const auto& DynAlloc = m_DynamicAllocations[ContextId];
    auto CurrentFrame = pCtx->GetContextFrameNumber();
    DEV_CHECK_ERR(DynAlloc.pDynamicMemMgr != nullptr, "Dynamic buffer '", m_Desc.Name, "' has not been mapped before its first use. Context Id: ", ContextId, ". Note: memory for dynamic buffers is allocated when a buffer is mapped.");
    DEV_CHECK_ERR(DynAlloc.dvpFrameNumber == CurrentFrame, "Dynamic allocation of dynamic buffer '", m_Desc.Name, "' in frame ", CurrentFrame, " is out-of-date. Note: contents of all dynamic resources is discarded at the end of every frame. A buffer must be mapped before its first use in any frame.");
}
#endif

}
