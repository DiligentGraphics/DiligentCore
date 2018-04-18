/*     Copyright 2015-2018 Egor Yusov
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
#include "RenderDeviceVkImpl.h"
#include "PipelineStateVkImpl.h"
#include "ShaderVkImpl.h"
#include "TextureVkImpl.h"
#include "VulkanTypeConversions.h"
#include "SamplerVkImpl.h"
#include "BufferVkImpl.h"
#include "ShaderResourceBindingVkImpl.h"
#include "DeviceContextVkImpl.h"

#include "EngineMemory.h"
namespace Diligent
{

RenderDeviceVkImpl :: RenderDeviceVkImpl(IReferenceCounters *pRefCounters, 
                                         IMemoryAllocator &RawMemAllocator, 
                                         const EngineVkAttribs &CreationAttribs, 
                                         ICommandQueueVk *pCmdQueue,
                                         std::shared_ptr<VulkanUtilities::VulkanInstance> Instance,
                                         std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                         std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                                         Uint32 NumDeferredContexts) : 
    TRenderDeviceBase(pRefCounters, RawMemAllocator, NumDeferredContexts, sizeof(TextureVkImpl), sizeof(TextureViewVkImpl), sizeof(BufferVkImpl), sizeof(BufferViewVkImpl), sizeof(ShaderVkImpl), sizeof(SamplerVkImpl), sizeof(PipelineStateVkImpl), sizeof(ShaderResourceBindingVkImpl)),
    m_VulkanInstance(Instance),
    m_PhysicalDevice(std::move(PhysicalDevice)),
    m_LogicalVkDevice(LogicalDevice),
    m_pCommandQueue(pCmdQueue),
    m_EngineAttribs(CreationAttribs),
	m_FrameNumber(0),
    m_NextCmdListNumber(0),
    /*m_CmdListManager(this),
    m_CPUDescriptorHeaps
    {
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[0], Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Vk_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[1], Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER,     Vk_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[2], Vk_DESCRIPTOR_HEAP_TYPE_RTV,         Vk_DESCRIPTOR_HEAP_FLAG_NONE},
        {RawMemAllocator, this, CreationAttribs.CPUDescriptorHeapAllocationSize[3], Vk_DESCRIPTOR_HEAP_TYPE_DSV,         Vk_DESCRIPTOR_HEAP_FLAG_NONE}
    },
    m_GPUDescriptorHeaps
    {
        {RawMemAllocator, this, CreationAttribs.GPUDescriptorHeapSize[0], CreationAttribs.GPUDescriptorHeapDynamicSize[0], Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Vk_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
        {RawMemAllocator, this, CreationAttribs.GPUDescriptorHeapSize[1], CreationAttribs.GPUDescriptorHeapDynamicSize[1], Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER,     Vk_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE}
    },
	m_DynamicDescriptorAllocationChunkSize
	{
		CreationAttribs.DynamicDescriptorAllocationChunkSize[0], // Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		CreationAttribs.DynamicDescriptorAllocationChunkSize[1]  // Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER
	},
    m_ContextPool(STD_ALLOCATOR_RAW_MEM(ContextPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<CommandContext>>")),
    m_AvailableContexts(STD_ALLOCATOR_RAW_MEM(CommandContext*, GetRawAllocator(), "Allocator for vector<CommandContext*>")),*/
    m_VkObjReleaseQueue(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, GetRawAllocator(), "Allocator for queue<ReleaseQueueElemType>")),
    m_StaleVkObjects(STD_ALLOCATOR_RAW_MEM(ReleaseQueueElemType, GetRawAllocator(), "Allocator for queue<ReleaseQueueElemType>")),/*,
    m_UploadHeaps(STD_ALLOCATOR_RAW_MEM(UploadHeapPoolElemType, GetRawAllocator(), "Allocator for vector<unique_ptr<DynamicUploadHeap>>"))
    */
    m_CmdBufferPool(m_LogicalVkDevice->GetSharedPtr(), pCmdQueue->GetQueueFamilyIndex(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
    m_FramebufferCache(*this)
{
    m_DeviceCaps.DevType = DeviceType::Vulkan;
    m_DeviceCaps.MajorVersion = 1;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;
    for(int fmt = 1; fmt < m_TextureFormatsInfo.size(); ++fmt)
        m_TextureFormatsInfo[fmt].Supported = true; // We will test every format on a specific hardware device
}

RenderDeviceVkImpl::~RenderDeviceVkImpl()
{
	// Finish current frame. This will release resources taken by previous frames, and
    // will move all stale resources to the release queues. The resources will not be
    // release until next call to FinishFrame()
    FinishFrame(false);
    // Wait for the GPU to complete all its operations
    IdleGPU(true);
    // Call FinishFrame() again to destroy resources in
    // release queues
    FinishFrame(true);

#if 0
	m_ContextPool.clear();
#endif

    VERIFY(m_StaleVkObjects.empty(), "Not all stale objects were destroyed");
    VERIFY(m_VkObjReleaseQueue.empty(), "Release queue is not empty");
    
    //if(m_PhysicalDevice)
    //{
    //    // If m_PhysicalDevice is empty, the device does not own vulkan logical device and must not
    //    // destroy it
    //    vkDestroyDevice(m_VkDevice, m_VulkanInstance->GetVkAllocator());
    //}
}


void RenderDeviceVkImpl::DisposeCommandBuffer(VkCommandBuffer CmdBuff)
{
	std::lock_guard<std::mutex> LockGuard(m_CmdPoolMutex);
    m_CmdBufferPool.DisposeCommandBuffer(CmdBuff, GetNextFenceValue());
}


void RenderDeviceVkImpl::ExecuteCommandBuffer(VkCommandBuffer CmdBuff, bool DiscardStaleObjects)
{
    VERIFY_EXPR(CmdBuff != VK_NULL_HANDLE);
    auto err = vkEndCommandBuffer(CmdBuff);
    VERIFY(err == VK_SUCCESS, "Failed to end command buffer");

    Uint64 FenceValue = 0;
    Uint64 CmdListNumber = 0;
    {
	    std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        auto NextFenceValue = m_pCommandQueue->GetNextFenceValue();
	    // Submit the command list to the queue
        FenceValue = m_pCommandQueue->ExecuteCommandBuffer(CmdBuff);
        VERIFY(FenceValue >= NextFenceValue, "Fence value of the executed command list is less than the next fence value previously queried through GetNextFenceValue()");
        FenceValue = std::max(FenceValue, NextFenceValue);
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }

    if (DiscardStaleObjects)
    {
        // The following basic requirement guarantees correctness of resource deallocation:
        //
        //        A resource is never released before the last draw command referencing it is invoked on the immediate context
        //
        // See http://diligentgraphics.com/diligent-engine/architecture/Vk/managing-resource-lifetimes/

        // Stale objects should only be discarded when submitting cmd list from 
        // the immediate context, otherwise the basic requirement may be violated
        // as in the following scenario
        //                                                           
        //  Signaled        |                                        |
        //  Fence Value     |        Immediate Context               |            InitContext            |
        //                  |                                        |                                   |
        //    N             |  Draw(ResourceX)                       |                                   |
        //                  |  Release(ResourceX)                    |                                   |
        //                  |   - (ResourceX, N) -> Release Queue    |                                   |
        //                  |                                        | CopyResource()                    |
        //   N+1            |                                        | CloseAndExecuteCommandContext()   |
        //                  |                                        |                                   |
        //   N+2            |  CloseAndExecuteCommandContext()       |                                   |
        //                  |   - Cmd list is submitted with number  |                                   |
        //                  |     N+1, but resource it references    |                                   |
        //                  |     was added to the delete queue      |                                   |
        //                  |     with number N                      |                                   |

        // Move stale objects into a release queue.
        // Note that objects are moved from stale list to release queue based on the
        // cmd list number, not the fence value. This makes sure that basic requirement
        // is met even when the fence value is not incremented while executing 
        // the command list (as is the case with Unity command queue).
        DiscardStaleVkObjects(CmdListNumber, FenceValue);
    }
#if 0
    // DiscardAllocator() is thread-safe
	m_CmdListManager.DiscardAllocator(FenceValue, pAllocator);
    
    pCtx->DiscardDynamicDescriptors(FenceValue);

    {
	    std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
    	m_AvailableContexts.push_back(pCtx);
    }
#endif
}


void RenderDeviceVkImpl::IdleGPU(bool ReleaseStaleObjects) 
{ 
    Uint64 FenceValue = 0;
    Uint64 CmdListNumber = 0;

    {
        // Lock the command queue to avoid other threads interfering with the GPU
        std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        FenceValue = m_pCommandQueue->GetNextFenceValue();
        m_pCommandQueue->IdleGPU();

        m_LogicalVkDevice->WaitIdle();

        // Increment cmd list number while keeping queue locked. 
        // This guarantees that any Vk object released after the lock
        // is released, will be associated with the incremented cmd list number
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }

    if (ReleaseStaleObjects)
    {
        // Do not wait until the end of the frame and force deletion. 
        // This is necessary to release outstanding references to the
        // swap chain buffers when it is resized in the middle of the frame.
        // Since GPU has been idled, it it is safe to do so
        DiscardStaleVkObjects(CmdListNumber, FenceValue);
        ProcessReleaseQueue(FenceValue);
    }
}


Bool RenderDeviceVkImpl::IsFenceSignaled(Uint64 FenceValue) 
{
    return FenceValue <= GetCompletedFenceValue();
}

Uint64 RenderDeviceVkImpl::GetCompletedFenceValue()
{
    return m_pCommandQueue->GetCompletedFenceValue();
}


void RenderDeviceVkImpl::FinishFrame(bool ReleaseAllResources)
{
    {
        if (auto pImmediateCtx = m_wpImmediateContext.Lock())
        {
            auto pImmediateCtxVk = pImmediateCtx.RawPtr<DeviceContextVkImpl>();
            if(pImmediateCtxVk->GetNumCommandsInCtx() != 0)
                LOG_ERROR_MESSAGE("There are outstanding commands in the immediate device context when finishing the frame. This is an error and may cause unpredicted behaviour. Call Flush() to submit all commands for execution before finishing the frame");
        }

        for (auto wpDeferredCtx : m_wpDeferredContexts)
        {
            if (auto pDeferredCtx = wpDeferredCtx.Lock())
            {
                auto pDeferredCtxVk = pDeferredCtx.RawPtr<DeviceContextVkImpl>();
                if(pDeferredCtxVk->GetNumCommandsInCtx() != 0)
                    LOG_ERROR_MESSAGE("There are outstanding commands in the deferred device context when finishing the frame. This is an error and may cause unpredicted behaviour. Close all deferred contexts and execute them before finishing the frame");
            }
        }
    }

    auto CompletedFenceValue = ReleaseAllResources ? std::numeric_limits<Uint64>::max() : GetCompletedFenceValue();

    // We must use NextFenceValue here, NOT current value, because the 
    // fence value may or may not have been incremented when the last 
    // command list was submitted for execution (Unity only
    // increments fence value once per frame)
    Uint64 NextFenceValue = 0;
    Uint64 CmdListNumber = 0;
    {
        // Lock the command queue to avoid other threads interfering with the GPU
        std::lock_guard<std::mutex> LockGuard(m_CmdQueueMutex);
        NextFenceValue = m_pCommandQueue->GetNextFenceValue();
        // Increment cmd list number while keeping queue locked. 
        // This guarantees that any Vk object released after the lock
        // is released, will be associated with the incremented cmd list number
        CmdListNumber = m_NextCmdListNumber;
        Atomics::AtomicIncrement(m_NextCmdListNumber);
    }

#if 0
    {
        // There is no need to lock as new heaps are only created during initialization
        // time for every context
        //std::lock_guard<std::mutex> LockGuard(m_UploadHeapMutex);
        
        // Upload heaps are used to update resource contents as well as to allocate
        // space for dynamic resources.
        // Initial resource data is uploaded using temporary one-time upload buffers, 
        // so can be performed in parallel across frame boundaries
        for (auto &UploadHeap : m_UploadHeaps)
        {
            // Currently upload heaps are free-threaded, so other threads must not allocate
            // resources at the same time. This means that all dynamic buffers must be unmaped 
            // in the same frame and all resources must be updated within boundaries of a single frame.
            //
            //    worker thread 3    | pDevice->CrateTexture(InitData) |    | pDevice->CrateBuffer(InitData) |    | pDevice->CrateTexture(InitData) |
            //                                                                                               
            //    worker thread 2     | pDfrdCtx2->UpdateResource()  |                                              ||
            //                                                                                                      ||
            //    worker thread 1       |  pDfrdCtx1->Map(WRITE_DISCARD) |    | pDfrdCtx1->UpdateResource()  |      ||
            //                                                                                                      ||
            //    main thread        |  pCtx->Map(WRITE_DISCARD )|  | pCtx->UpdateResource()  |                     ||   | Present() |
            //
            //
            
            UploadHeap->FinishFrame(NextFenceValue, CompletedFenceValue);
        }
    }

    for(Uint32 CPUHeap=0; CPUHeap < _countof(m_CPUDescriptorHeaps); ++CPUHeap)
    {
        // This is OK if other thread disposes descriptor heap allocation at this time
        // The allocation will be registered as part of the current frame
        m_CPUDescriptorHeaps[CPUHeap].ReleaseStaleAllocations(CompletedFenceValue);
    }
        
    for(Uint32 GPUHeap=0; GPUHeap < _countof(m_GPUDescriptorHeaps); ++GPUHeap)
    {
        m_GPUDescriptorHeaps[GPUHeap].ReleaseStaleAllocations(CompletedFenceValue);
    }
#endif
    // Discard all remaining objects. This is important to do if there were 
    // no command lists submitted during the frame
    DiscardStaleVkObjects(CmdListNumber, NextFenceValue);
    ProcessReleaseQueue(CompletedFenceValue);

    Atomics::AtomicIncrement(m_FrameNumber);
}

#if 0
DynamicUploadHeap* RenderDeviceVkImpl::RequestUploadHeap()
{
    std::lock_guard<std::mutex> LockGuard(m_UploadHeapMutex);

#ifdef _DEBUG
    size_t InitialSize = 1024+64;
#else
    size_t InitialSize = 64<<10;//16<<20;
#endif

    auto &UploadHeapAllocator = GetRawAllocator();
    auto *pRawMem = ALLOCATE(UploadHeapAllocator, "DynamicUploadHeap instance", sizeof(DynamicUploadHeap));
    auto *pNewHeap = new (pRawMem) DynamicUploadHeap(GetRawAllocator(), true, this, InitialSize);
    m_UploadHeaps.emplace_back( pNewHeap, STDDeleterRawMem<DynamicUploadHeap>(UploadHeapAllocator) );
    return pNewHeap;
}

void RenderDeviceVkImpl::ReleaseUploadHeap(DynamicUploadHeap* pUploadHeap)
{

}
#endif

VkCommandBuffer RenderDeviceVkImpl::AllocateCommandBuffer(const Char *DebugName)
{
	std::lock_guard<std::mutex> LockGuard(m_CmdPoolMutex);
    auto CmdBuffer = m_CmdBufferPool.GetCommandBuffer(GetCompletedFenceValue(), DebugName);
    return CmdBuffer;
}


template<typename VulkanObjectType>
class RenderDeviceVkImpl::StaleVulkanObject : public StaleVulkanObjectBase
{
public:
    StaleVulkanObject(VulkanUtilities::VulkanObjectWrapper<VulkanObjectType> &&Object) :
        m_VkObject(std::move(Object))
    {}

    ~StaleVulkanObject()
    {
        m_VkObject.Release();
    }

private:
    VulkanUtilities::VulkanObjectWrapper<VulkanObjectType> m_VkObject;
};

template<typename VulkanObjectType>
void RenderDeviceVkImpl::SafeReleaseVkObject(VulkanUtilities::VulkanObjectWrapper<VulkanObjectType>&& vkObject)
{
    // When Vk object is released, it is first moved into the
    // stale objects list. The list is moved into a release queue
    // when the next command list is executed. 
    std::lock_guard<std::mutex> LockGuard(m_StaleObjectsMutex);
    m_StaleVkObjects.emplace_back(m_NextCmdListNumber, new StaleVulkanObject<VulkanObjectType>(std::move(vkObject)) );
}

template void RenderDeviceVkImpl::SafeReleaseVkObject<VkBuffer>       (VulkanUtilities::BufferWrapper       &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkBufferView>   (VulkanUtilities::BufferViewWrapper   &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkImage>        (VulkanUtilities::ImageWrapper        &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkImageView>    (VulkanUtilities::ImageViewWrapper    &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkSampler>      (VulkanUtilities::SamplerWrapper      &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkDeviceMemory> (VulkanUtilities::DeviceMemoryWrapper &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkRenderPass>   (VulkanUtilities::RenderPassWrapper   &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkPipeline>     (VulkanUtilities::PipelineWrapper     &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkShaderModule> (VulkanUtilities::ShaderModuleWrapper &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkPipelineLayout>(VulkanUtilities::PipelineLayoutWrapper &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkFramebuffer>   (VulkanUtilities::FramebufferWrapper    &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkDescriptorPool> (VulkanUtilities::DescriptorPoolWrapper &&Object);
template void RenderDeviceVkImpl::SafeReleaseVkObject<VkDescriptorSetLayout>(VulkanUtilities::DescriptorSetLayoutWrapper &&Object);


void RenderDeviceVkImpl::DiscardStaleVkObjects(Uint64 CmdListNumber, Uint64 FenceValue)
{
    // Only discard these stale objects that were released before CmdListNumber
    // was executed
    std::lock_guard<std::mutex> StaleObjectsLock(m_StaleObjectsMutex);
    std::lock_guard<std::mutex> ReleaseQueueLock(m_ReleaseQueueMutex);
    while (!m_StaleVkObjects.empty() )
    {
        auto &FirstStaleObj = m_StaleVkObjects.front();
        if (FirstStaleObj.first <= CmdListNumber)
        {
            m_VkObjReleaseQueue.emplace_back(FenceValue, std::move(FirstStaleObj.second));
            m_StaleVkObjects.pop_front();
        }
        else 
            break;
    }
}

void RenderDeviceVkImpl::ProcessReleaseQueue(Uint64 CompletedFenceValue)
{
    std::lock_guard<std::mutex> LockGuard(m_ReleaseQueueMutex);

    // Release all objects whose associated fence value is at most CompletedFenceValue
    // See http://diligentgraphics.com/diligent-engine/architecture/Vk/managing-resource-lifetimes/
    while (!m_VkObjReleaseQueue.empty())
    {
        auto &FirstObj = m_VkObjReleaseQueue.front();
        if (FirstObj.first <= CompletedFenceValue)
            m_VkObjReleaseQueue.pop_front();
        else
            break;
    }
}


void RenderDeviceVkImpl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto vkPhysicalDevice = m_PhysicalDevice->GetVkDeviceHandle();

    auto SRVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_SHADER_RESOURCE, BIND_SHADER_RESOURCE);
    auto RTVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_RENDER_TARGET, BIND_RENDER_TARGET);
    auto DSVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_DEPTH_STENCIL, BIND_DEPTH_STENCIL);
    
    if(SRVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat vkSrvFormat = TexFormatToVkFormat(SRVFormat);
        VkFormatProperties vkSrvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkSrvFormat, &vkSrvFmtProps);

        if(vkSrvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        {
            TexFormatInfo.Filterable = true;

            {
                VkImageFormatProperties ImgFmtProps = {};
                auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex1DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};
                auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex2DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};
                auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex3DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};
                auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &ImgFmtProps);
                TexFormatInfo.TexCubeFmt = err == VK_SUCCESS;
            }

        }
    }

    if (RTVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat vkRtvFormat = TexFormatToVkFormat(RTVFormat);
        VkFormatProperties vkRtvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkRtvFormat, &vkRtvFmtProps);
        if (vkRtvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        {
            VkImageFormatProperties ImgFmtProps = {};
            auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkRtvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &ImgFmtProps);
            TexFormatInfo.ColorRenderable = err == VK_SUCCESS;
            if (TexFormatInfo.ColorRenderable)
            {
                TexFormatInfo.SupportsMS = ImgFmtProps.sampleCounts > VK_SAMPLE_COUNT_1_BIT;
            }
        }
    }

    if (DSVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat vkDsvFormat = TexFormatToVkFormat(DSVFormat);
        VkFormatProperties vkDsvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkDsvFormat, &vkDsvFmtProps);
        if (vkDsvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            VkImageFormatProperties ImgFmtProps = {};
            auto err = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkDsvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &ImgFmtProps);
            TexFormatInfo.DepthRenderable = err == VK_SUCCESS;
            if (TexFormatInfo.DepthRenderable)
            {
                TexFormatInfo.SupportsMS = ImgFmtProps.sampleCounts > VK_SAMPLE_COUNT_1_BIT;
            }
        }
    }
}


IMPLEMENT_QUERY_INTERFACE( RenderDeviceVkImpl, IID_RenderDeviceVk, TRenderDeviceBase )

void RenderDeviceVkImpl::CreatePipelineState(const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState)
{
    CreateDeviceObject("Pipeline State", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateVkImpl *pPipelineStateVk( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateVkImpl instance", PipelineStateVkImpl)(this, PipelineDesc ) );
            pPipelineStateVk->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateVk );
        } 
    );
}

#if 0
void RenderDeviceVkImpl :: CreateBufferFromD3DResource(IVkResource *pVkBuffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferVkImpl *pBufferVk( NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, pVkBuffer ) );
            pBufferVk->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferVk->CreateDefaultViews();
            OnCreateDeviceObject( pBufferVk );
        } 
    );
}
#endif

