/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "VulkanTypeConversions.hpp"

#if PLATFORM_ANDROID
#    include "FileSystem.hpp"
#endif

namespace Diligent
{
namespace
{

/// Engine factory for Vk implementation
class EngineFactoryVkImpl final : public EngineFactoryBase<IEngineFactoryVk>
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
    {
    }

    virtual void DILIGENT_CALL_TYPE CreateDeviceAndContextsVk(const EngineVkCreateInfo& EngineCI,
                                                              IRenderDevice**           ppDevice,
                                                              IDeviceContext**          ppContexts) override final;

    void AttachToVulkanDevice(std::shared_ptr<VulkanUtilities::VulkanInstance>       Instance,
                              std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                              std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>  LogicalDevice,
                              size_t                                                 CommandQueueCount,
                              ICommandQueueVk**                                      ppCommandQueues,
                              const EngineVkCreateInfo&                              EngineCI,
                              const GraphicsAdapterInfo&                             AdapterInfo,
                              IRenderDevice**                                        ppDevice,
                              IDeviceContext**                                       ppContexts);

    virtual void DILIGENT_CALL_TYPE CreateSwapChainVk(IRenderDevice*       pDevice,
                                                      IDeviceContext*      pImmediateContext,
                                                      const SwapChainDesc& SwapChainDesc,
                                                      const NativeWindow&  Window,
                                                      ISwapChain**         ppSwapChain) override final;

    virtual void DILIGENT_CALL_TYPE EnumerateAdapters(Version              MinVersion,
                                                      Uint32&              NumAdapters,
                                                      GraphicsAdapterInfo* Adapters) const override final;

#if PLATFORM_ANDROID
    virtual void InitAndroidFileSystem(struct ANativeActivity* NativeActivity,
                                       const char*             NativeActivityClassName,
                                       struct AAssetManager*   AssetManager) const override final;
#endif

private:
    std::function<void(RenderDeviceVkImpl*)> OnRenderDeviceCreated = nullptr;

