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
#include "RenderDeviceVkImpl.hpp"
#include "PipelineStateVkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "TextureVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "SamplerVkImpl.hpp"
#include "BufferVkImpl.hpp"
#include "ShaderResourceBindingVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "FenceVkImpl.hpp"
#include "QueryVkImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceVkImpl::RenderDeviceVkImpl(IReferenceCounters*                                    pRefCounters,
                                       IMemoryAllocator&                                      RawMemAllocator,
                                       IEngineFactory*                                        pEngineFactory,
                                       const EngineVkCreateInfo&                              EngineCI,
                                       size_t                                                 CommandQueueCount,
                                       ICommandQueueVk**                                      CmdQueues,
                                       std::shared_ptr<VulkanUtilities::VulkanInstance>       Instance,
                                       std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                       std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>  LogicalDevice) :
    // clang-format off
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        pEngineFactory,
        CommandQueueCount,
        CmdQueues,
        EngineCI.NumDeferredContexts,
        DeviceObjectSizes
        {
            sizeof(TextureVkImpl),
            sizeof(TextureViewVkImpl),
            sizeof(BufferVkImpl),
            sizeof(BufferViewVkImpl),
            sizeof(ShaderVkImpl),
            sizeof(SamplerVkImpl),
            sizeof(PipelineStateVkImpl),
            sizeof(ShaderResourceBindingVkImpl),
            sizeof(FenceVkImpl),
            sizeof(QueryVkImpl)
        }
    },
    m_VulkanInstance    {Instance                 },
    m_PhysicalDevice    {std::move(PhysicalDevice)},
    m_LogicalVkDevice   {std::move(LogicalDevice) },
    m_EngineAttribs     {EngineCI                 },
    m_FramebufferCache  {*this                    },
    m_RenderPassCache   {*this                    },
    m_DescriptorSetAllocator
    {
        *this,
        "Main descriptor pool",
        std::vector<VkDescriptorPoolSize>
        {
            {VK_DESCRIPTOR_TYPE_SAMPLER,                EngineCI.MainDescriptorPoolSize.NumSeparateSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, EngineCI.MainDescriptorPoolSize.NumCombinedSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          EngineCI.MainDescriptorPoolSize.NumSampledImageDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          EngineCI.MainDescriptorPoolSize.NumStorageImageDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   EngineCI.MainDescriptorPoolSize.NumUniformTexelBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   EngineCI.MainDescriptorPoolSize.NumStorageTexelBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         EngineCI.MainDescriptorPoolSize.NumUniformBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         EngineCI.MainDescriptorPoolSize.NumStorageBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, EngineCI.MainDescriptorPoolSize.NumUniformBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, EngineCI.MainDescriptorPoolSize.NumStorageBufferDescriptors},
        },
        EngineCI.MainDescriptorPoolSize.MaxDescriptorSets,
        true
    },
    m_DynamicDescriptorPool
    {
        *this,
        "Dynamic descriptor pool",
        std::vector<VkDescriptorPoolSize>
        {
            {VK_DESCRIPTOR_TYPE_SAMPLER,                EngineCI.DynamicDescriptorPoolSize.NumSeparateSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, EngineCI.DynamicDescriptorPoolSize.NumCombinedSamplerDescriptors},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          EngineCI.DynamicDescriptorPoolSize.NumSampledImageDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          EngineCI.DynamicDescriptorPoolSize.NumStorageImageDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   EngineCI.DynamicDescriptorPoolSize.NumUniformTexelBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   EngineCI.DynamicDescriptorPoolSize.NumStorageTexelBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         EngineCI.DynamicDescriptorPoolSize.NumUniformBufferDescriptors},
            //{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         EngineCI.DynamicDescriptorPoolSize.NumStorageBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, EngineCI.DynamicDescriptorPoolSize.NumUniformBufferDescriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, EngineCI.DynamicDescriptorPoolSize.NumStorageBufferDescriptors},
        },
        EngineCI.DynamicDescriptorPoolSize.MaxDescriptorSets,
        false // Pools can only be reset
    },
    m_TransientCmdPoolMgr
    {
        *this,
        "Transient command buffer pool manager",
        CmdQueues[0]->GetQueueFamilyIndex(),
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    },
    m_MemoryMgr
    {
        "Global resource memory manager",
        *m_LogicalVkDevice,
        *m_PhysicalDevice,
        GetRawAllocator(),
        EngineCI.DeviceLocalMemoryPageSize,
        EngineCI.HostVisibleMemoryPageSize,
        EngineCI.DeviceLocalMemoryReserveSize,
        EngineCI.HostVisibleMemoryReserveSize
    },
    m_DynamicMemoryManager
    {
        GetRawAllocator(),
        *this,
        EngineCI.DynamicHeapSize,
        ~Uint64{0}
    }
