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

#include "RenderDeviceVk.h"
#include "RenderDeviceBase.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "DynamicUploadHeap.h"
#include "Atomics.h"
#include "CommandQueueVk.h"
#include "VulkanUtilities/VulkanInstance.h"
#include "VulkanUtilities/VulkanPhysicalDevice.h"

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
                        VkDevice vkLogicalDevice,
                        Uint32 NumDeferredContexts );
    ~RenderDeviceVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)override final;
    
    void CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, class TextureVkImpl **ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)override final;

    //virtual IVkDevice* GetVkDevice()override final{return m_pVkDevice;}
    
    //virtual void CreateTextureFromD3DResource(IVkResource *pVkTexture, ITexture **ppTexture)override final;

    //virtual void CreateBufferFromD3DResource(IVkResource *pVkBuffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)override final;

/*
    DescriptorHeapAllocation AllocateDescriptor( Vk_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );
    DescriptorHeapAllocation AllocateGPUDescriptors( Vk_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );

    Uint64 GetCompletedFenceValue();
	virtual Uint64 GetNextFenceValue() override final
    {
        return m_pCommandQueue->GetNextFenceValue();
    }

	Uint64 GetCurrentFrameNumber()const {return static_cast<Uint64>(m_FrameNumber);}
    virtual Bool IsFenceSignaled(Uint64 FenceValue) override final;
    */
    ICommandQueueVk *GetCmdQueue(){return m_pCommandQueue;}
    /*
	void IdleGPU(bool ReleaseStaleObjects);
    CommandContext* AllocateCommandContext(const Char *ID = "");
    void CloseAndExecuteCommandContext(CommandContext *pCtx, bool DiscardStaleObjects);
    void DisposeCommandContext(CommandContext*);
*/
    void SafeReleaseVkBuffer(VkBuffer vkBuffer);
    
/*
    void FinishFrame(bool ReleaseAllResources);
    virtual void FinishFrame()override final { FinishFrame(false); }

    DynamicUploadHeap* RequestUploadHeap();
    void ReleaseUploadHeap(DynamicUploadHeap* pUploadHeap);
    */
    

    std::shared_ptr<VulkanUtilities::VulkanInstance> GetVulkanInstance(){return m_VulkanInstance;}
    VulkanUtilities::VulkanPhysicalDevice &GetPhysicalDevice(){return *m_PhysicalDevice;}
    VkDevice GetVkDevice()const{return m_VkDevice;}

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat );
#if 0
    void ProcessReleaseQueue(Uint64 CompletedFenceValue);
    void DiscardStaleVkObjects(Uint64 CmdListNumber, Uint64 FenceValue);
#endif
    std::shared_ptr<VulkanUtilities::VulkanInstance> m_VulkanInstance;
    std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> m_PhysicalDevice;

    /// Vk device
    VkDevice m_VkDevice = VK_NULL_HANDLE;
    RefCntAutoPtr<ICommandQueueVk> m_pCommandQueue;

    EngineVkAttribs m_EngineAttribs;
#if 0
    CPUDescriptorHeap m_CPUDescriptorHeaps[Vk_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    GPUDescriptorHeap m_GPUDescriptorHeaps[2]; // Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == 0
                                               // Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER	 == 1
	
	const Uint32 m_DynamicDescriptorAllocationChunkSize[2];

	std::mutex m_CmdQueueMutex;

	Atomics::AtomicInt64 m_FrameNumber;
    Atomics::AtomicInt64 m_NextCmdListNumber;

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
	std::mutex m_ContextAllocationMutex;

    std::mutex m_ReleaseQueueMutex;
    typedef std::pair<Uint64, CComPtr<IVkObject> > ReleaseQueueElemType;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_VkObjReleaseQueue;
#endif
    std::mutex m_StaleObjectsMutex;
#if 0
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_StaleVkObjects;

    std::mutex m_UploadHeapMutex;
    typedef std::unique_ptr<DynamicUploadHeap, STDDeleterRawMem<DynamicUploadHeap> > UploadHeapPoolElemType;
    std::vector< UploadHeapPoolElemType, STDAllocatorRawMem<UploadHeapPoolElemType> > m_UploadHeaps;
#endif
};

}
