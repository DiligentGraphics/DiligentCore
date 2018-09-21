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
#include <sstream>
#include "RenderDeviceVkImpl.h"
#include "PipelineStateVkImpl.h"
#include "ShaderVkImpl.h"
#include "TextureVkImpl.h"
#include "VulkanTypeConversions.h"
#include "SamplerVkImpl.h"
#include "BufferVkImpl.h"
#include "ShaderResourceBindingVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "FenceVkImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceVkImpl :: RenderDeviceVkImpl(IReferenceCounters*                                     pRefCounters, 
                                         IMemoryAllocator&                                       RawMemAllocator, 
                                         const EngineVkAttribs&                                  CreationAttribs, 
                                         ICommandQueueVk*                                        pCmdQueue,
                                         std::shared_ptr<VulkanUtilities::VulkanInstance>        Instance,
                                         std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice>  PhysicalDevice,
                                         std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>   LogicalDevice,
                                         Uint32                                                  NumDeferredContexts) : 
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        1,
        NumDeferredContexts,
        sizeof(TextureVkImpl),
        sizeof(TextureViewVkImpl),
        sizeof(BufferVkImpl),
        sizeof(BufferViewVkImpl),
        sizeof(ShaderVkImpl),
        sizeof(SamplerVkImpl),
        sizeof(PipelineStateVkImpl),
        sizeof(ShaderResourceBindingVkImpl),
        sizeof(FenceVkImpl)
    },
    m_VulkanInstance(Instance),
    m_PhysicalDevice(std::move(PhysicalDevice)),
    m_LogicalVkDevice(std::move(LogicalDevice)),
    m_EngineAttribs(CreationAttribs),
    m_FramebufferCache(*this),
    m_RenderPassCache(*this),
    m_MainDescriptorPool
    {
        m_LogicalVkDevice, 
        std::vector<VkDescriptorPoolSize>
        {
            {VK_DESCRIPTOR_TYPE_SAMPLER,                CreationAttribs.MainDescriptorPoolSize.NumSeparateSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, CreationAttribs.MainDescriptorPoolSize.NumCombinedSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          CreationAttribs.MainDescriptorPoolSize.NumSampledImageDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          CreationAttribs.MainDescriptorPoolSize.NumStorageImageDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   CreationAttribs.MainDescriptorPoolSize.NumUniformTexelBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   CreationAttribs.MainDescriptorPoolSize.NumStorageTexelBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         CreationAttribs.MainDescriptorPoolSize.NumUniformBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         CreationAttribs.MainDescriptorPoolSize.NumStorageBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, CreationAttribs.MainDescriptorPoolSize.NumUniformBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, CreationAttribs.MainDescriptorPoolSize.NumStorageBufferDescriptors},
        },
        CreationAttribs.MainDescriptorPoolSize.MaxDescriptorSets
    },
    m_TransientCmdPoolMgr(*m_LogicalVkDevice, pCmdQueue->GetQueueFamilyIndex(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
    m_MemoryMgr("Global resource memory manager", *m_LogicalVkDevice, *m_PhysicalDevice, GetRawAllocator(), CreationAttribs.DeviceLocalMemoryPageSize, CreationAttribs.HostVisibleMemoryPageSize, CreationAttribs.DeviceLocalMemoryReserveSize, CreationAttribs.HostVisibleMemoryReserveSize),
    m_DynamicMemoryManager
    {
        GetRawAllocator(),
        *this,
        CreationAttribs.DynamicHeapSize,
        ~Uint64{0}
    }
{
    m_CommandQueues[0].CmdQueue = pCmdQueue;

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
    // Explicitly destroy dynamic heap
    m_DynamicMemoryManager.Destroy();
	// Finish current frame. This will release resources taken by previous frames, and
    // will move all stale resources to the release queues. The resources will not be
    // release until the next call to FinishFrame()
    FinishFrame(false);
    // Wait for the GPU to complete all its operations
    IdleGPU(true);
    // Call FinishFrame() again to destroy resources in
    // release queues
    FinishFrame(true);

    m_TransientCmdPoolMgr.DestroyPools(m_CommandQueues[0].CmdQueue->GetCompletedFenceValue());

    // We must destroy command queues explicitly prior to releasing Vulkan device
    DestroyCommandQueues();

    //if(m_PhysicalDevice)
    //{
    //    // If m_PhysicalDevice is empty, the device does not own vulkan logical device and must not
    //    // destroy it
    //    vkDestroyDevice(m_VkDevice, m_VulkanInstance->GetVkAllocator());
    //}
}


void RenderDeviceVkImpl::AllocateTransientCmdPool(VulkanUtilities::CommandPoolWrapper& CmdPool, VkCommandBuffer& vkCmdBuff, const Char *DebugPoolName)
{
    // TODO: rework this
    auto CompletedFenceValue = m_CommandQueues[0].CmdQueue->GetCompletedFenceValue();
    CmdPool = m_TransientCmdPoolMgr.AllocateCommandPool(CompletedFenceValue, DebugPoolName);

    // Allocate command buffer from the cmd pool
    VkCommandBufferAllocateInfo BuffAllocInfo = {};
    BuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    BuffAllocInfo.pNext = nullptr;
    BuffAllocInfo.commandPool = CmdPool;
    BuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    BuffAllocInfo.commandBufferCount = 1;
    vkCmdBuff = m_LogicalVkDevice->AllocateVkCommandBuffer(BuffAllocInfo);
    VERIFY_EXPR(vkCmdBuff != VK_NULL_HANDLE);
        
    VkCommandBufferBeginInfo CmdBuffBeginInfo = {};
    CmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CmdBuffBeginInfo.pNext = nullptr;
    CmdBuffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Each recording of the command buffer will only be 
                                                                          // submitted once, and the command buffer will be reset 
                                                                          // and recorded again between each submission.
    CmdBuffBeginInfo.pInheritanceInfo = nullptr; // Ignored for a primary command buffer
    vkBeginCommandBuffer(vkCmdBuff, &CmdBuffBeginInfo);
}


void RenderDeviceVkImpl::ExecuteAndDisposeTransientCmdBuff(Uint32 QueueIndex, VkCommandBuffer vkCmdBuff, VulkanUtilities::CommandPoolWrapper&& CmdPool)
{
    VERIFY_EXPR(vkCmdBuff != VK_NULL_HANDLE);

    auto err = vkEndCommandBuffer(vkCmdBuff);
    VERIFY(err == VK_SUCCESS, "Failed to end command buffer");

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &vkCmdBuff;

    // We MUST NOT discard stale objects when executing transient command buffer,
    // otherwise a resource can be destroyed while still being used by the GPU:
    // 
    //                           
    // Next Cmd Buff| Next Fence |        Immediate Context               |            This thread               |
    //              |            |                                        |                                      |
    //      N       |     F      |                                        |                                      |
    //              |            |  Draw(ResourceX)                       |                                      |
    //      N  -  - | -   -   -  |  Release(ResourceX)                    |                                      |
    //              |            |  - {N, ResourceX} -> Stale Objects     |                                      |
    //              |            |                                        |                                      |
    //              |            |                                        | SubmitCommandBuffer()                |
    //              |            |                                        | - SubmittedCmdBuffNumber = N         |
    //              |            |                                        | - SubmittedFenceValue = F            |
    //     N+1      |    F+1     |                                        | - DiscardStaleVkObjects(N, F)        |
    //              |            |                                        |   - {F, ResourceX} -> Release Queue  |
    //              |            |                                        |                                      |
    //     N+2 -   -|  - F+2  -  |  ExecuteCommandBuffer()                |                                      |
    //              |            |  - SubmitCommandBuffer()               |                                      |
    //              |            |  - ResourceX is already in release     |                                      |
    //              |            |    queue with fence value F, and       |                                      |
    //              |            |    F < SubmittedFenceValue==F+1        |                                      |
    //
    // Since transient command buffers do not count as real command buffers, submit them directly to the queue
    // to avoid interference with the command buffer numbers
    Uint64 FenceValue = GetCommandQueue(0).Submit(SubmitInfo);
    // Dispose command pool
    m_TransientCmdPoolMgr.DisposeCommandPool(std::move(CmdPool), FenceValue);
}

void RenderDeviceVkImpl::SubmitCommandBuffer(const VkSubmitInfo& SubmitInfo, 
                                             Uint64&             SubmittedCmdBuffNumber,                      // Number of the submitted command buffer 
                                             Uint64&             SubmittedFenceValue,                         // Fence value associated with the submitted command buffer
                                             std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >* pFences // List of fences to signal
                                             )
{
	// Submit the command list to the queue
    Uint32 QueueIndex = 0;
    auto CmbBuffInfo = TRenderDeviceBase::SubmitCommandBuffer(QueueIndex, SubmitInfo, true);
    SubmittedFenceValue    = CmbBuffInfo.FenceValue;
    SubmittedCmdBuffNumber = CmbBuffInfo.CmdBufferNumber;
    if (pFences != nullptr)
    {
        for (auto& val_fence : *pFences)
        {
            auto* pFenceVkImpl = val_fence.second.RawPtr<FenceVkImpl>();
            auto vkFence = pFenceVkImpl->GetVkFence();
            m_CommandQueues[QueueIndex].CmdQueue->SignalFence(vkFence);
            pFenceVkImpl->AddPendingFence(std::move(vkFence), val_fence.first);
        }
    }
}

Uint64 RenderDeviceVkImpl::ExecuteCommandBuffer(const VkSubmitInfo& SubmitInfo, DeviceContextVkImpl* pImmediateCtx, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >* pSignalFences)
{
    // pImmediateCtx parameter is only used to make sure the command buffer is submitted from the immediate context
    // Stale objects MUST only be discarded when submitting cmd list from the immediate context
    VERIFY(!pImmediateCtx->IsDeferred(), "Command buffers must be submitted from immediate context only");

    Uint64 SubmittedFenceValue = 0;
    Uint64 SubmittedCmdBuffNumber = 0;
    SubmitCommandBuffer(SubmitInfo, SubmittedCmdBuffNumber, SubmittedFenceValue, pSignalFences);

    // TODO: rework this
    auto CompletedFenceValue = m_CommandQueues[0].CmdQueue->GetCompletedFenceValue();
    m_MainDescriptorPool.ReleaseStaleAllocations(CompletedFenceValue);
    m_MemoryMgr.ShrinkMemory();
    PurgeReleaseQueues();

    return SubmittedFenceValue;
}


void RenderDeviceVkImpl::IdleGPU(bool ReleaseStaleObjects) 
{ 
    IdleCommandQueues(ReleaseStaleObjects, ReleaseStaleObjects);
    m_LogicalVkDevice->WaitIdle();

    if (ReleaseStaleObjects)
    {
        // Do not wait until the end of the frame and force deletion. 
        // This is necessary to release outstanding references to the
        // swap chain buffers when it is resized in the middle of the frame.
        // Since GPU has been idled, it it is safe to do so

        // SubmittedFenceValue has now been signaled by the GPU since we waited for it
        m_MainDescriptorPool.ReleaseStaleAllocations(m_CommandQueues[0].CmdQueue->GetCompletedFenceValue());
        m_MemoryMgr.ShrinkMemory();
    }
}


void RenderDeviceVkImpl::FinishFrame(bool ReleaseAllResources)
{
    // TODO: rework this
    auto CompletedFenceValue = ReleaseAllResources ? std::numeric_limits<Uint64>::max() : m_CommandQueues[0].CmdQueue->GetCompletedFenceValue();
    
    // Discard all remaining objects. This is important to do if there were 
    // no command lists submitted during the frame. All stale resources will
    // be associated with the submitted fence value and thus will not be released
    // until the GPU is finished with the current frame
    Uint64 SubmittedFenceValue = 0;
    Uint64 SubmittedCmdBuffNumber = 0;
    VkSubmitInfo DummySubmitInfo = {};
    // Submit empty command buffer to set a fence on the GPU
    SubmitCommandBuffer(DummySubmitInfo, SubmittedCmdBuffNumber, SubmittedFenceValue, nullptr);
        
    PurgeReleaseQueues();

    m_MainDescriptorPool.ReleaseStaleAllocations(CompletedFenceValue);
    m_MemoryMgr.ShrinkMemory();

    m_DynamicMemoryManager.ReleaseStaleBlocks(CompletedFenceValue);
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


void RenderDeviceVkImpl :: CreateBufferFromVulkanResource(VkBuffer vkBuffer, const BufferDesc& BuffDesc, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferVkImpl* pBufferVk( NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, vkBuffer ) );
            pBufferVk->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferVk->CreateDefaultViews();
            OnCreateDeviceObject( pBufferVk );
        } 
    );
}


