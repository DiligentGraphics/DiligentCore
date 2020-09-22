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

/// \file
/// Routines that initialize Vulkan-based engine implementation

#include "pch.h"
#include <array>
#include "EngineFactoryVk.h"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "SwapChainVkImpl.hpp"
#include "EngineMemory.h"
#include "CommandQueueVkImpl.hpp"
#include "VulkanUtilities/VulkanInstance.hpp"
#include "VulkanUtilities/VulkanPhysicalDevice.hpp"
#include "EngineFactoryBase.hpp"

#if PLATFORM_ANDROID
#    include "FileSystem.hpp"
#endif

namespace Diligent
{

/// Engine factory for Vk implementation
class EngineFactoryVkImpl : public EngineFactoryBase<IEngineFactoryVk>
{
public:
    static EngineFactoryVkImpl* GetInstance()
    {
        static EngineFactoryVkImpl TheFactory;
        return &TheFactory;
    }

    using TBase = EngineFactoryBase<IEngineFactoryVk>;
    EngineFactoryVkImpl() :
        TBase{IID_EngineFactoryVk}
    {}

    virtual void DILIGENT_CALL_TYPE CreateDeviceAndContextsVk(const EngineVkCreateInfo& EngineCI,
                                                              IRenderDevice**           ppDevice,
                                                              IDeviceContext**          ppContexts) override final;

    virtual void DILIGENT_CALL_TYPE AttachToVulkanDevice(std::shared_ptr<VulkanUtilities::VulkanInstance>       Instance,
                                                         std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                                         std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>  LogicalDevice,
                                                         size_t                                                 CommandQueueCount,
                                                         ICommandQueueVk**                                      ppCommandQueues,
                                                         const EngineVkCreateInfo&                              EngineCI,
                                                         IRenderDevice**                                        ppDevice,
                                                         IDeviceContext**                                       ppContexts); //override final;