// clang-format on
{
    m_DeviceCaps.DevType      = RENDER_DEVICE_TYPE_VULKAN;
    m_DeviceCaps.MajorVersion = 1;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.AdaterType   = ADAPTER_TYPE_HARDWARE;
    for (Uint32 fmt = 1; fmt < m_TextureFormatsInfo.size(); ++fmt)
        m_TextureFormatsInfo[fmt].Supported = true; // We will test every format on a specific hardware device

    auto&       Features         = m_DeviceCaps.Features;
    const auto& vkDeviceFeatures = m_PhysicalDevice->GetFeatures();

    Features.SeparablePrograms                 = True;
    Features.IndirectRendering                 = True;
    Features.WireframeFill                     = vkDeviceFeatures.fillModeNonSolid != VK_FALSE;
    Features.MultithreadedResourceCreation     = True;
    Features.ComputeShaders                    = True;
    Features.GeometryShaders                   = vkDeviceFeatures.geometryShader != VK_FALSE;
    Features.Tessellation                      = vkDeviceFeatures.tessellationShader != VK_FALSE;
    Features.BindlessResources                 = True;
    Features.OcclusionQueries                  = vkDeviceFeatures.occlusionQueryPrecise != VK_FALSE;
    Features.BinaryOcclusionQueries            = True;
    Features.TimestampQueries                  = True;
    Features.PipelineStatisticsQueries         = vkDeviceFeatures.pipelineStatisticsQuery != VK_FALSE;
    Features.DepthBiasClamp                    = vkDeviceFeatures.depthBiasClamp != VK_FALSE;
    Features.DepthClamp                        = vkDeviceFeatures.depthClamp != VK_FALSE;
    Features.IndependentBlend                  = vkDeviceFeatures.independentBlend != VK_FALSE;
    Features.DualSourceBlend                   = vkDeviceFeatures.dualSrcBlend != VK_FALSE;
    Features.MultiViewport                     = vkDeviceFeatures.multiViewport != VK_FALSE;
    Features.TextureCompressionBC              = vkDeviceFeatures.textureCompressionBC != VK_FALSE;
    Features.VertexPipelineUAVWritesAndAtomics = vkDeviceFeatures.vertexPipelineStoresAndAtomics != VK_FALSE;
    Features.PixelUAVWritesAndAtomics          = vkDeviceFeatures.fragmentStoresAndAtomics != VK_FALSE;
    Features.TextureUAVExtendedFormats         = vkDeviceFeatures.shaderStorageImageExtendedFormats != VK_FALSE;

    const auto& vkDeviceLimits = m_PhysicalDevice->GetProperties().limits;
    auto&       TexCaps        = m_DeviceCaps.TexCaps;

    TexCaps.MaxTexture1DDimension     = vkDeviceLimits.maxImageDimension1D;
    TexCaps.MaxTexture1DArraySlices   = vkDeviceLimits.maxImageArrayLayers;
    TexCaps.MaxTexture2DDimension     = vkDeviceLimits.maxImageDimension2D;
    TexCaps.MaxTexture2DArraySlices   = vkDeviceLimits.maxImageArrayLayers;
    TexCaps.MaxTexture3DDimension     = vkDeviceLimits.maxImageDimension3D;
    TexCaps.MaxTextureCubeDimension   = vkDeviceLimits.maxImageDimensionCube;
    TexCaps.Texture2DMSSupported      = True;
    TexCaps.Texture2DMSArraySupported = True;
    TexCaps.TextureViewSupported      = True;
    TexCaps.CubemapArraysSupported    = vkDeviceFeatures.imageCubeArray;


    auto& SamCaps = m_DeviceCaps.SamCaps;

    SamCaps.BorderSamplingModeSupported   = True;
    SamCaps.AnisotropicFilteringSupported = vkDeviceFeatures.samplerAnisotropy;
    SamCaps.LODBiasSupported              = True;
}

