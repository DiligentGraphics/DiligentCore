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
        Uint32 Version = VK_API_VERSION_1_0;
        if (EngineCI.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED)
            Version = VK_API_VERSION_1_2;

        auto Instance = VulkanUtilities::VulkanInstance::Create(
            Version,
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

        auto GetFeatureState = [](DEVICE_FEATURE_STATE RequestedState, bool IsFeatureSupported, const char* FeatureName) //
        {
            switch (RequestedState)
            {
                case DEVICE_FEATURE_STATE_DISABLED:
                    return DEVICE_FEATURE_STATE_DISABLED;

                case DEVICE_FEATURE_STATE_ENABLED:
                {
                    if (IsFeatureSupported)
                        return DEVICE_FEATURE_STATE_ENABLED;
                    else
                        LOG_ERROR_AND_THROW(FeatureName, " not supported by this device");
                }

                case DEVICE_FEATURE_STATE_OPTIONAL:
                    return IsFeatureSupported ? DEVICE_FEATURE_STATE_ENABLED : DEVICE_FEATURE_STATE_DISABLED;

                default:
                    UNEXPECTED("Unexpected feature state");
                    return DEVICE_FEATURE_STATE_DISABLED;
            }
        };

#define ENABLE_FEATURE(vkFeature, State, FeatureName)                                          \
    do                                                                                         \
    {                                                                                          \
        State =                                                                                \
            GetFeatureState(State, PhysicalDeviceFeatures.vkFeature != VK_FALSE, FeatureName); \
        EnabledFeatures.vkFeature =                                                            \
            State == DEVICE_FEATURE_STATE_ENABLED ? VK_TRUE : VK_FALSE;                        \
    } while (false)

        auto ImageCubeArrayFeature    = DEVICE_FEATURE_STATE_OPTIONAL;
        auto SamplerAnisotropyFeature = DEVICE_FEATURE_STATE_OPTIONAL;
        // clang-format off
        ENABLE_FEATURE(geometryShader,                    EngineCI.Features.GeometryShaders,                   "Geometry shaders are");
        ENABLE_FEATURE(tessellationShader,                EngineCI.Features.Tessellation,                      "Tessellation is");
        ENABLE_FEATURE(pipelineStatisticsQuery,           EngineCI.Features.PipelineStatisticsQueries,         "Pipeline statistics queries are");
        ENABLE_FEATURE(occlusionQueryPrecise,             EngineCI.Features.OcclusionQueries,                  "Occlusion queries are");
        ENABLE_FEATURE(imageCubeArray,                    ImageCubeArrayFeature,                               "Image cube arrays are");
        ENABLE_FEATURE(fillModeNonSolid,                  EngineCI.Features.WireframeFill,                     "Wireframe fill is");
        ENABLE_FEATURE(samplerAnisotropy,                 SamplerAnisotropyFeature,                            "Anisotropic texture filtering is");
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

        const VulkanUtilities::VulkanPhysicalDevice::ExtensionFeatures& DeviceExtFeatures = PhysicalDevice->GetExtFeatures();
        VulkanUtilities::VulkanPhysicalDevice::ExtensionFeatures        EnabledExtFeats   = {};

        // SPIRV 1.5 is in Vulkan 1.2 core
        EnabledExtFeats.Spirv15 = DeviceExtFeatures.Spirv15;

#define ENABLE_FEATURE(IsFeatureSupported, Feature, FeatureName)                         \
    do                                                                                   \
    {                                                                                    \
        EngineCI.Features.Feature =                                                      \
            GetFeatureState(EngineCI.Features.Feature, IsFeatureSupported, FeatureName); \
    } while (false)

        const auto& MeshShaderFeats = DeviceExtFeatures.MeshShader;
        ENABLE_FEATURE(MeshShaderFeats.taskShader != VK_FALSE && MeshShaderFeats.meshShader != VK_FALSE, MeshShaders, "Mesh shaders are");

        const auto& ShaderFloat16Int8Feats = DeviceExtFeatures.ShaderFloat16Int8;
        // clang-format off
        ENABLE_FEATURE(ShaderFloat16Int8Feats.shaderFloat16 != VK_FALSE, ShaderFloat16, "16-bit float shader operations are");
        ENABLE_FEATURE(ShaderFloat16Int8Feats.shaderInt8    != VK_FALSE, ShaderInt8,    "8-bit int shader operations are");
        // clang-format on

        const auto& Storage16BitFeats = DeviceExtFeatures.Storage16Bit;
        // clang-format off
        ENABLE_FEATURE(Storage16BitFeats.storageBuffer16BitAccess           != VK_FALSE, ResourceBuffer16BitAccess, "16-bit resoure buffer access is");
        ENABLE_FEATURE(Storage16BitFeats.uniformAndStorageBuffer16BitAccess != VK_FALSE, UniformBuffer16BitAccess,  "16-bit uniform buffer access is");
        ENABLE_FEATURE(Storage16BitFeats.storageInputOutput16               != VK_FALSE, ShaderInputOutput16,       "16-bit shader inputs/outputs are");
        // clang-format on

        const auto& Storage8BitFeats = DeviceExtFeatures.Storage8Bit;
        // clang-format off
        ENABLE_FEATURE(Storage8BitFeats.storageBuffer8BitAccess           != VK_FALSE, ResourceBuffer8BitAccess, "8-bit resoure buffer access is");
        ENABLE_FEATURE(Storage8BitFeats.uniformAndStorageBuffer8BitAccess != VK_FALSE, UniformBuffer8BitAccess,  "8-bit uniform buffer access is");
        // clang-format on

        ENABLE_FEATURE(DeviceExtFeatures.AccelStruct.accelerationStructure != VK_FALSE && DeviceExtFeatures.RayTracingPipeline.rayTracingPipeline != VK_FALSE, RayTracing, "Ray tracing is");
#undef FeatureSupport


        // To enable some device extensions you must enable instance extension VK_KHR_get_physical_device_properties2
        // and add feature description to DeviceCreateInfo.pNext.
        bool SupportsFeatures2 = Instance->IsExtensionEnabled(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        // Enable extensions
        if (SupportsFeatures2)
        {
            void** NextExt = const_cast<void**>(&DeviceCreateInfo.pNext);

            // Mesh shader
            if (EngineCI.Features.MeshShaders != DEVICE_FEATURE_STATE_DISABLED)
            {
                EnabledExtFeats.MeshShader = MeshShaderFeats;
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
                EnabledExtFeats.ShaderFloat16Int8 = ShaderFloat16Int8Feats;
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
                EnabledExtFeats.Storage16Bit = Storage16BitFeats;
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
                EnabledExtFeats.Storage8Bit = Storage8BitFeats;
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
                VERIFY_EXPR(PhysicalDevice->IsExtensionSupported(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME));
                DeviceExtensions.push_back(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);
            }


            // Ray tracing
            if (EngineCI.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED)
            {
                // this extensions added to Vulkan 1.2 core
                if (!DeviceExtFeatures.Spirv15)
                {
                    DeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME); // required for VK_KHR_spirv_1_4
                    DeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);             // required for VK_KHR_ray_tracing_pipeline
                    EnabledExtFeats.Spirv14 = DeviceExtFeatures.Spirv14;
                    VERIFY_EXPR(DeviceExtFeatures.Spirv14);
                }

                DeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);             // required for VK_EXT_descriptor_indexing
                DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);      // required for VK_KHR_acceleration_structure
                DeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);    // required for VK_KHR_acceleration_structure
                DeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // required for VK_KHR_acceleration_structure
                DeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);   // required for ray tracing
                DeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);     // required for ray tracing

                EnabledExtFeats.AccelStruct         = DeviceExtFeatures.AccelStruct;
                EnabledExtFeats.RayTracingPipeline  = DeviceExtFeatures.RayTracingPipeline;
                EnabledExtFeats.BufferDeviceAddress = DeviceExtFeatures.BufferDeviceAddress;
                EnabledExtFeats.DescriptorIndexing  = DeviceExtFeatures.DescriptorIndexing;

                // disable unused features
                EnabledExtFeats.AccelStruct.accelerationStructureCaptureReplay                    = false;
                EnabledExtFeats.AccelStruct.accelerationStructureIndirectBuild                    = false;
                EnabledExtFeats.AccelStruct.accelerationStructureHostCommands                     = false;
                EnabledExtFeats.AccelStruct.descriptorBindingAccelerationStructureUpdateAfterBind = false;

                EnabledExtFeats.RayTracingPipeline.rayTracingPipelineShaderGroupHandleCaptureReplay      = false;
                EnabledExtFeats.RayTracingPipeline.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = false;
                EnabledExtFeats.RayTracingPipeline.rayTracingPipelineTraceRaysIndirect                   = false;
                EnabledExtFeats.RayTracingPipeline.rayTraversalPrimitiveCulling                          = false; // for GLSL_EXT_ray_flags_primitive_culling

                *NextExt = &EnabledExtFeats.AccelStruct;
                NextExt  = &EnabledExtFeats.AccelStruct.pNext;
                *NextExt = &EnabledExtFeats.RayTracingPipeline;
                NextExt  = &EnabledExtFeats.RayTracingPipeline.pNext;
                *NextExt = &EnabledExtFeats.DescriptorIndexing;
                NextExt  = &EnabledExtFeats.DescriptorIndexing.pNext;
                *NextExt = &EnabledExtFeats.BufferDeviceAddress;
                NextExt  = &EnabledExtFeats.BufferDeviceAddress.pNext;
            }

            // make sure that last pNext is null
            *NextExt = nullptr;
        }

#if defined(_MSC_VER) && defined(_WIN64)
        static_assert(sizeof(DeviceFeatures) == 32, "Did you add a new feature to DeviceFeatures? Please handle its satus here.");
#endif

        DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.empty() ? nullptr : DeviceExtensions.data();
        DeviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(DeviceExtensions.size());

        auto vkAllocator   = Instance->GetVkAllocator();
        auto LogicalDevice = VulkanUtilities::VulkanLogicalDevice::Create(*PhysicalDevice, DeviceCreateInfo, EnabledExtFeats, vkAllocator);

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

extern "C"
{
    API_QUALIFIER
    Diligent::IEngineFactoryVk* Diligent_GetEngineFactoryVk()
    {
        return Diligent::GetEngineFactoryVk();
    }
}