    virtual void DILIGENT_CALL_TYPE CreateSwapChainVk(IRenderDevice*       pDevice,
                                                      IDeviceContext*      pImmediateContext,
                                                      const SwapChainDesc& SwapChainDesc,
                                                      const NativeWindow&  Window,
                                                      ISwapChain**         ppSwapChain) override final;

#if PLATFORM_ANDROID
    virtual void InitAndroidFileSystem(struct ANativeActivity* NativeActivity,
                                       const char*             NativeActivityClassName,
                                       struct AAssetManager*   AssetManager) const override final;
#endif

private:
    std::function<void(RenderDeviceVkImpl*)> OnRenderDeviceCreated = nullptr;
};


void EngineFactoryVkImpl::CreateDeviceAndContextsVk(const EngineVkCreateInfo& _EngineCI,
                                                    IRenderDevice**           ppDevice,
                                                    IDeviceContext**          ppContexts)
{
    if (_EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(_EngineCI.DebugMessageCallback);

    if (_EngineCI.APIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", _EngineCI.APIVersion, ")");
        return;
    }

    VERIFY(ppDevice && ppContexts, "Null pointer provided");
    if (!ppDevice || !ppContexts)
        return;

    EngineVkCreateInfo EngineCI = _EngineCI;

#if 0
    for (Uint32 Type = Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; Type < Vk_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++Type)
    {
        auto CPUHeapAllocSize = EngineCI.CPUDescriptorHeapAllocationSize[Type];
        Uint32 MaxSize = 1 << 20;
        if (CPUHeapAllocSize > 1 << 20)
        {
            LOG_ERROR("CPU Heap allocation size is too large (", CPUHeapAllocSize, "). Max allowed size is ", MaxSize);
            return;
        }

        if ((CPUHeapAllocSize % 16) != 0)
        {
            LOG_ERROR("CPU Heap allocation size (", CPUHeapAllocSize, ") is expected to be multiple of 16");
            return;
        }
    }
#endif

    SetRawAllocator(EngineCI.pRawMemAllocator);

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1 + EngineCI.NumDeferredContexts));

    try
    {
        auto Instance = VulkanUtilities::VulkanInstance::Create(
            EngineCI.EnableValidation,
            EngineCI.GlobalExtensionCount,
            EngineCI.ppGlobalExtensionNames,
            reinterpret_cast<VkAllocationCallbacks*>(EngineCI.pVkAllocator));

        auto        vkDevice               = Instance->SelectPhysicalDevice(EngineCI.AdapterId);
        auto        PhysicalDevice         = VulkanUtilities::VulkanPhysicalDevice::Create(vkDevice, *Instance);
        const auto& PhysicalDeviceFeatures = PhysicalDevice->GetFeatures();

        // If an implementation exposes any queue family that supports graphics operations,
        // at least one queue family of at least one physical device exposed by the implementation
        // must support both graphics and compute operations.

        VkDeviceQueueCreateInfo QueueInfo{};
        QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueInfo.flags = 0; // reserved for future use
        // All commands that are allowed on a queue that supports transfer operations are also allowed on a
        // queue that supports either graphics or compute operations. Thus, if the capabilities of a queue family
        // include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT, then reporting the VK_QUEUE_TRANSFER_BIT
        // capability separately for that queue family is optional (4.1).
        QueueInfo.queueFamilyIndex       = PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
        QueueInfo.queueCount             = 1;
        const float defaultQueuePriority = 1.0f; // Ask for highest priority for our queue. (range [0,1])
        QueueInfo.pQueuePriorities       = &defaultQueuePriority;

        VkDeviceCreateInfo DeviceCreateInfo = {};
        DeviceCreateInfo.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DeviceCreateInfo.flags              = 0; // Reserved for future use
        // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#extended-functionality-device-layer-deprecation
        DeviceCreateInfo.enabledLayerCount       = 0;       // Deprecated and ignored.
        DeviceCreateInfo.ppEnabledLayerNames     = nullptr; // Deprecated and ignored
        DeviceCreateInfo.queueCreateInfoCount    = 1;
        DeviceCreateInfo.pQueueCreateInfos       = &QueueInfo;
        VkPhysicalDeviceFeatures EnabledFeatures = {};
        EnabledFeatures.fullDrawIndexUint32      = PhysicalDeviceFeatures.fullDrawIndexUint32;

#define ENABLE_FEATURE(Feature, RequestedState, FeatureName)                           \
    do                                                                                 \
    {                                                                                  \
        switch (RequestedState)                                                        \
        {                                                                              \
            case DEVICE_FEATURE_STATE_DISABLED:                                        \
                EnabledFeatures.Feature = VK_FALSE;                                    \
                break;                                                                 \
            case DEVICE_FEATURE_STATE_ENABLED:                                         \
            {                                                                          \
                if (PhysicalDeviceFeatures.Feature == VK_FALSE)                        \
                    LOG_ERROR_AND_THROW(FeatureName, " not supported by this device"); \
                else                                                                   \
                    EnabledFeatures.Feature = VK_TRUE;                                 \
            }                                                                          \
            break;                                                                     \
            case DEVICE_FEATURE_STATE_OPTIONAL:                                        \
                EnabledFeatures.Feature = PhysicalDeviceFeatures.Feature;              \
                break;                                                                 \
            default: UNEXPECTED("Unexpected feature state");                           \
        }                                                                              \
    } while (false)

        // clang-format off
        ENABLE_FEATURE(geometryShader,                    EngineCI.Features.GeometryShaders,                   "Geometry shaders are");
        ENABLE_FEATURE(tessellationShader,                EngineCI.Features.Tessellation,                      "Tessellation is");
        ENABLE_FEATURE(pipelineStatisticsQuery,           EngineCI.Features.PipelineStatisticsQueries,         "Pipeline statistics queries are");
        ENABLE_FEATURE(occlusionQueryPrecise,             EngineCI.Features.OcclusionQueries,                  "Occlusion queries are");
        ENABLE_FEATURE(imageCubeArray,                    DEVICE_FEATURE_STATE_OPTIONAL,                       "Image cube arrays");
        ENABLE_FEATURE(fillModeNonSolid,                  EngineCI.Features.WireframeFill,                     "Wireframe fill is");
        ENABLE_FEATURE(samplerAnisotropy,                 DEVICE_FEATURE_STATE_OPTIONAL,                       "Anisotropic texture filtering is");
        ENABLE_FEATURE(depthBiasClamp,                    EngineCI.Features.DepthBiasClamp,                    "Depth bias clamp is");
        ENABLE_FEATURE(depthClamp,                        EngineCI.Features.DepthClamp,                        "Depth clamp is");
        ENABLE_FEATURE(independentBlend,                  EngineCI.Features.IndependentBlend,                  "Independent blend is");
        ENABLE_FEATURE(dualSrcBlend,                      EngineCI.Features.DualSourceBlend,                   "Dual-source blend is");
        ENABLE_FEATURE(multiViewport,                     EngineCI.Features.MultiViewport,                     "Multiviewport is");
        ENABLE_FEATURE(textureCompressionBC,              EngineCI.Features.TextureCompressionBC,              "BC texture compression is");
        ENABLE_FEATURE(vertexPipelineStoresAndAtomics,    EngineCI.Features.VertexPipelineUAVWritesAndAtomics, "Vertex pipeline UAV writes and atomics are");
        ENABLE_FEATURE(fragmentStoresAndAtomics,          EngineCI.Features.PixelUAVWritesAndAtomics,          "Pixel UAV writes and atomics are");
        ENABLE_FEATURE(shaderStorageImageExtendedFormats, EngineCI.Features.TextureUAVExtendedFormats,         "Texture UAV extended formats are");
        // clang-format on
#undef ENABLE_FEATURE

        DeviceCreateInfo.pEnabledFeatures = &EnabledFeatures; // NULL or a pointer to a VkPhysicalDeviceFeatures structure that contains
                                                              // boolean indicators of all the features to be enabled.

        std::vector<const char*> DeviceExtensions =
            {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME // To allow negative viewport height
            };

        // To enable some device extensions you must enable instance extension VK_KHR_get_physical_device_properties2
        // and add feature description to DeviceCreateInfo.pNext.
        const auto SupportsFeatures2 = Instance->IsExtensionEnabled(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        // Enable mesh shader extension.
        bool                                 MeshShadersSupported = false;
        VkPhysicalDeviceMeshShaderFeaturesNV MeshShaderFeats      = {};
        if (SupportsFeatures2)
        {
            void** NextExt = const_cast<void**>(&DeviceCreateInfo.pNext);
            *NextExt       = nullptr;

            if (EngineCI.Features.MeshShaders != DEVICE_FEATURE_STATE_DISABLED)
            {
                MeshShaderFeats      = PhysicalDevice->GetExtFeatures().MeshShader;
                MeshShadersSupported = MeshShaderFeats.taskShader != VK_FALSE && MeshShaderFeats.meshShader != VK_FALSE;
                if (PhysicalDevice->IsExtensionSupported(VK_NV_MESH_SHADER_EXTENSION_NAME) && MeshShadersSupported)
                {
                    DeviceExtensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
                    *NextExt = &MeshShaderFeats;
                    NextExt  = &MeshShaderFeats.pNext;
                }
            }
        }

        if (EngineCI.Features.MeshShaders == DEVICE_FEATURE_STATE_ENABLED && !MeshShadersSupported)
            LOG_ERROR_AND_THROW("Mesh shaders are not supported by this device");

            // The actual state of Features.MeshShaders in device caps is set by VulkanLogicalDevice

#if defined(_MSC_VER) && defined(_WIN64)
        static_assert(sizeof(DeviceFeatures) == 23, "Did you add a new feature to DeviceFeatures? Please handle its satus here.");
#endif

        DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.empty() ? nullptr : DeviceExtensions.data();
        DeviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(DeviceExtensions.size());

        auto vkAllocator      = Instance->GetVkAllocator();
        auto vkPhysicalDevice = PhysicalDevice->GetVkDeviceHandle();
        auto LogicalDevice    = VulkanUtilities::VulkanLogicalDevice::Create(vkPhysicalDevice, DeviceCreateInfo, vkAllocator);

        auto& RawMemAllocator = GetRawAllocator();

        RefCntAutoPtr<CommandQueueVkImpl> pCmdQueueVk{
            NEW_RC_OBJ(RawMemAllocator, "CommandQueueVk instance", CommandQueueVkImpl)(LogicalDevice, QueueInfo.queueFamilyIndex)};

        OnRenderDeviceCreated = [&](RenderDeviceVkImpl* pRenderDeviceVk) //
        {
            FenceDesc Desc;
            Desc.Name = "Command queue internal fence";
            // Render device owns command queue that in turn owns the fence, so it is an internal device object
            constexpr bool IsDeviceInternal = true;

            RefCntAutoPtr<FenceVkImpl> pFenceVk{
                NEW_RC_OBJ(RawMemAllocator, "FenceVkImpl instance", FenceVkImpl)(pRenderDeviceVk, Desc, IsDeviceInternal)};
            pCmdQueueVk->SetFence(std::move(pFenceVk));
        };

        std::array<ICommandQueueVk*, 1> CommandQueues = {{pCmdQueueVk}};
        AttachToVulkanDevice(Instance, std::move(PhysicalDevice), LogicalDevice, CommandQueues.size(), CommandQueues.data(), EngineCI, ppDevice, ppContexts);
    }
    catch (std::runtime_error&)
    {
        return;
    }
}