RenderDeviceVkImpl::~RenderDeviceVkImpl()
{
    // Explicitly destroy dynamic heap. This will move resources owned by
    // the heap into release queues
    m_DynamicMemoryManager.Destroy();

    // Wait for the GPU to complete all its operations
    IdleGPU();

    ReleaseStaleResources(true);

    DEV_CHECK_ERR(m_DescriptorSetAllocator.GetAllocatedDescriptorSetCounter() == 0, "All allocated descriptor sets must have been released now.");
    DEV_CHECK_ERR(m_TransientCmdPoolMgr.GetAllocatedPoolCount() == 0, "All allocated transient command pools must have been released now. If there are outstanding references to the pools in release queues, the app will crash when CommandPoolManager::FreeCommandPool() is called.");
    DEV_CHECK_ERR(m_DynamicDescriptorPool.GetAllocatedPoolCounter() == 0, "All allocated dynamic descriptor pools must have been released now.");
    DEV_CHECK_ERR(m_DynamicMemoryManager.GetMasterBlockCounter() == 0, "All allocated dynamic master blocks must have been returned to the pool.");

    // Immediately destroys all command pools
    m_TransientCmdPoolMgr.DestroyPools();

    // We must destroy command queues explicitly prior to releasing Vulkan device
    DestroyCommandQueues();

    //if(m_PhysicalDevice)
    //{
    //    // If m_PhysicalDevice is empty, the device does not own vulkan logical device and must not
    //    // destroy it
    //    vkDestroyDevice(m_VkDevice, m_VulkanInstance->GetVkAllocator());
    //}
}


void RenderDeviceVkImpl::AllocateTransientCmdPool(VulkanUtilities::CommandPoolWrapper& CmdPool, VkCommandBuffer& vkCmdBuff, const Char* DebugPoolName)
{
    CmdPool = m_TransientCmdPoolMgr.AllocateCommandPool(DebugPoolName);

    // Allocate command buffer from the cmd pool
    VkCommandBufferAllocateInfo BuffAllocInfo = {};

    BuffAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    BuffAllocInfo.pNext              = nullptr;
    BuffAllocInfo.commandPool        = CmdPool;
    BuffAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    BuffAllocInfo.commandBufferCount = 1;

    vkCmdBuff = m_LogicalVkDevice->AllocateVkCommandBuffer(BuffAllocInfo);
    DEV_CHECK_ERR(vkCmdBuff != VK_NULL_HANDLE, "Failed to allocate Vulkan command buffer");


    VkCommandBufferBeginInfo CmdBuffBeginInfo = {};

    CmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CmdBuffBeginInfo.pNext = nullptr;
    CmdBuffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Each recording of the command buffer will only be
                                                                          // submitted once, and the command buffer will be reset
                                                                          // and recorded again between each submission.
    CmdBuffBeginInfo.pInheritanceInfo = nullptr;                          // Ignored for a primary command buffer

    auto err = vkBeginCommandBuffer(vkCmdBuff, &CmdBuffBeginInfo);
    DEV_CHECK_ERR(err == VK_SUCCESS, "vkBeginCommandBuffer() failed");
    (void)err;
}


