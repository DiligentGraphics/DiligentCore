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

/// Namespace for the Direct3D11 implementation of the graphics engine
namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceVk interface
class RenderDeviceVkImpl : public RenderDeviceBase<IRenderDeviceVk>
{
public:
    typedef RenderDeviceBase<IRenderDeviceVk> TRenderDeviceBase;

    RenderDeviceVkImpl( IReferenceCounters *pRefCounters, 
                        IMemoryAllocator &RawMemAllocator, 
                        const EngineVkAttribs &CreationAttribs, 
                        ICommandQueueVk *pCmdQueue, 
                        std::shared_ptr<VulkanUtilities::VulkanInstance> Instance,
                        std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                        std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                        Uint32 NumDeferredContexts );
    ~RenderDeviceVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)override final;
    
    void CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, class TextureVkImpl **ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)override final;

    virtual VkDevice GetVkDevice()override final{ return m_LogicalVkDevice->GetVkDevice();}
    
    //virtual void CreateTextureFromD3DResource(IVkResource *pVkTexture, ITexture **ppTexture)override final;

    //virtual void CreateBufferFromD3DResource(IVkResource *pVkBuffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)override final;

    Uint64 GetCompletedFenceValue();
	virtual Uint64 GetNextFenceValue() override final
    {
        return m_pCommandQueue->GetNextFenceValue();
    }

	Uint64 GetCurrentFrameNumber()const {return static_cast<Uint64>(m_FrameNumber);}
    virtual Bool IsFenceSignaled(Uint64 FenceValue) override final;

    ICommandQueueVk *GetCmdQueue(){return m_pCommandQueue;}
    
	void IdleGPU(bool ReleaseStaleObjects);
    void ExecuteCommandBuffer(const VkSubmitInfo &SubmitInfo, bool DiscardStaleObjects);
    void ExecuteCommandBuffer(VkCommandBuffer CmdBuff, bool DiscardStaleObjects);
    
    void AllocateTransientCmdPool(VulkanUtilities::CommandPoolWrapper& CmdPool, VkCommandBuffer& vkCmdBuff, const Char* DebugPoolName = nullptr);
    void DisposeTransientCmdPool(VulkanUtilities::CommandPoolWrapper&& CmdPool);


    template<typename ObjectType>
    void SafeReleaseVkObject(ObjectType&& Object);
    
    void FinishFrame(bool ReleaseAllResources);
    virtual void FinishFrame()override final { FinishFrame(false); }

    DescriptorPoolAllocation AllocateDescriptorSet(VkDescriptorSetLayout SetLayout)
    {
        return m_DescriptorPools[0].Allocate(SetLayout);
    }
    DescriptorPoolAllocation AllocateDynamicDescriptorSet(VkDescriptorSetLayout SetLayout, Uint32 CtxId)
    {
        // Descriptor pools are externally synchronized, meaning that the application must not allocate 
        // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
        return m_DescriptorPools[1 + CtxId].Allocate(SetLayout);
    }

    VulkanUtilities::VulkanUploadAllocation AllocateUploadSpace(Uint32 CtxId, size_t Size)
    {
        return m_UploadHeaps[CtxId].Allocate(Size);
    }
    
    std::shared_ptr<const VulkanUtilities::VulkanInstance> GetVulkanInstance()const{return m_VulkanInstance;}
    const VulkanUtilities::VulkanPhysicalDevice &GetPhysicalDevice(){return *m_PhysicalDevice;}
    const auto &GetLogicalDevice(){return *m_LogicalVkDevice;}
    FramebufferCache& GetFramebufferCache(){return m_FramebufferCache;}

    VulkanUtilities::VulkanMemoryAllocation AllocateMemory(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProperties)
    {
        return m_MemoryMgr.Allocate(MemReqs, MemoryProperties);
    }

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;
    void ProcessReleaseQueue(Uint64 CompletedFenceValue);
    void DiscardStaleVkObjects(Uint64 CmdListNumber, Uint64 FenceValue);

    std::shared_ptr<VulkanUtilities::VulkanInstance> m_VulkanInstance;
    std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> m_PhysicalDevice;
    std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> m_LogicalVkDevice;
    
    std::mutex m_CmdQueueMutex;
    RefCntAutoPtr<ICommandQueueVk> m_pCommandQueue;

    EngineVkAttribs m_EngineAttribs;

	Atomics::AtomicInt64 m_FrameNumber;
    Atomics::AtomicInt64 m_NextCmdListNumber;
#if 0
    // The following basic requirement guarantees correctness of resource deallocation:
    //
    //        A resource is never released before the last draw command referencing it is invoked on the immediate context
    //
    // See http://diligentgraphics.com/diligent-engine/architecture/Vk/managing-resource-lifetimes/

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

    CommandListManager m_CmdListManager;

    typedef std::unique_ptr<CommandContext, STDDeleterRawMem<CommandContext> > ContextPoolElemType;
	std::vector< ContextPoolElemType, STDAllocatorRawMem<ContextPoolElemType> > m_ContextPool;
    std::deque<CommandContext*, STDAllocatorRawMem<CommandContext*> > m_AvailableContexts;
#endif
	
    std::mutex m_ReleaseQueueMutex;

    class StaleVulkanObjectBase
    {
    public:
        virtual ~StaleVulkanObjectBase() = 0 {}
    };

    using ReleaseQueueElemType = std::pair<Uint64, std::unique_ptr<StaleVulkanObjectBase> >;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_VkObjReleaseQueue;

    std::mutex m_StaleObjectsMutex;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_StaleVkObjects;
    FramebufferCache m_FramebufferCache;

    // [0] - Main descriptor pool
    // [1] - Immediate context dynamic descriptor pool
    // [2+] - Deferred context dynamic descriptor pool
    std::vector<DescriptorPoolManager, STDAllocatorRawMem<DescriptorPoolManager> > m_DescriptorPools;

    std::vector<VulkanUtilities::VulkanUploadHeap, STDAllocatorRawMem<VulkanUtilities::VulkanUploadHeap>> m_UploadHeaps;

    // These one-time command pools are used by buffer and texture constructors to
    // issue copy commands. Vulkan requires that every command pool is used by one thread 
    // at a time, so every constructor must allocate command buffer from its own pool.
    CommandPoolManager m_TransientCmdPoolMgr;

    VulkanUtilities::VulkanMemoryManager m_MemoryMgr;
};

}