/// Attaches to existing Vulkan device

/// \param [in] Instance - shared pointer to a VulkanUtilities::VulkanInstance object
/// \param [in] PhysicalDevice - pointer to the object representing physical device
/// \param [in] LogicalDevice - shared pointer to a VulkanUtilities::VulkanLogicalDevice object
/// \param [in] pCommandQueue - pointer to the implementation of command queue
/// \param [in] EngineCI - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to
///                           the contexts will be written. Immediate context goes at
///                           position 0. If EngineCI.NumDeferredContexts > 0,
///                           pointers to the deferred contexts are written afterwards.
void EngineFactoryVkImpl::AttachToVulkanDevice(std::shared_ptr<VulkanUtilities::VulkanInstance>       Instance,
                                               std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                               std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>  LogicalDevice,
                                               size_t                                                 CommandQueueCount,
                                               ICommandQueueVk**                                      ppCommandQueues,
                                               const EngineVkCreateInfo&                              EngineCI,
                                               IRenderDevice**                                        ppDevice,
                                               IDeviceContext**                                       ppContexts)
{
    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    if (EngineCI.APIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", EngineCI.APIVersion, ")");
        return;
    }

    VERIFY(ppCommandQueues && ppDevice && ppContexts, "Null pointer provided");
    if (!LogicalDevice || !ppCommandQueues || !ppDevice || !ppContexts)
        return;

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1 + EngineCI.NumDeferredContexts));

    try
    {
        auto& RawMemAllocator = GetRawAllocator();

        RenderDeviceVkImpl* pRenderDeviceVk(NEW_RC_OBJ(RawMemAllocator, "RenderDeviceVkImpl instance", RenderDeviceVkImpl)(RawMemAllocator, this, EngineCI, CommandQueueCount, ppCommandQueues, Instance, std::move(PhysicalDevice), LogicalDevice));
        pRenderDeviceVk->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice));

        if (OnRenderDeviceCreated != nullptr)
            OnRenderDeviceCreated(pRenderDeviceVk);

        std::shared_ptr<GenerateMipsVkHelper> GenerateMipsHelper(new GenerateMipsVkHelper(*pRenderDeviceVk));

        RefCntAutoPtr<DeviceContextVkImpl> pImmediateCtxVk(NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, false, EngineCI, 0, 0, GenerateMipsHelper));
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
        // keep a weak reference to the context
        pImmediateCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts));
        pRenderDeviceVk->SetImmediateContext(pImmediateCtxVk);

        for (Uint32 DeferredCtx = 0; DeferredCtx < EngineCI.NumDeferredContexts; ++DeferredCtx)
        {
            RefCntAutoPtr<DeviceContextVkImpl> pDeferredCtxVk(NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, true, EngineCI, 1 + DeferredCtx, 0, GenerateMipsHelper));
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
            // keep a weak reference to the context
            pDeferredCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + 1 + DeferredCtx));
            pRenderDeviceVk->SetDeferredContext(DeferredCtx, pDeferredCtxVk);
        }
    }
    catch (const std::runtime_error&)
    {
        if (*ppDevice)
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }
        for (Uint32 ctx = 0; ctx < 1 + EngineCI.NumDeferredContexts; ++ctx)
        {
            if (ppContexts[ctx] != nullptr)
            {
                ppContexts[ctx]->Release();
                ppContexts[ctx] = nullptr;
            }
        }

        LOG_ERROR("Failed to create device and contexts");
    }
}