void RenderDeviceVkImpl::ExecuteAndDisposeTransientCmdBuff(Uint32 QueueIndex, VkCommandBuffer vkCmdBuff, VulkanUtilities::CommandPoolWrapper&& CmdPool)
{
    VERIFY_EXPR(vkCmdBuff != VK_NULL_HANDLE);

    auto err = vkEndCommandBuffer(vkCmdBuff);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to end command buffer");
    (void)err;

    VkSubmitInfo SubmitInfo = {};

    SubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers    = &vkCmdBuff;

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
    // to avoid interference with the command buffer counter
    Uint64 FenceValue = 0;
    LockCmdQueueAndRun(QueueIndex,
                       [&](ICommandQueueVk* pCmdQueueVk) //
                       {
                           FenceValue = pCmdQueueVk->Submit(SubmitInfo);
                       } //
    );
    m_TransientCmdPoolMgr.SafeReleaseCommandPool(std::move(CmdPool), QueueIndex, FenceValue);
}

void RenderDeviceVkImpl::SubmitCommandBuffer(Uint32                                                 QueueIndex,
                                             const VkSubmitInfo&                                    SubmitInfo,
                                             Uint64&                                                SubmittedCmdBuffNumber, // Number of the submitted command buffer
                                             Uint64&                                                SubmittedFenceValue,    // Fence value associated with the submitted command buffer
                                             std::vector<std::pair<Uint64, RefCntAutoPtr<IFence>>>* pFences                 // List of fences to signal
)
{
    // Submit the command list to the queue
    auto CmbBuffInfo       = TRenderDeviceBase::SubmitCommandBuffer(QueueIndex, SubmitInfo, true);
    SubmittedFenceValue    = CmbBuffInfo.FenceValue;
    SubmittedCmdBuffNumber = CmbBuffInfo.CmdBufferNumber;
    if (pFences != nullptr)
    {
        for (auto& val_fence : *pFences)
        {
            auto* pFenceVkImpl = val_fence.second.RawPtr<FenceVkImpl>();
            auto  vkFence      = pFenceVkImpl->GetVkFence();
            m_CommandQueues[QueueIndex].CmdQueue->SignalFence(vkFence);
            pFenceVkImpl->AddPendingFence(std::move(vkFence), val_fence.first);
        }
    }
}

Uint64 RenderDeviceVkImpl::ExecuteCommandBuffer(Uint32 QueueIndex, const VkSubmitInfo& SubmitInfo, DeviceContextVkImpl* pImmediateCtx, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence>>>* pSignalFences)
{
    // pImmediateCtx parameter is only used to make sure the command buffer is submitted from the immediate context
    // Stale objects MUST only be discarded when submitting cmd list from the immediate context
    VERIFY(!pImmediateCtx->IsDeferred(), "Command buffers must be submitted from immediate context only");

    Uint64 SubmittedFenceValue    = 0;
    Uint64 SubmittedCmdBuffNumber = 0;
    SubmitCommandBuffer(QueueIndex, SubmitInfo, SubmittedCmdBuffNumber, SubmittedFenceValue, pSignalFences);

    m_MemoryMgr.ShrinkMemory();
    PurgeReleaseQueue(QueueIndex);

    return SubmittedFenceValue;
}


void RenderDeviceVkImpl::IdleGPU()
{
    IdleAllCommandQueues(true);
    m_LogicalVkDevice->WaitIdle();
    ReleaseStaleResources();
}

void RenderDeviceVkImpl::FlushStaleResources(Uint32 CmdQueueIndex)
{
    // Submit empty command buffer to the queue. This will effectively signal the fence and
    // discard all resources
    VkSubmitInfo DummySumbitInfo = {};
    TRenderDeviceBase::SubmitCommandBuffer(0, DummySumbitInfo, true);
}

void RenderDeviceVkImpl::ReleaseStaleResources(bool ForceRelease)
{
    m_MemoryMgr.ShrinkMemory();
    PurgeReleaseQueues(ForceRelease);
}