    bool m_RenderDeviceCreated = false;
};


void GetPhysicalDeviceGraphicsAdapterInfo(const VulkanUtilities::VulkanPhysicalDevice& PhysicalDevice, GraphicsAdapterInfo& AdapterInfo)
{
    AdapterInfo = {};

    const uint32_t VkVersion = PhysicalDevice.GetVkVersion();

    // Set graphics adapter properties
    {
        const auto& DeviceProps = PhysicalDevice.GetProperties();

        AdapterInfo.Capabilities.DevType          = RENDER_DEVICE_TYPE_VULKAN;
        AdapterInfo.Capabilities.APIVersion.Major = static_cast<Uint8>(VK_VERSION_MAJOR(VkVersion));
        AdapterInfo.Capabilities.APIVersion.Minor = static_cast<Uint8>(VK_VERSION_MINOR(VkVersion));

        static_assert(_countof(AdapterInfo.Description) <= _countof(DeviceProps.deviceName), "");
        for (size_t i = 0; i < _countof(AdapterInfo.Description) - 1 && DeviceProps.deviceName[i] != 0; ++i)
            AdapterInfo.Description[i] = DeviceProps.deviceName[i];

        AdapterInfo.Type       = VkPhysicalDeviceTypeToAdapterType(DeviceProps.deviceType);
        AdapterInfo.Vendor     = VendorIdToAdapterVendor(DeviceProps.vendorID);
        AdapterInfo.VendorId   = DeviceProps.vendorID;
        AdapterInfo.DeviceId   = DeviceProps.deviceID;
        AdapterInfo.NumOutputs = 0;
    }

    // Enable features
    {
#define ENABLE_FEATURE(FeatureName, Supported) \
    Features.FeatureName = (Supported) ? DEVICE_FEATURE_STATE_OPTIONAL : DEVICE_FEATURE_STATE_DISABLED;

        const auto& DeviceFeatures    = PhysicalDevice.GetFeatures();
        const auto& DeviceExtFeatures = PhysicalDevice.GetExtFeatures();
        auto&       Features          = AdapterInfo.Capabilities.Features;

        // The following features are always enabled
        Features.SeparablePrograms             = DEVICE_FEATURE_STATE_ENABLED;
        Features.ShaderResourceQueries         = DEVICE_FEATURE_STATE_ENABLED;
        Features.IndirectRendering             = DEVICE_FEATURE_STATE_ENABLED;
        Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_ENABLED;
        Features.ComputeShaders                = DEVICE_FEATURE_STATE_ENABLED;
        Features.BindlessResources             = DEVICE_FEATURE_STATE_ENABLED;
        Features.BinaryOcclusionQueries        = DEVICE_FEATURE_STATE_ENABLED;
        Features.TimestampQueries              = DEVICE_FEATURE_STATE_ENABLED;
        Features.DurationQueries               = DEVICE_FEATURE_STATE_ENABLED;

        // clang-format off
        ENABLE_FEATURE(GeometryShaders,                   DeviceFeatures.geometryShader);
        ENABLE_FEATURE(Tessellation,                      DeviceFeatures.tessellationShader);
        ENABLE_FEATURE(PipelineStatisticsQueries,         DeviceFeatures.pipelineStatisticsQuery);
        ENABLE_FEATURE(OcclusionQueries,                  DeviceFeatures.occlusionQueryPrecise);
        ENABLE_FEATURE(WireframeFill,                     DeviceFeatures.fillModeNonSolid);
        ENABLE_FEATURE(DepthBiasClamp,                    DeviceFeatures.depthBiasClamp);
        ENABLE_FEATURE(DepthClamp,                        DeviceFeatures.depthClamp);
        ENABLE_FEATURE(IndependentBlend,                  DeviceFeatures.independentBlend);
        ENABLE_FEATURE(DualSourceBlend,                   DeviceFeatures.dualSrcBlend);
        ENABLE_FEATURE(MultiViewport,                     DeviceFeatures.multiViewport);
        ENABLE_FEATURE(TextureCompressionBC,              DeviceFeatures.textureCompressionBC);
        ENABLE_FEATURE(VertexPipelineUAVWritesAndAtomics, DeviceFeatures.vertexPipelineStoresAndAtomics);
        ENABLE_FEATURE(PixelUAVWritesAndAtomics,          DeviceFeatures.fragmentStoresAndAtomics);
        ENABLE_FEATURE(TextureUAVExtendedFormats,         DeviceFeatures.shaderStorageImageExtendedFormats);
        // clang-format on

        const auto& MeshShaderFeats = DeviceExtFeatures.MeshShader;
        ENABLE_FEATURE(MeshShaders, MeshShaderFeats.taskShader != VK_FALSE && MeshShaderFeats.meshShader != VK_FALSE);

        const auto& ShaderFloat16Int8Feats = DeviceExtFeatures.ShaderFloat16Int8;
        // clang-format off
        ENABLE_FEATURE(ShaderFloat16, ShaderFloat16Int8Feats.shaderFloat16 != VK_FALSE);
        ENABLE_FEATURE(ShaderInt8,    ShaderFloat16Int8Feats.shaderInt8    != VK_FALSE);
        // clang-format on

        const auto& Storage16BitFeats = DeviceExtFeatures.Storage16Bit;
        // clang-format off
        ENABLE_FEATURE(ResourceBuffer16BitAccess, Storage16BitFeats.storageBuffer16BitAccess           != VK_FALSE && DeviceFeatures.shaderInt16 != VK_FALSE);
        ENABLE_FEATURE(UniformBuffer16BitAccess,  Storage16BitFeats.uniformAndStorageBuffer16BitAccess != VK_FALSE && DeviceFeatures.shaderInt16 != VK_FALSE);
        ENABLE_FEATURE(ShaderInputOutput16,       Storage16BitFeats.storageInputOutput16               != VK_FALSE && DeviceFeatures.shaderInt16 != VK_FALSE);
        // clang-format on

        const auto& Storage8BitFeats = DeviceExtFeatures.Storage8Bit;
        // clang-format off
        ENABLE_FEATURE(ResourceBuffer8BitAccess, Storage8BitFeats.storageBuffer8BitAccess           != VK_FALSE);
        ENABLE_FEATURE(UniformBuffer8BitAccess,  Storage8BitFeats.uniformAndStorageBuffer8BitAccess != VK_FALSE);
        // clang-format on

        const auto& DescrIndexingFeats = DeviceExtFeatures.DescriptorIndexing;
        ENABLE_FEATURE(ShaderResourceRuntimeArray, DescrIndexingFeats.runtimeDescriptorArray != VK_FALSE);
        const auto& AccelStructFeats = DeviceExtFeatures.AccelStruct;
        const auto& RayTracingFeats  = DeviceExtFeatures.RayTracingPipeline;
        const auto& RayQueryFeats    = DeviceExtFeatures.RayQuery;
        // clang-format off
        ENABLE_FEATURE(RayTracing,
                        VkVersion                              >= VK_API_VERSION_1_1 &&
                        AccelStructFeats.accelerationStructure != VK_FALSE           &&
                        RayTracingFeats.rayTracingPipeline     != VK_FALSE);
        ENABLE_FEATURE(RayTracing2,
                        VkVersion                                           >= VK_API_VERSION_1_1 &&
                        AccelStructFeats.accelerationStructure              != VK_FALSE           &&
                        RayTracingFeats.rayTracingPipeline                  != VK_FALSE           &&
                        RayTracingFeats.rayTracingPipelineTraceRaysIndirect != VK_FALSE           &&
                        RayTracingFeats.rayTraversalPrimitiveCulling        != VK_FALSE           &&
                        RayQueryFeats.rayQuery                              != VK_FALSE);
        // clang-format on

        const auto& SubgroupProps          = PhysicalDevice.GetExtProperties().Subgroup;
        const auto  RequiredSubgroupFeats  = VK_SUBGROUP_FEATURE_BASIC_BIT;
        const auto  RequiredSubgroupStages = VK_SHADER_STAGE_COMPUTE_BIT;
        ENABLE_FEATURE(WaveOp,
                       (VkVersion >= VK_API_VERSION_1_1 &&
                        (SubgroupProps.supportedOperations & RequiredSubgroupFeats) == RequiredSubgroupFeats &&
                        (SubgroupProps.supportedStages & RequiredSubgroupStages) == RequiredSubgroupStages));

        const auto& VertexAttribDivisorFeats = PhysicalDevice.GetExtFeatures().VertexAttributeDivisor;
        ENABLE_FEATURE(InstanceDataStepRate,
                       (VertexAttribDivisorFeats.vertexAttributeInstanceRateDivisor != VK_FALSE &&
                        VertexAttribDivisorFeats.vertexAttributeInstanceRateZeroDivisor != VK_FALSE));
#undef ENABLE_FEATURE
    }

    // Set texture and sampler capabilities
    {
        const auto& vkDeviceLimits = PhysicalDevice.GetProperties().limits;
        const auto& vkFeatures     = PhysicalDevice.GetFeatures();

        auto& TexCaps = AdapterInfo.Capabilities.TexCaps;

        TexCaps.MaxTexture1DDimension     = vkDeviceLimits.maxImageDimension1D;
        TexCaps.MaxTexture1DArraySlices   = vkDeviceLimits.maxImageArrayLayers;
        TexCaps.MaxTexture2DDimension     = vkDeviceLimits.maxImageDimension2D;
        TexCaps.MaxTexture2DArraySlices   = vkDeviceLimits.maxImageArrayLayers;
        TexCaps.MaxTexture3DDimension     = vkDeviceLimits.maxImageDimension3D;
        TexCaps.MaxTextureCubeDimension   = vkDeviceLimits.maxImageDimensionCube;
        TexCaps.Texture2DMSSupported      = True;
        TexCaps.Texture2DMSArraySupported = True;
        TexCaps.TextureViewSupported      = True;
        TexCaps.CubemapArraysSupported    = vkFeatures.imageCubeArray;

        auto& SamCaps = AdapterInfo.Capabilities.SamCaps;

        SamCaps.BorderSamplingModeSupported   = True;
        SamCaps.AnisotropicFilteringSupported = vkFeatures.samplerAnisotropy;
        SamCaps.LODBiasSupported              = True;
    }

    // Set properties
    {
        const auto& Features   = AdapterInfo.Capabilities.Features;
        auto&       Properties = AdapterInfo.Properties;

        if (Features.RayTracing)
        {
            Properties.MaxRayTracingRecursionDepth = PhysicalDevice.GetExtProperties().RayTracingPipeline.maxRayRecursionDepth;
        }
        if (Features.WaveOp)
        {
            const auto& vkWaveProps           = PhysicalDevice.GetExtProperties().Subgroup;
            Properties.WaveOp.MinSize         = vkWaveProps.subgroupSize;
            Properties.WaveOp.MaxSize         = vkWaveProps.subgroupSize;
            Properties.WaveOp.SupportedStages = VkShaderStageFlagsToShaderTypes(vkWaveProps.supportedStages);
            Properties.WaveOp.Features        = VkSubgroupFeatureFlagsToWaveFeatures(vkWaveProps.supportedOperations);
        }
    }

    // Set memory properties
    {
        auto& Mem = AdapterInfo.Memory;

        Mem.DeviceLocalMemory  = 0;
        Mem.HostVisibileMemory = 0;
        Mem.UnifiedMemory      = 0;

        std::bitset<VK_MAX_MEMORY_HEAPS> DeviceLocalHeap;
        std::bitset<VK_MAX_MEMORY_HEAPS> HostVisibleHeap;
        std::bitset<VK_MAX_MEMORY_HEAPS> UnifiedHeap;

        const auto& MemoryProps = PhysicalDevice.GetMemoryProperties();
        for (uint32_t type = 0; type < MemoryProps.memoryTypeCount; ++type)
        {
            const auto&    MemTypeInfo        = MemoryProps.memoryTypes[type];
            constexpr auto UnifiedMemoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

            if ((MemTypeInfo.propertyFlags & UnifiedMemoryFlags) == UnifiedMemoryFlags)
            {
                UnifiedHeap[MemTypeInfo.heapIndex] = true;
                if (MemTypeInfo.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    Mem.UnifiedMemoryCPUAccess |= CPU_ACCESS_WRITE;
                if (MemTypeInfo.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                    Mem.UnifiedMemoryCPUAccess |= CPU_ACCESS_READ;
            }
            else if (MemTypeInfo.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                DeviceLocalHeap[MemTypeInfo.heapIndex] = true;
            }
            else if (MemTypeInfo.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                HostVisibleHeap[MemTypeInfo.heapIndex] = true;
            }
        }

        for (uint32_t heap = 0; heap < MemoryProps.memoryHeapCount; ++heap)
        {
            const auto& HeapInfo = MemoryProps.memoryHeaps[heap];

            if (UnifiedHeap[heap])
                Mem.UnifiedMemory += static_cast<Uint64>(HeapInfo.size);
            else if (DeviceLocalHeap[heap])
                Mem.DeviceLocalMemory += static_cast<Uint64>(HeapInfo.size);
            else if (HostVisibleHeap[heap])
                Mem.HostVisibileMemory += static_cast<Uint64>(HeapInfo.size);
        }
    }

    // Set queue info
    {
        const auto& QueueProperties = PhysicalDevice.GetQueueProperties();
        AdapterInfo.NumQueues       = std::min(MAX_ADAPTER_QUEUES, static_cast<Uint32>(QueueProperties.size()));

        for (Uint32 q = 0; q < AdapterInfo.NumQueues; ++q)
        {
            const auto& SrcQueue = QueueProperties[q];
            auto&       DstQueue = AdapterInfo.Queues[q];

            DstQueue.QueueType                 = VkQueueFlagsToContextType(SrcQueue.queueFlags);
            DstQueue.MaxDeviceContexts         = SrcQueue.queueCount;
            DstQueue.TextureCopyGranularity[0] = SrcQueue.minImageTransferGranularity.width;
            DstQueue.TextureCopyGranularity[1] = SrcQueue.minImageTransferGranularity.height;
            DstQueue.TextureCopyGranularity[2] = SrcQueue.minImageTransferGranularity.depth;
        }
    }

    // Set limits
    {
        auto&       Limits         = AdapterInfo.Limits;
        const auto& vkDeviceLimits = PhysicalDevice.GetProperties().limits;

        Limits.ConstantBufferOffsetAlignment   = static_cast<Uint32>(vkDeviceLimits.minUniformBufferOffsetAlignment);
        Limits.StructuredBufferOffsetAlignment = static_cast<Uint32>(vkDeviceLimits.minStorageBufferOffsetAlignment);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(DeviceFeatures) == 36, "Did you add a new feature to DeviceFeatures? Please handle its satus here (if necessary).");
    static_assert(sizeof(DeviceProperties) == 20, "Did you add a new peroperties to DeviceProperties? Please handle its satus here.");
    static_assert(sizeof(DeviceLimits) == 8, "Did you add a new member to DeviceLimits? Please handle it here (if necessary).");
#endif
}

void EngineFactoryVkImpl::EnumerateAdapters(Version              MinVersion,
                                            Uint32&              NumAdapters,
                                            GraphicsAdapterInfo* Adapters) const
{
    if (m_RenderDeviceCreated)
    {
        LOG_ERROR_MESSAGE("We have global pointers to Vulkan functions and can not simultaniously use more than one instance and logical device.");
        NumAdapters = 0;
        return;
    }

    // Create instance with maximum available version.
    // If Volk is not enabled then version will be 1.0
    const uint32_t APIVersion = VK_MAKE_VERSION(0xFF, 0xFF, 0);
    auto           Instance   = VulkanUtilities::VulkanInstance::Create(APIVersion, false, 0, nullptr, nullptr);

    if (Adapters == nullptr)
    {
        NumAdapters = static_cast<Uint32>(Instance->GetVkPhysicalDevices().size());
        return;
    }

    NumAdapters = std::min(NumAdapters, static_cast<Uint32>(Instance->GetVkPhysicalDevices().size()));
    for (Uint32 i = 0; i < NumAdapters; ++i)
    {
        auto PhysicalDevice = VulkanUtilities::VulkanPhysicalDevice::Create(Instance->GetVkPhysicalDevices()[i], *Instance);
        GetPhysicalDeviceGraphicsAdapterInfo(*PhysicalDevice, Adapters[i]);
    }
}

void EngineFactoryVkImpl::CreateDeviceAndContextsVk(const EngineVkCreateInfo& _EngineCI,
                                                    IRenderDevice**           ppDevice,
                                                    IDeviceContext**          ppContexts)
{
    if (_EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(_EngineCI.DebugMessageCallback);

    if (_EngineCI.EngineAPIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", _EngineCI.EngineAPIVersion, ")");
        return;
    }

    VERIFY(ppDevice && ppContexts, "Null pointer provided");
    if (!ppDevice || !ppContexts)
        return;

    if (m_RenderDeviceCreated)
    {
        LOG_ERROR_MESSAGE("We have global pointers to Vulkan functions and can not simultaniously use more than one instance and logical device.");
        return;
    }

    EngineVkCreateInfo EngineCI = _EngineCI;

    SetRawAllocator(EngineCI.pRawMemAllocator);
    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (std::max(1u, EngineCI.NumContexts) + EngineCI.NumDeferredContexts));

    try
    {
        if (EngineCI.GraphicsAPIVersion == Version{0, 0})
        {
            // Instance will use maximum available version
            EngineCI.GraphicsAPIVersion = Version{0xFF, 0xFF};
        }

        auto Instance = VulkanUtilities::VulkanInstance::Create(
            VK_MAKE_VERSION(EngineCI.GraphicsAPIVersion.Major, EngineCI.GraphicsAPIVersion.Minor, 0),
            EngineCI.EnableValidation,
            EngineCI.InstanceExtensionCount,
            EngineCI.ppInstanceExtensionNames,
            reinterpret_cast<VkAllocationCallbacks*>(EngineCI.pVkAllocator));

        auto vkDevice       = Instance->SelectPhysicalDevice(EngineCI.AdapterId);
        auto PhysicalDevice = VulkanUtilities::VulkanPhysicalDevice::Create(vkDevice, *Instance);

        // Enable device feature if they are supported and throw error if not supported but required by user.
        GraphicsAdapterInfo AdapterInfo;
        GetPhysicalDeviceGraphicsAdapterInfo(*PhysicalDevice, AdapterInfo);
        EnableDeviceFeatures(AdapterInfo.Capabilities.Features, EngineCI.Features);
        AdapterInfo.Capabilities.Features = EngineCI.Features;

        VerifyEngineCreateInfo(EngineCI, AdapterInfo);

        std::vector<VkDeviceQueueGlobalPriorityCreateInfoEXT> QueueGlobalPriority;
        std::vector<VkDeviceQueueCreateInfo>                  QueueInfos;
        std::vector<float>                                    QueuePriorities;
        std::array<Uint8, MAX_ADAPTER_QUEUES>                 QueueIDtoQueueInfo;
        std::array<QUEUE_PRIORITY, MAX_ADAPTER_QUEUES>        QueueIDtoPriority;
        QueueIDtoQueueInfo.fill(DEFAULT_QUEUE_ID);
        QueueIDtoPriority.fill(QUEUE_PRIORITY_UNKNOWN);

        // Setup device queues
        if (EngineCI.NumContexts > 0)
        {
            VERIFY(EngineCI.pContextInfo != nullptr, "Must be verified in VerifyEngineCreateInfo()");

            const auto& QueueProperties = PhysicalDevice->GetQueueProperties();
            QueuePriorities.resize(EngineCI.NumContexts, 1.0f);

            for (Uint32 CtxInd = 0; CtxInd < EngineCI.NumContexts; ++CtxInd)
            {
                const auto& ContextInfo = EngineCI.pContextInfo[CtxInd];
                VERIFY(ContextInfo.QueueId < QueueProperties.size() && ContextInfo.QueueId < QueueIDtoQueueInfo.size(),
                       "Must be verified in VerifyEngineCreateInfo()");

                auto& QueueIndex = QueueIDtoQueueInfo[ContextInfo.QueueId];
                if (QueueIndex == DEFAULT_QUEUE_ID)
                {
                    QueueIndex = static_cast<Uint8>(QueueInfos.size());

                    VkDeviceQueueCreateInfo QueueCI = {};
                    QueueCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    QueueCI.pNext                   = nullptr;
                    QueueCI.flags                   = 0; // reserved for future use
                    QueueCI.queueFamilyIndex        = ContextInfo.QueueId;
                    QueueCI.queueCount              = 0;
                    QueueCI.pQueuePriorities        = QueuePriorities.data();
                    QueueInfos.push_back(QueueCI);
                }
                QueueInfos[QueueIndex].queueCount += 1;

                auto& Priority = QueueIDtoPriority[QueueIndex];
                if (Priority != QUEUE_PRIORITY_UNKNOWN && Priority != ContextInfo.Priority)
                    LOG_ERROR_AND_THROW("Context priority for all contexts with QueueId must be the same");
                Priority = ContextInfo.Priority;
            }

            if (Instance->IsExtensionEnabled(VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME))
            {
                QueueGlobalPriority.resize(QueueInfos.size());
                for (Uint32 QInd = 0; QInd < QueueInfos.size(); ++QInd)
                {
                    auto& QPriority          = QueueGlobalPriority[QInd];
                    QPriority.sType          = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT;
                    QPriority.pNext          = nullptr;
                    QPriority.globalPriority = QueuePriorityToVkQueueGlobalPriority(QueueIDtoPriority[QInd]);
                    QueueInfos[QInd].pNext   = &QPriority;
                }
            }
        }
        else
        {
            QueueInfos.resize(1);
            QueuePriorities.resize(1);

            auto& QueueCI         = QueueInfos[0];
            QueuePriorities[0]    = 1.0f; // Ask for highest priority for our queue. (range [0,1])
            QueueIDtoQueueInfo[0] = 0;

            // If an implementation exposes any queue family that supports graphics operations,
            // at least one queue family of at least one physical device exposed by the implementation
            // must support both graphics and compute operations.

            QueueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCI.flags            = 0; // reserved for future use
            QueueCI.queueFamilyIndex = PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
            QueueCI.queueCount       = 1;
            QueueCI.pQueuePriorities = QueuePriorities.data();
        }

        VkDeviceCreateInfo DeviceCreateInfo = {};
        DeviceCreateInfo.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DeviceCreateInfo.flags              = 0; // Reserved for future use
        // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#extended-functionality-device-layer-deprecation
        DeviceCreateInfo.enabledLayerCount    = 0;       // Deprecated and ignored.
        DeviceCreateInfo.ppEnabledLayerNames  = nullptr; // Deprecated and ignored
        DeviceCreateInfo.queueCreateInfoCount = static_cast<Uint32>(QueueInfos.size());
        DeviceCreateInfo.pQueueCreateInfos    = QueueInfos.data();

        const auto&              DeviceFeatures  = PhysicalDevice->GetFeatures();
        VkPhysicalDeviceFeatures EnabledFeatures = {};
        DeviceCreateInfo.pEnabledFeatures        = &EnabledFeatures;

#define ENABLE_VKFEATURE(vkFeature, State) \
    EnabledFeatures.vkFeature = (State == DEVICE_FEATURE_STATE_ENABLED ? VK_TRUE : VK_FALSE);

        auto ImageCubeArrayFeature    = DEVICE_FEATURE_STATE_OPTIONAL;
        auto SamplerAnisotropyFeature = DEVICE_FEATURE_STATE_OPTIONAL;
        // clang-format off
        ENABLE_VKFEATURE(geometryShader,                    EngineCI.Features.GeometryShaders);
        ENABLE_VKFEATURE(tessellationShader,                EngineCI.Features.Tessellation);
        ENABLE_VKFEATURE(pipelineStatisticsQuery,           EngineCI.Features.PipelineStatisticsQueries);
        ENABLE_VKFEATURE(occlusionQueryPrecise,             EngineCI.Features.OcclusionQueries);
        ENABLE_VKFEATURE(imageCubeArray,                    ImageCubeArrayFeature);
        ENABLE_VKFEATURE(fillModeNonSolid,                  EngineCI.Features.WireframeFill);
        ENABLE_VKFEATURE(samplerAnisotropy,                 SamplerAnisotropyFeature);
        ENABLE_VKFEATURE(depthBiasClamp,                    EngineCI.Features.DepthBiasClamp);
        ENABLE_VKFEATURE(depthClamp,                        EngineCI.Features.DepthClamp);
        ENABLE_VKFEATURE(independentBlend,                  EngineCI.Features.IndependentBlend);
        ENABLE_VKFEATURE(dualSrcBlend,                      EngineCI.Features.DualSourceBlend);
        ENABLE_VKFEATURE(multiViewport,                     EngineCI.Features.MultiViewport);
        ENABLE_VKFEATURE(textureCompressionBC,              EngineCI.Features.TextureCompressionBC);
        ENABLE_VKFEATURE(vertexPipelineStoresAndAtomics,    EngineCI.Features.VertexPipelineUAVWritesAndAtomics);
        ENABLE_VKFEATURE(fragmentStoresAndAtomics,          EngineCI.Features.PixelUAVWritesAndAtomics);
        ENABLE_VKFEATURE(shaderStorageImageExtendedFormats, EngineCI.Features.TextureUAVExtendedFormats);
        // clang-format on
#undef ENABLE_VKFEATURE

        // Enable features (if they are supported) that are not covered by DeviceFeatures but required for some operations.
        EnabledFeatures.imageCubeArray                          = DeviceFeatures.imageCubeArray;
        EnabledFeatures.samplerAnisotropy                       = DeviceFeatures.samplerAnisotropy;
        EnabledFeatures.fullDrawIndexUint32                     = DeviceFeatures.fullDrawIndexUint32;
        EnabledFeatures.multiDrawIndirect                       = DeviceFeatures.multiDrawIndirect;
        EnabledFeatures.drawIndirectFirstInstance               = DeviceFeatures.drawIndirectFirstInstance;
        EnabledFeatures.shaderStorageImageWriteWithoutFormat    = DeviceFeatures.shaderStorageImageWriteWithoutFormat;
        EnabledFeatures.shaderUniformBufferArrayDynamicIndexing = DeviceFeatures.shaderUniformBufferArrayDynamicIndexing;
        EnabledFeatures.shaderSampledImageArrayDynamicIndexing  = DeviceFeatures.shaderSampledImageArrayDynamicIndexing;
        EnabledFeatures.shaderStorageBufferArrayDynamicIndexing = DeviceFeatures.shaderStorageBufferArrayDynamicIndexing;
        EnabledFeatures.shaderStorageImageArrayDynamicIndexing  = DeviceFeatures.shaderStorageImageArrayDynamicIndexing;

        std::vector<const char*> DeviceExtensions =
            {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME // To allow negative viewport height
            };

        using ExtensionFeatures                    = VulkanUtilities::VulkanPhysicalDevice::ExtensionFeatures;
        const ExtensionFeatures& DeviceExtFeatures = PhysicalDevice->GetExtFeatures();
        ExtensionFeatures        EnabledExtFeats   = {};

        // To enable some device extensions you must enable instance extension VK_KHR_get_physical_device_properties2
        // and add feature description to DeviceCreateInfo.pNext.
        const bool SupportsFeatures2 = Instance->IsExtensionEnabled(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        // Enable extensions
        if (SupportsFeatures2)
        {
            void** NextExt = const_cast<void**>(&DeviceCreateInfo.pNext);

            // Mesh shader
            if (EngineCI.Features.MeshShaders != DEVICE_FEATURE_STATE_DISABLED)
            {
                EnabledExtFeats.MeshShader = DeviceExtFeatures.MeshShader;
                VERIFY_EXPR(EnabledExtFeats.MeshShader.taskShader != VK_FALSE && EnabledExtFeats.MeshShader.meshShader != VK_FALSE);
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_NV_MESH_SHADER_EXTENSION_NAME),
                       "VK_NV_mesh_shader extension must be supported as it has already been checked by VulkanPhysicalDevice and "
                       "both taskShader and meshShader features are TRUE");
                DeviceExtensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
                *NextExt = &EnabledExtFeats.MeshShader;
                NextExt  = &EnabledExtFeats.MeshShader.pNext;
            }

            if (EngineCI.Features.ShaderFloat16 != DEVICE_FEATURE_STATE_DISABLED ||
                EngineCI.Features.ShaderInt8 != DEVICE_FEATURE_STATE_DISABLED)
            {
                EnabledExtFeats.ShaderFloat16Int8 = DeviceExtFeatures.ShaderFloat16Int8;
                VERIFY_EXPR(EnabledExtFeats.ShaderFloat16Int8.shaderFloat16 != VK_FALSE || EnabledExtFeats.ShaderFloat16Int8.shaderInt8 != VK_FALSE);
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME),
                       "VK_KHR_shader_float16_int8 extension must be supported as it has already been checked by VulkanPhysicalDevice "
                       "and at least one of shaderFloat16 or shaderInt8 features is TRUE");
                DeviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

                if (EngineCI.Features.ShaderFloat16 == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.ShaderFloat16Int8.shaderFloat16 = VK_FALSE;
                if (EngineCI.Features.ShaderInt8 == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.ShaderFloat16Int8.shaderInt8 = VK_FALSE;

                *NextExt = &EnabledExtFeats.ShaderFloat16Int8;
                NextExt  = &EnabledExtFeats.ShaderFloat16Int8.pNext;
            }

            bool StorageBufferStorageClassExtensionRequired = false;

            // clang-format off
            if (EngineCI.Features.ResourceBuffer16BitAccess != DEVICE_FEATURE_STATE_DISABLED ||
                EngineCI.Features.UniformBuffer16BitAccess  != DEVICE_FEATURE_STATE_DISABLED ||
                EngineCI.Features.ShaderInputOutput16       != DEVICE_FEATURE_STATE_DISABLED)
            // clang-format on
            {
                // clang-format off
                EnabledExtFeats.Storage16Bit = DeviceExtFeatures.Storage16Bit;
                VERIFY_EXPR(EngineCI.Features.ResourceBuffer16BitAccess == DEVICE_FEATURE_STATE_DISABLED || EnabledExtFeats.Storage16Bit.storageBuffer16BitAccess           != VK_FALSE);
                VERIFY_EXPR(EngineCI.Features.UniformBuffer16BitAccess  == DEVICE_FEATURE_STATE_DISABLED || EnabledExtFeats.Storage16Bit.uniformAndStorageBuffer16BitAccess != VK_FALSE);
                VERIFY_EXPR(EngineCI.Features.ShaderInputOutput16       == DEVICE_FEATURE_STATE_DISABLED || EnabledExtFeats.Storage16Bit.storageInputOutput16               != VK_FALSE);
                // clang-format on

                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_16BIT_STORAGE_EXTENSION_NAME),
                       "VK_KHR_16bit_storage must be supported as it has already been checked by VulkanPhysicalDevice and at least one of "
                       "storageBuffer16BitAccess, uniformAndStorageBuffer16BitAccess, or storagePushConstant16 features is TRUE");
                DeviceExtensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);

                // VK_KHR_16bit_storage extension requires VK_KHR_storage_buffer_storage_class extension.
                // All required extensions for each extension in the VkDeviceCreateInfo::ppEnabledExtensionNames
                // list must also be present in that list.
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME),
                       "VK_KHR_storage_buffer_storage_class must be supported as it has already been checked by VulkanPhysicalDevice and at least one of "
                       "storageBuffer16BitAccess, uniformAndStorageBuffer16BitAccess, or storagePushConstant16 features is TRUE");
                StorageBufferStorageClassExtensionRequired = true;

                EnabledFeatures.shaderInt16 = VK_TRUE;
                if (EngineCI.Features.ResourceBuffer16BitAccess == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.Storage16Bit.storageBuffer16BitAccess = VK_FALSE;
                if (EngineCI.Features.UniformBuffer16BitAccess == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.Storage16Bit.uniformAndStorageBuffer16BitAccess = VK_FALSE;
                if (EngineCI.Features.ShaderInputOutput16 == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.Storage16Bit.storageInputOutput16 = VK_FALSE;

                *NextExt = &EnabledExtFeats.Storage16Bit;
                NextExt  = &EnabledExtFeats.Storage16Bit.pNext;
            }

            // clang-format off
            if (EngineCI.Features.ResourceBuffer8BitAccess != DEVICE_FEATURE_STATE_DISABLED ||
                EngineCI.Features.UniformBuffer8BitAccess  != DEVICE_FEATURE_STATE_DISABLED)
            // clang-format on
            {
                // clang-format off
                EnabledExtFeats.Storage8Bit = DeviceExtFeatures.Storage8Bit;
                VERIFY_EXPR(EngineCI.Features.ResourceBuffer8BitAccess == DEVICE_FEATURE_STATE_DISABLED || EnabledExtFeats.Storage8Bit.storageBuffer8BitAccess           != VK_FALSE);
                VERIFY_EXPR(EngineCI.Features.UniformBuffer8BitAccess  == DEVICE_FEATURE_STATE_DISABLED || EnabledExtFeats.Storage8Bit.uniformAndStorageBuffer8BitAccess != VK_FALSE);
                // clang-format on

                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_8BIT_STORAGE_EXTENSION_NAME),
                       "VK_KHR_8bit_storage must be supported as it has already been checked by VulkanPhysicalDevice and at least one of "
                       "storageBuffer8BitAccess or uniformAndStorageBuffer8BitAccess features is TRUE");
                DeviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);

                // VK_KHR_8bit_storage extension requires VK_KHR_storage_buffer_storage_class extension.
                // All required extensions for each extension in the VkDeviceCreateInfo::ppEnabledExtensionNames
                // list must also be present in that list.
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME),
                       "VK_KHR_storage_buffer_storage_class must be supported as it has already been checked by VulkanPhysicalDevice and at least one of "
                       "storageBuffer8BitAccess or uniformAndStorageBuffer8BitAccess features is TRUE");
                StorageBufferStorageClassExtensionRequired = true;

                if (EngineCI.Features.ResourceBuffer8BitAccess == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.Storage8Bit.storageBuffer8BitAccess = VK_FALSE;
                if (EngineCI.Features.UniformBuffer8BitAccess == DEVICE_FEATURE_STATE_DISABLED)
                    EnabledExtFeats.Storage8Bit.uniformAndStorageBuffer8BitAccess = VK_FALSE;

                *NextExt = &EnabledExtFeats.Storage8Bit;
                NextExt  = &EnabledExtFeats.Storage8Bit.pNext;
            }

            if (StorageBufferStorageClassExtensionRequired)
            {
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME), "VK_KHR_storage_buffer_storage_class extension must be supported");
                DeviceExtensions.push_back(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);
            }

            // clang-format off
            if (EngineCI.Features.ShaderResourceRuntimeArray != DEVICE_FEATURE_STATE_DISABLED ||
                EngineCI.Features.RayTracing                 != DEVICE_FEATURE_STATE_DISABLED)
            // clang-format on
            {
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_MAINTENANCE3_EXTENSION_NAME), "VK_KHR_maintenance3 extension must be supported");
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_MAINTENANCE3_EXTENSION_NAME), "VK_EXT_descriptor_indexing extension must be supported");
                DeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME); // required for VK_EXT_descriptor_indexing
                DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

                EnabledExtFeats.DescriptorIndexing = DeviceExtFeatures.DescriptorIndexing;
                VERIFY_EXPR(EnabledExtFeats.DescriptorIndexing.runtimeDescriptorArray != VK_FALSE);

                *NextExt = &EnabledExtFeats.DescriptorIndexing;
                NextExt  = &EnabledExtFeats.DescriptorIndexing.pNext;
            }

            // Ray tracing
            if (EngineCI.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED || EngineCI.Features.RayTracing2 != DEVICE_FEATURE_STATE_DISABLED)
            {
                // this extensions added to Vulkan 1.2 core
                if (!DeviceExtFeatures.Spirv15)
                {
                    VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME), "VK_KHR_shader_float_controls extension must be supported");
                    VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_SPIRV_1_4_EXTENSION_NAME), "VK_KHR_spirv_1_4 extension must be supported");
                    DeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME); // required for VK_KHR_spirv_1_4
                    DeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);             // required for VK_KHR_ray_tracing_pipeline or VK_KHR_ray_query
                    EnabledExtFeats.Spirv14 = DeviceExtFeatures.Spirv14;
                    VERIFY_EXPR(DeviceExtFeatures.Spirv14);
                }

                // SPIRV 1.5 is in Vulkan 1.2 core
                EnabledExtFeats.Spirv15 = DeviceExtFeatures.Spirv15;

                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME), "VK_KHR_buffer_device_address extension must be supported");
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME), "VK_KHR_deferred_host_operations extension must be supported");
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), "VK_KHR_acceleration_structure extension must be supported");
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME), "VK_KHR_ray_tracing_pipeline extension must be supported");
                DeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);    // required for VK_KHR_acceleration_structure
                DeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // required for VK_KHR_acceleration_structure
                DeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);   // required for ray tracing
                DeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);     // required for ray tracing

                EnabledExtFeats.AccelStruct         = DeviceExtFeatures.AccelStruct;
                EnabledExtFeats.RayTracingPipeline  = DeviceExtFeatures.RayTracingPipeline;
                EnabledExtFeats.BufferDeviceAddress = DeviceExtFeatures.BufferDeviceAddress;

                // disable unused features
                EnabledExtFeats.AccelStruct.accelerationStructureCaptureReplay                    = false;
                EnabledExtFeats.AccelStruct.accelerationStructureHostCommands                     = false;
                EnabledExtFeats.AccelStruct.descriptorBindingAccelerationStructureUpdateAfterBind = false;

                EnabledExtFeats.RayTracingPipeline.rayTracingPipelineShaderGroupHandleCaptureReplay      = false;
                EnabledExtFeats.RayTracingPipeline.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = false;

                *NextExt = &EnabledExtFeats.AccelStruct;
                NextExt  = &EnabledExtFeats.AccelStruct.pNext;
                *NextExt = &EnabledExtFeats.RayTracingPipeline;
                NextExt  = &EnabledExtFeats.RayTracingPipeline.pNext;
                *NextExt = &EnabledExtFeats.BufferDeviceAddress;
                NextExt  = &EnabledExtFeats.BufferDeviceAddress.pNext;

                // Inline ray tracing from any shader.
                if (EngineCI.Features.RayTracing2 != DEVICE_FEATURE_STATE_DISABLED)
                {
                    VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME), "VK_KHR_ray_query extension must be supported");
                    DeviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

                    EnabledExtFeats.RayQuery = DeviceExtFeatures.RayQuery;

                    *NextExt = &EnabledExtFeats.RayQuery;
                    NextExt  = &EnabledExtFeats.RayQuery.pNext;
                }
                else
                {
                    EnabledExtFeats.AccelStruct.accelerationStructureIndirectBuild         = false;
                    EnabledExtFeats.RayTracingPipeline.rayTracingPipelineTraceRaysIndirect = false;
                    EnabledExtFeats.RayTracingPipeline.rayTraversalPrimitiveCulling        = false; // for GLSL_EXT_ray_flags_primitive_culling
                }
            }