void RenderDeviceVkImpl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferVkImpl* pBufferVk( NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, BuffData ) );
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


void RenderDeviceVkImpl::CreateTextureFromVulkanImage(VkImage vkImage, const TextureDesc& TexDesc, ITexture** ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, vkImage );

            pTextureVk->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureVk->CreateDefaultViews();
            OnCreateDeviceObject( pTextureVk );
        } 
    );
}


void RenderDeviceVkImpl::CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, class TextureVkImpl **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, std::move(vkImgHandle));
            pTextureVk->QueryInterface( IID_TextureVk, reinterpret_cast<IObject**>(ppTexture) );
        }
    );
}


void RenderDeviceVkImpl :: CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, Data );

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
                SamplerVkImpl* pSamplerVk( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerVkImpl instance", SamplerVkImpl)(this, SamplerDesc ) );
                pSamplerVk->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerVk );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

void RenderDeviceVkImpl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject( "Fence", Desc, ppFence, 
        [&]()
        {
            FenceVkImpl* pFenceVk( NEW_RC_OBJ(m_FenceAllocator, "FenceVkImpl instance", FenceVkImpl)
                                             (this, Desc) );
            pFenceVk->QueryInterface( IID_Fence, reinterpret_cast<IObject**>(ppFence) );
            OnCreateDeviceObject( pFenceVk );
        }
    );
}

}