void RenderDeviceVkImpl::TestTextureFormat(TEXTURE_FORMAT TexFormat)
{
    auto& TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY(TexFormatInfo.Supported, "Texture format is not supported");

    auto vkPhysicalDevice = m_PhysicalDevice->GetVkDeviceHandle();

    auto SRVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_SHADER_RESOURCE, BIND_SHADER_RESOURCE);
    auto RTVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_RENDER_TARGET, BIND_RENDER_TARGET);
    auto DSVFormat = GetDefaultTextureViewFormat(TexFormat, TEXTURE_VIEW_DEPTH_STENCIL, BIND_DEPTH_STENCIL);

    if (SRVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat           vkSrvFormat   = TexFormatToVkFormat(SRVFormat);
        VkFormatProperties vkSrvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkSrvFormat, &vkSrvFmtProps);

        if (vkSrvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        {
            TexFormatInfo.Filterable = true;

            {
                VkImageFormatProperties ImgFmtProps = {};

                auto err               = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_1D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex1DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};

                auto err               = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex2DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};

                auto err               = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImgFmtProps);
                TexFormatInfo.Tex3DFmt = err == VK_SUCCESS;
            }

            {
                VkImageFormatProperties ImgFmtProps = {};

                auto err                 = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkSrvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                    VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, &ImgFmtProps);
                TexFormatInfo.TexCubeFmt = err == VK_SUCCESS;
            }
        }
    }

    if (RTVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat           vkRtvFormat   = TexFormatToVkFormat(RTVFormat);
        VkFormatProperties vkRtvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkRtvFormat, &vkRtvFmtProps);
        if (vkRtvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        {
            VkImageFormatProperties ImgFmtProps = {};

            auto err                      = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkRtvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &ImgFmtProps);
            TexFormatInfo.ColorRenderable = err == VK_SUCCESS;
            if (TexFormatInfo.ColorRenderable)
            {
                TexFormatInfo.SampleCounts = ImgFmtProps.sampleCounts;
            }
        }
    }

    if (DSVFormat != TEX_FORMAT_UNKNOWN)
    {
        VkFormat           vkDsvFormat   = TexFormatToVkFormat(DSVFormat);
        VkFormatProperties vkDsvFmtProps = {};
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice, vkDsvFormat, &vkDsvFmtProps);
        if (vkDsvFmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            VkImageFormatProperties ImgFmtProps = {};
            auto                    err         = vkGetPhysicalDeviceImageFormatProperties(vkPhysicalDevice, vkDsvFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &ImgFmtProps);
            TexFormatInfo.DepthRenderable       = err == VK_SUCCESS;
            if (TexFormatInfo.DepthRenderable)
            {
                TexFormatInfo.SampleCounts = ImgFmtProps.sampleCounts;
            }
        }
    }
}


IMPLEMENT_QUERY_INTERFACE(RenderDeviceVkImpl, IID_RenderDeviceVk, TRenderDeviceBase)

void RenderDeviceVkImpl::CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState** ppPipelineState)
{
    CreateDeviceObject(
        "Pipeline State", PipelineDesc, ppPipelineState,
        [&]() //
        {
            PipelineStateVkImpl* pPipelineStateVk(NEW_RC_OBJ(m_PSOAllocator, "PipelineStateVkImpl instance", PipelineStateVkImpl)(this, PipelineDesc));
            pPipelineStateVk->QueryInterface(IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState));
            OnCreateDeviceObject(pPipelineStateVk);
        } //
    );
}


void RenderDeviceVkImpl::CreateBufferFromVulkanResource(VkBuffer vkBuffer, const BufferDesc& BuffDesc, RESOURCE_STATE InitialState, IBuffer** ppBuffer)
{
    CreateDeviceObject(
        "buffer", BuffDesc, ppBuffer,
        [&]() //
        {
            BufferVkImpl* pBufferVk(NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, InitialState, vkBuffer));
            pBufferVk->QueryInterface(IID_Buffer, reinterpret_cast<IObject**>(ppBuffer));
            pBufferVk->CreateDefaultViews();
            OnCreateDeviceObject(pBufferVk);
        } //
    );
}


void RenderDeviceVkImpl::CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)
{
    CreateDeviceObject(
        "buffer", BuffDesc, ppBuffer,
        [&]() //
        {
            BufferVkImpl* pBufferVk(NEW_RC_OBJ(m_BufObjAllocator, "BufferVkImpl instance", BufferVkImpl)(m_BuffViewObjAllocator, this, BuffDesc, pBuffData));
            pBufferVk->QueryInterface(IID_Buffer, reinterpret_cast<IObject**>(ppBuffer));
            pBufferVk->CreateDefaultViews();
            OnCreateDeviceObject(pBufferVk);
        } //
    );
}