#ifdef PLATFORM_MACOS
            if (DeviceExtFeatures.HasPortabilitySubset)
            {
                EnabledExtFeats.HasPortabilitySubset = DeviceExtFeatures.HasPortabilitySubset;
                EnabledExtFeats.PortabilitySubset    = DeviceExtFeatures.PortabilitySubset;
                VERIFY(PhysicalDevice->IsExtensionSupported(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME), "VK_KHR_portability_subset extension must be supported");
                DeviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);

                *NextExt = &EnabledExtFeats.PortabilitySubset;
                NextExt  = &EnabledExtFeats.PortabilitySubset.pNext;
            }
#endif

            if (EngineCI.Features.WaveOp != DEVICE_FEATURE_STATE_DISABLED)
            {
                EnabledExtFeats.SubgroupOps = true;
            }

            if (EngineCI.Features.InstanceDataStepRate != DEVICE_FEATURE_STATE_DISABLED)
            {
                VERIFY_EXPR(PhysicalDevice->IsExtensionSupported(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME));
                DeviceExtensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);

                EnabledExtFeats.VertexAttributeDivisor = DeviceExtFeatures.VertexAttributeDivisor;

                *NextExt = &EnabledExtFeats.VertexAttributeDivisor;
                NextExt  = &EnabledExtFeats.VertexAttributeDivisor.pNext;
            }

            // Append user-defined features
            *NextExt = EngineCI.pDeviceExtensionFeatures;
        }
        else
        {
            if (EngineCI.pDeviceExtensionFeatures == nullptr)
                LOG_ERROR_MESSAGE("Can not enable extended device features when VK_KHR_get_physical_device_properties2 extension is not supported by device");
        }

