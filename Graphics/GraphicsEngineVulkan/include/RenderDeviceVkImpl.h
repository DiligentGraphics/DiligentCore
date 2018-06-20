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

#pragma once

/// \file
/// Declaration of Diligent::RenderDeviceVkImpl class
#include <memory>

#include "RenderDeviceVk.h"
#include "RenderDeviceBase.h"
#include "DescriptorPoolManager.h"
#include "CommandContext.h"
#include "VulkanDynamicHeap.h"
#include "Atomics.h"
#include "CommandQueueVk.h"
#include "VulkanUtilities/VulkanInstance.h"
#include "VulkanUtilities/VulkanPhysicalDevice.h"
#include "VulkanUtilities/VulkanCommandBufferPool.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "VulkanUtilities/VulkanMemoryManager.h"
#include "VulkanUtilities/VulkanUploadHeap.h"
#include "FramebufferCache.h"
#include "CommandPoolManager.h"
#include "ResourceReleaseQueue.h"
#include "VulkanDynamicHeap.h"

/// Namespace for the Direct3D11 implementation of the graphics engine
namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceVk interface
class RenderDeviceVkImpl : public RenderDeviceBase<IRenderDeviceVk>
{
public:
    typedef RenderDeviceBase<IRenderDeviceVk> TRenderDeviceBase;