void RenderDeviceVkImpl::CreateShader(const ShaderCreateInfo& ShaderCI, IShader** ppShader)
{
    CreateDeviceObject(
        "shader", ShaderCI.Desc, ppShader,
        [&]() //
        {
            ShaderVkImpl* pShaderVk(NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderVkImpl instance", ShaderVkImpl)(this, ShaderCI));
            pShaderVk->QueryInterface(IID_Shader, reinterpret_cast<IObject**>(ppShader));

            OnCreateDeviceObject(pShaderVk);
        } //
    );
}


void RenderDeviceVkImpl::CreateTextureFromVulkanImage(VkImage vkImage, const TextureDesc& TexDesc, RESOURCE_STATE InitialState, ITexture** ppTexture)
{
    CreateDeviceObject(
        "texture", TexDesc, ppTexture,
        [&]() //
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, InitialState, vkImage);

            pTextureVk->QueryInterface(IID_Texture, reinterpret_cast<IObject**>(ppTexture));
            pTextureVk->CreateDefaultViews();
            OnCreateDeviceObject(pTextureVk);
        } //
    );
}


void RenderDeviceVkImpl::CreateTexture(const TextureDesc& TexDesc, VkImage vkImgHandle, RESOURCE_STATE InitialState, class TextureVkImpl** ppTexture)
{
    CreateDeviceObject(
        "texture", TexDesc, ppTexture,
        [&]() //
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, InitialState, std::move(vkImgHandle));
            pTextureVk->QueryInterface(IID_TextureVk, reinterpret_cast<IObject**>(ppTexture));
        } //
    );
}


void RenderDeviceVkImpl::CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)
{
    CreateDeviceObject(
        "texture", TexDesc, ppTexture,
        [&]() //
        {
            TextureVkImpl* pTextureVk = NEW_RC_OBJ(m_TexObjAllocator, "TextureVkImpl instance", TextureVkImpl)(m_TexViewObjAllocator, this, TexDesc, pData);

            pTextureVk->QueryInterface(IID_Texture, reinterpret_cast<IObject**>(ppTexture));
            pTextureVk->CreateDefaultViews();
            OnCreateDeviceObject(pTextureVk);
        } //
    );
}

void RenderDeviceVkImpl::CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)
{
    CreateDeviceObject(
        "sampler", SamplerDesc, ppSampler,
        [&]() //
        {
            m_SamplersRegistry.Find(SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler));
            if (*ppSampler == nullptr)
            {
                SamplerVkImpl* pSamplerVk(NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerVkImpl instance", SamplerVkImpl)(this, SamplerDesc));
                pSamplerVk->QueryInterface(IID_Sampler, reinterpret_cast<IObject**>(ppSampler));
                OnCreateDeviceObject(pSamplerVk);
                m_SamplersRegistry.Add(SamplerDesc, *ppSampler);
            }
        } //
    );
}

void RenderDeviceVkImpl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject(
        "Fence", Desc, ppFence,
        [&]() //
        {
            FenceVkImpl* pFenceVk(NEW_RC_OBJ(m_FenceAllocator, "FenceVkImpl instance", FenceVkImpl)(this, Desc));
            pFenceVk->QueryInterface(IID_Fence, reinterpret_cast<IObject**>(ppFence));
            OnCreateDeviceObject(pFenceVk);
        } //
    );
}

void RenderDeviceVkImpl::CreateQuery(const QueryDesc& Desc, IQuery** ppQuery)
{
    CreateDeviceObject(
        "Query", Desc, ppQuery,
        [&]() //
        {
            QueryVkImpl* pQueryVk(NEW_RC_OBJ(m_QueryAllocator, "QueryVkImpl instance", QueryVkImpl)(this, Desc));
            pQueryVk->QueryInterface(IID_Query, reinterpret_cast<IObject**>(ppQuery));
            OnCreateDeviceObject(pQueryVk);
        } //
    );
}

} // namespace Diligent