void EngineFactoryVkImpl::CreateSwapChainVk(IRenderDevice*       pDevice,
                                            IDeviceContext*      pImmediateContext,
                                            const SwapChainDesc& SCDesc,
                                            const NativeWindow&  Window,
                                            ISwapChain**         ppSwapChain)
{
    VERIFY(ppSwapChain, "Null pointer provided");
    if (!ppSwapChain)
        return;

    *ppSwapChain = nullptr;

    try
    {
        auto* pDeviceVk        = ValidatedCast<RenderDeviceVkImpl>(pDevice);
        auto* pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pImmediateContext);
        auto& RawMemAllocator  = GetRawAllocator();

        auto* pSwapChainVk = NEW_RC_OBJ(RawMemAllocator, "SwapChainVkImpl instance", SwapChainVkImpl)(SCDesc, pDeviceVk, pDeviceContextVk, Window);
        pSwapChainVk->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain));
    }
    catch (const std::runtime_error&)
    {
        if (*ppSwapChain)
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR("Failed to create the swap chain");
    }
}

#if PLATFORM_ANDROID
void EngineFactoryVkImpl::InitAndroidFileSystem(struct ANativeActivity* NativeActivity,
                                                const char*             NativeActivityClassName,
                                                struct AAssetManager*   AssetManager) const
{
    AndroidFileSystem::Init(NativeActivity, NativeActivityClassName, AssetManager);
}
#endif

#ifdef DOXYGEN
/// Loads Direct3D12-based engine implementation and exports factory functions
///
/// return - Pointer to the function that returns factory for Vk engine implementation.
///          See Diligent::EngineFactoryVkImpl.
///
/// \remarks Depending on the configuration and platform, the function loads different dll:
///
/// Platform\\Configuration   |           Debug               |        Release
/// --------------------------|-------------------------------|----------------------------
///         x86               | GraphicsEngineVk_32d.dll   |    GraphicsEngineVk_32r.dll
///         x64               | GraphicsEngineVk_64d.dll   |    GraphicsEngineVk_64r.dll
///
GetEngineFactoryVkType LoadGraphicsEngineVk()
{
// This function is only required because DoxyGen refuses to generate documentation for a static function when SHOW_FILES==NO
#    error This function must never be compiled;
}
#endif

API_QUALIFIER
IEngineFactoryVk* GetEngineFactoryVk()
{
    return EngineFactoryVkImpl::GetInstance();
}

} // namespace Diligent