void RenderDeviceVkImpl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferVkImpl *pBufferVk( NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, BuffData ) );
            pBufferVk->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferVk->CreateDefaultViews();
            OnCreateDeviceObject( pBufferVk );
        } 
    );
}


void RenderDeviceVkImpl :: CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)
{
    CreateDeviceObject( "shader", ShaderCreationAttribs.Desc, ppShader, 
        [&]()
        {
            ShaderVkImpl *pShaderVk( NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderVkImpl instance", ShaderVkImpl)(this, ShaderCreationAttribs ) );
            pShaderVk->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderVk );
        } 
    );
}

#if 0
void RenderDeviceVkImpl::CreateTextureFromD3DResource(IVkResource *pVkTexture, ITexture **ppTexture)
{
    TextureDesc TexDesc;
    TexDesc.Name = "Texture from Vk resource";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl *pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, pVkTexture );

            pTextureVk->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureVk->CreateDefaultViews();
            OnCreateDeviceObject( pTextureVk );
        } 
    );
}
#endif

void RenderDeviceVkImpl::CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, class TextureVkImpl **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl *pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, std::move(vkImgHandle));
            pTextureVk->QueryInterface( IID_TextureVk, reinterpret_cast<IObject**>(ppTexture) );
        }
    );
}


void RenderDeviceVkImpl :: CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl *pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, Data );

            pTextureVk->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureVk->CreateDefaultViews();
            OnCreateDeviceObject( pTextureVk );
        } 
    );
}

void RenderDeviceVkImpl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if( *ppSampler == nullptr )
            {
                SamplerVkImpl *pSamplerVk( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerVkImpl instance", SamplerVkImpl)(this, SamplerDesc ) );
                pSamplerVk->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerVk );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

#if 0
DescriptorHeapAllocation RenderDeviceVkImpl :: AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE Type, UINT Count /*= 1*/)
{
    VERIFY(Type >= Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type < Vk_DESCRIPTOR_HEAP_TYPE_NUM_TYPES, "Invalid heap type");
    return m_CPUDescriptorHeaps[Type].Allocate(Count);
}

DescriptorHeapAllocation RenderDeviceVkImpl :: AllocateGPUDescriptors(Vk_DESCRIPTOR_HEAP_TYPE Type, UINT Count /*= 1*/)
{
    VERIFY(Type >= Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type <= Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Invalid heap type");
    return m_GPUDescriptorHeaps[Type].Allocate(Count);
}
#endif

}