#if defined(_MSC_VER) && defined(_WIN64)
        static_assert(sizeof(Diligent::DeviceFeatures) == 36, "Did you add a new feature to DeviceFeatures? Please handle its satus here.");
#endif

        for (Uint32 i = 0; i < EngineCI.DeviceExtensionCount; ++i)
        {
            if (!PhysicalDevice->IsExtensionSupported(EngineCI.ppDeviceExtensionNames[i]))
            {
                LOG_ERROR_MESSAGE("Required device extension '", EngineCI.ppDeviceExtensionNames[i], "' is not supported.");
                continue;
            }

            // Remove duplicate extensions
            bool Exists = false;
            for (auto* ExtName : DeviceExtensions)
            {
                if (std::strcmp(ExtName, EngineCI.ppDeviceExtensionNames[i]) == 0)
                {
                    Exists = true;
                    break;
                }
            }

            if (!Exists)
                DeviceExtensions.push_back(EngineCI.ppDeviceExtensionNames[i]);
        }

        DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.empty() ? nullptr : DeviceExtensions.data();
        DeviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(DeviceExtensions.size());

        auto vkAllocator   = Instance->GetVkAllocator();
        auto LogicalDevice = VulkanUtilities::VulkanLogicalDevice::Create(*PhysicalDevice, DeviceCreateInfo, EnabledExtFeats, vkAllocator);

        auto& RawMemAllocator = GetRawAllocator();

        std::vector<RefCntAutoPtr<CommandQueueVkImpl>> CommandQueuesVk{std::max(1u, EngineCI.NumContexts)};
        std::vector<ICommandQueueVk*>                  CommandQueues{CommandQueuesVk.size()};

        if (EngineCI.NumContexts > 0)
        {
            for (Uint32 QInd = 0; QInd < QueueInfos.size(); ++QInd)
                QueueInfos[QInd].queueCount = 0;

            for (Uint32 CtxInd = 0; CtxInd < CommandQueuesVk.size(); ++CtxInd)
            {
                const auto& ContextInfo = EngineCI.pContextInfo[CtxInd];
                const auto  QueueIndex  = QueueIDtoQueueInfo[ContextInfo.QueueId];
                VERIFY_EXPR(QueueIndex != DEFAULT_QUEUE_ID);
                auto& QueueCI = QueueInfos[QueueIndex];

                CommandQueuesVk[CtxInd] = NEW_RC_OBJ(RawMemAllocator, "CommandQueueVk instance", CommandQueueVkImpl)(LogicalDevice, CommandQueueIndex{CtxInd}, EngineCI.NumContexts, QueueCI.queueCount, ContextInfo);
                CommandQueues[CtxInd]   = CommandQueuesVk[CtxInd];
                QueueCI.queueCount += 1;
            }
        }
        else
        {
            VERIFY_EXPR(CommandQueuesVk.size() == 1);
            ContextCreateInfo DefaultContextInfo{};
            DefaultContextInfo.Name    = "Graphics context";
            DefaultContextInfo.QueueId = static_cast<Uint8>(QueueInfos[0].queueFamilyIndex);

            CommandQueuesVk[0] = NEW_RC_OBJ(RawMemAllocator, "CommandQueueVk instance", CommandQueueVkImpl)(LogicalDevice, CommandQueueIndex{0}, 1, 1, DefaultContextInfo);
            CommandQueues[0]   = CommandQueuesVk[0];
        }

        OnRenderDeviceCreated = [&](RenderDeviceVkImpl* pRenderDeviceVk) //
        {
            FenceDesc Desc;
            Desc.Name = "Command queue internal fence";
            // Render device owns command queue that in turn owns the fence, so it is an internal device object
            constexpr bool IsDeviceInternal = true;

            for (Uint32 CtxInd = 0; CtxInd < CommandQueuesVk.size(); ++CtxInd)
            {
                RefCntAutoPtr<FenceVkImpl> pFenceVk{NEW_RC_OBJ(RawMemAllocator, "FenceVkImpl instance", FenceVkImpl)(pRenderDeviceVk, Desc, IsDeviceInternal)};
                CommandQueuesVk[CtxInd]->SetFence(std::move(pFenceVk));
            }
        };

        AttachToVulkanDevice(Instance, std::move(PhysicalDevice), LogicalDevice, CommandQueues.size(), CommandQueues.data(), EngineCI, AdapterInfo, ppDevice, ppContexts);

        m_RenderDeviceCreated = true;
    }
    catch (std::runtime_error&)
    {
        return;
    }
}