    RenderDeviceVkImpl( IReferenceCounters*     pRefCounters, 
                        IMemoryAllocator&       RawMemAllocator, 
                        const EngineVkAttribs&  CreationAttribs, 
                        ICommandQueueVk*        pCmdQueue, 
                        std::shared_ptr<VulkanUtilities::VulkanInstance>        Instance,
                        std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice>  PhysicalDevice,
                        std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>   LogicalDevice,
                        Uint32                  NumDeferredContexts );
    ~RenderDeviceVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject** ppInterface )override final;

    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState** ppPipelineState )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData& BuffData, IBuffer** ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader** ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData& Data, ITexture** ppTexture)override final;
    
    void CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, class TextureVkImpl** ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)override final;

    virtual VkDevice GetVkDevice()override final{ return m_LogicalVkDevice->GetVkDevice();}
    
    //virtual void CreateTextureFromD3DResource(IVkResource* pVkTexture, ITexture** ppTexture)override final;

    //virtual void CreateBufferFromD3DResource(IVkResource* pVkBuffer, const BufferDesc& BuffDesc, IBuffer** ppBuffer)override final;

    Uint64 GetCompletedFenceValue();
	virtual Uint64 GetNextFenceValue() override final
    {
        return m_pCommandQueue->GetNextFenceValue();
    }

	Uint64 GetCurrentFrameNumber()const {return static_cast<Uint64>(m_FrameNumber);}
    virtual Bool IsFenceSignaled(Uint64 FenceValue) override final;

    ICommandQueueVk *GetCmdQueue(){return m_pCommandQueue;}
    
    // Idles GPU and returns fence value that was signaled
	Uint64 IdleGPU(bool ReleaseStaleObjects);
    // pImmediateCtx parameter is only used to make sure the command buffer is submitted from the immediate context
    // The method returns fence value associated with the submitted command buffer
    Uint64 ExecuteCommandBuffer(const VkSubmitInfo &SubmitInfo, class DeviceContextVkImpl* pImmediateCtx);

    void AllocateTransientCmdPool(VulkanUtilities::CommandPoolWrapper& CmdPool, VkCommandBuffer& vkCmdBuff, const Char* DebugPoolName = nullptr);
    void ExecuteAndDisposeTransientCmdBuff(VkCommandBuffer vkCmdBuff, VulkanUtilities::CommandPoolWrapper&& CmdPool);

    template<typename ObjectType>
    void SafeReleaseVkObject(ObjectType&& Object)
    {
        m_ReleaseQueue.SafeReleaseResource(std::move(Object), m_NextCmdBuffNumber);
    }
    
    void FinishFrame(bool ReleaseAllResources);
    virtual void FinishFrame()override final { FinishFrame(false); }

    DescriptorPoolAllocation AllocateDescriptorSet(VkDescriptorSetLayout SetLayout)
    {
        return m_MainDescriptorPool.Allocate(SetLayout);
    }

    std::shared_ptr<const VulkanUtilities::VulkanInstance> GetVulkanInstance()const{return m_VulkanInstance;}
    const VulkanUtilities::VulkanPhysicalDevice& GetPhysicalDevice(){return *m_PhysicalDevice;}
    const VulkanUtilities::VulkanLogicalDevice&  GetLogicalDevice() {return *m_LogicalVkDevice;}
    FramebufferCache& GetFramebufferCache(){return m_FramebufferCache;}

    VulkanUtilities::VulkanMemoryAllocation AllocateMemory(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProperties)
    {
        return m_MemoryMgr.Allocate(MemReqs, MemoryProperties);
    }

    VulkanRingBuffer& GetDynamicHeapRingBuffer(){return m_DynamicHeapRingBuffer;}

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;
    void ProcessStaleResources(Uint64 SubmittedCmdBufferNumber, Uint64 SubmittedFenceValue, Uint64 CompletedFenceValue);

    // Submits command buffer for execution to the command queue
    // Returns the submitted command buffer number and the fence value
    // Parameters:
    //      * SubmittedCmdBuffNumber - submitted command buffer number
    //      * SubmittedFenceValue    - fence value associated with the submitted command buffer
    void SubmitCommandBuffer(const VkSubmitInfo& SubmitInfo, Uint64& SubmittedCmdBuffNumber, Uint64& SubmittedFenceValue);

    std::shared_ptr<VulkanUtilities::VulkanInstance>        m_VulkanInstance;
    std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice>  m_PhysicalDevice;
    std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>   m_LogicalVkDevice;
    
    std::mutex                      m_CmdQueueMutex;
    RefCntAutoPtr<ICommandQueueVk>  m_pCommandQueue;

    EngineVkAttribs m_EngineAttribs;

	Atomics::AtomicInt64 m_FrameNumber;
    Atomics::AtomicInt64 m_NextCmdBuffNumber;
    
    // The following basic requirement guarantees correctness of resource deallocation:
    //
    //        A resource is never released before the last draw command referencing it is invoked on the immediate context
    //

    //
    // CPU
    //                       Last Reference
    //                        of resource X
    //                             |
    //                             |     Submit Cmd       Submit Cmd            Submit Cmd
    //                             |      List N           List N+1              List N+2
    //                             V         |                |                     |
    //    NextFenceValue       |   *  N      |      N+1       |          N+2        |
    //
    //
    //    CompletedFenceValue       |     N-3      |      N-2      |        N-1        |        N       |
    //                              .              .               .                   .                .
    // -----------------------------.--------------.---------------.-------------------.----------------.-------------
    //                              .              .               .                   .                .
    //       
    // GPU                          | Cmd List N-2 | Cmd List N-1  |    Cmd List N     |   Cmd List N+1 |
    //                                                                                 |
    //                                                                                 |
    //                                                                          Resource X can
    //                                                                           be released

    FramebufferCache m_FramebufferCache;

    DescriptorPoolManager m_MainDescriptorPool;

    // These one-time command pools are used by buffer and texture constructors to
    // issue copy commands. Vulkan requires that every command pool is used by one thread 
    // at a time, so every constructor must allocate command buffer from its own pool.
    CommandPoolManager m_TransientCmdPoolMgr;

    VulkanUtilities::VulkanMemoryManager m_MemoryMgr;
    ResourceReleaseQueue<DynamicStaleResourceWrapper> m_ReleaseQueue;

    VulkanRingBuffer m_DynamicHeapRingBuffer;
};

}