/// Attaches to existing Vulkan device

/// \param [in]  Instance       - shared pointer to a VulkanUtilities::VulkanInstance object
/// \param [in]  PhysicalDevice - pointer to the object representing physical device
/// \param [in]  LogicalDevice  - shared pointer to a VulkanUtilities::VulkanLogicalDevice object
/// \param [in]  pCommandQueue  - pointer to the implementation of command queue
/// \param [in]  EngineCI       - Engine creation attributes.
/// \param [out] ppDevice       - Address of the memory location where pointer to
///                               the created device will be written
/// \param [out] ppContexts     - Address of the memory location where pointers to
///                               the contexts will be written. Immediate context goes at
///                               position 0. If EngineCI.NumDeferredContexts > 0,
///                               pointers to the deferred contexts are written afterwards.
void EngineFactoryVkImpl::AttachToVulkanDevice(std::shared_ptr<VulkanUtilities::VulkanInstance>       Instance,
                                               std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                               std::shared_ptr<VulkanUtilities::VulkanLogicalDevice>  LogicalDevice,
                                               size_t                                                 CommandQueueCount,
                                               ICommandQueueVk**                                      ppCommandQueues,
                                               const EngineVkCreateInfo&                              EngineCI,
                                               const GraphicsAdapterInfo&                             AdapterInfo,
                                               IRenderDevice**                                        ppDevice,
                                               IDeviceContext**                                       ppContexts)
{
    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    if (EngineCI.EngineAPIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", EngineCI.EngineAPIVersion, ")");
        return;
    }

    VERIFY(ppCommandQueues && ppDevice && ppContexts, "Null pointer provided");
    if (!LogicalDevice || !ppCommandQueues || !ppDevice || !ppContexts)
        return;

    const Uint32 NumImmediateContexts = std::max(1u, EngineCI.NumContexts);

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (NumImmediateContexts + EngineCI.NumDeferredContexts));

    try
    {
        auto& RawMemAllocator = GetRawAllocator();

        VERIFY_EXPR(memcmp(&AdapterInfo.Capabilities.Features, &EngineCI.Features, sizeof(EngineCI.Features)) == 0);

        RenderDeviceVkImpl* pRenderDeviceVk(NEW_RC_OBJ(RawMemAllocator, "RenderDeviceVkImpl instance", RenderDeviceVkImpl)(RawMemAllocator, this, EngineCI, AdapterInfo, CommandQueueCount, ppCommandQueues, Instance, std::move(PhysicalDevice), LogicalDevice));
        pRenderDeviceVk->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice));

        if (OnRenderDeviceCreated != nullptr)
            OnRenderDeviceCreated(pRenderDeviceVk);

        std::shared_ptr<GenerateMipsVkHelper> GenerateMipsHelper(new GenerateMipsVkHelper(*pRenderDeviceVk));

        for (Uint32 CtxInd = 0; CtxInd < NumImmediateContexts; ++CtxInd)
        {
            RefCntAutoPtr<DeviceContextVkImpl> pImmediateCtxVk{NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, false, EngineCI, ContextIndex{CtxInd}, CommandQueueIndex{CtxInd}, GenerateMipsHelper)};
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
            // keep a weak reference to the context
            pImmediateCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + CtxInd));
            pRenderDeviceVk->SetImmediateContext(CtxInd, pImmediateCtxVk);
        }

        for (Uint32 DeferredCtx = 0; DeferredCtx < EngineCI.NumDeferredContexts; ++DeferredCtx)
        {
            RefCntAutoPtr<DeviceContextVkImpl> pDeferredCtxVk{NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, true, EngineCI, ContextIndex{NumImmediateContexts + DeferredCtx}, CommandQueueIndex{MAX_COMMAND_QUEUES}, GenerateMipsHelper)};
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
            // keep a weak reference to the context
            pDeferredCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + NumImmediateContexts + DeferredCtx));
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
        for (Uint32 ctx = 0; ctx < NumImmediateContexts + EngineCI.NumDeferredContexts; ++ctx)
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

} // namespace

API_QUALIFIER
IEngineFactoryVk* GetEngineFactoryVk()
{
    return EngineFactoryVkImpl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER
    Diligent::IEngineFactoryVk* Diligent_GetEngineFactoryVk()
    {
        return Diligent::GetEngineFactoryVk();
    }
}
