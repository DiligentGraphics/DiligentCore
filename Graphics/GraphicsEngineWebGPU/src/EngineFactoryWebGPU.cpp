/*
 *  Copyright 2023-2025 Diligent Graphics LLC
 *
 *  You may not use this file except in compliance with the License (see License.txt).
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
/// Routines that initialize WebGPU-based engine implementation

#include "pch.h"

#include "WebGPUObjectWrappers.hpp"

#include "EngineFactoryBase.hpp"
#include "EngineFactoryWebGPU.h"

#include "DeviceContextWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "SwapChainWebGPUImpl.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "TextureWebGPUImpl.hpp"
#include "PipelineStateWebGPUImpl.hpp"
#include "PipelineResourceSignatureWebGPUImpl.hpp"
#include "PipelineResourceAttribsWebGPU.hpp"
#include "ShaderResourceCacheWebGPU.hpp"
#include "FenceWebGPUImpl.hpp"
#include "DearchiverWebGPUImpl.hpp"
#include "WebGPUStubs.hpp"

#include "StringTools.hpp"
#include "GraphicsAccessories.hpp"

#if !PLATFORM_WEB
#    include "dawn/native/DawnNative.h"
#    include "dawn/dawn_proc.h"
#endif

#if PLATFORM_WEB
#    include <emscripten.h>
#endif


namespace Diligent
{

/// Engine factory for WebGPU implementation
class EngineFactoryWebGPUImpl final : public EngineFactoryBase<IEngineFactoryWebGPU>
{
public:
    static EngineFactoryWebGPUImpl* GetInstance()
    {
        static EngineFactoryWebGPUImpl TheFactory;
        return &TheFactory;
    }

    using TBase = EngineFactoryBase;

    EngineFactoryWebGPUImpl() :
        TBase{IID_EngineFactoryWebGPU}
    {}

    void DILIGENT_CALL_TYPE EnumerateAdapters(Version              MinVersion,
                                              Uint32&              NumAdapters,
                                              GraphicsAdapterInfo* Adapters) const override final;

    void DILIGENT_CALL_TYPE CreateDearchiver(const DearchiverCreateInfo& CreateInfo,
                                             IDearchiver**               ppDearchiver) const override final;

    void DILIGENT_CALL_TYPE CreateDeviceAndContextsWebGPU(const EngineWebGPUCreateInfo& EngineCI,
                                                          IRenderDevice**               ppDevice,
                                                          IDeviceContext**              ppContexts) override final;

    void DILIGENT_CALL_TYPE CreateSwapChainWebGPU(IRenderDevice*       pDevice,
                                                  IDeviceContext*      pImmediateContext,
                                                  const SwapChainDesc& SCDesc,
                                                  const NativeWindow&  Window,
                                                  ISwapChain**         ppSwapChain) override final;

    void DILIGENT_CALL_TYPE AttachToWebGPUDevice(void*                         wgpuInstance,
                                                 void*                         wgpuAdapter,
                                                 void*                         wgpuDevice,
                                                 const EngineWebGPUCreateInfo& EngineCI,
                                                 IRenderDevice**               ppDevice,
                                                 IDeviceContext**              ppContexts) override final;

    const void* DILIGENT_CALL_TYPE GetProcessTable() const override final;
};

namespace
{

void InstancePoolEvents(WGPUInstance wgpuInstance)
{
#if !PLATFORM_WEB
    wgpuInstanceProcessEvents(wgpuInstance);
#endif
}

WebGPUInstanceWrapper InitializeWebGPUInstance(bool EnableUnsafe)
{
    // Not implemented in Emscripten https://github.com/emscripten-core/emscripten/blob/217010a223375e6e9251669187d406ef2ddf266e/system/lib/webgpu/webgpu.cpp#L24
#if PLATFORM_WEB
    WebGPUInstanceWrapper wgpuInstance{wgpuCreateInstance(nullptr)};
#else
    struct SetDawnProcsHelper
    {
        SetDawnProcsHelper()
        {
            dawnProcSetProcs(&dawn::native::GetProcs());
        }
    };
    static SetDawnProcsHelper SetDawnProcs;

    const char* ToggleNames[] = {
        "allow_unsafe_apis"};

    WGPUDawnTogglesDescriptor wgpuDawnTogglesDesc = {};
    wgpuDawnTogglesDesc.chain.sType               = WGPUSType_DawnTogglesDescriptor;
    wgpuDawnTogglesDesc.enabledToggleCount        = _countof(ToggleNames);
    wgpuDawnTogglesDesc.enabledToggles            = ToggleNames;

    WGPUInstanceDescriptor wgpuInstanceDesc = {};
    if (EnableUnsafe)
    {
        wgpuInstanceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuDawnTogglesDesc);
    }
    WebGPUInstanceWrapper wgpuInstance{wgpuCreateInstance(&wgpuInstanceDesc)};
#endif
    if (!wgpuInstance)
        LOG_ERROR_AND_THROW("Failed to create WebGPU instance");
    return wgpuInstance;
}

std::vector<WebGPUAdapterWrapper> FindCompatibleAdapters(WGPUInstance wgpuInstance, Version MinVersion)
{
    std::vector<WebGPUAdapterWrapper> wgpuAdapters;

    struct CallbackUserData
    {
        WGPUAdapter              wgpuAdapter       = nullptr;
        WGPURequestAdapterStatus wgpuRequestStatus = {};
        String                   Message           = {};
        bool                     IsReady           = {};
    };

    auto OnAdapterRequestEnded = [](WGPURequestAdapterStatus wgpuStatus, WGPUAdapter wgpuAdapter, WGPUStringView Message, void* pUserData1, void* pUserData2) {
        if (pUserData1 != nullptr)
        {
            CallbackUserData* pUserData  = static_cast<CallbackUserData*>(pUserData1);
            pUserData->wgpuAdapter       = wgpuAdapter;
            pUserData->wgpuRequestStatus = wgpuStatus;
            pUserData->IsReady           = true;
            if (WGPUStringViewValid(Message))
                pUserData->Message = WGPUStringViewToString(Message);
        }
    };

    WGPUPowerPreference PowerPreferences[] = {
        WGPUPowerPreference_HighPerformance,
        WGPUPowerPreference_LowPower};

    for (const WGPUPowerPreference& PowerPreference : PowerPreferences)
    {
        CallbackUserData UserData{};

        WGPURequestAdapterOptions wgpuAdapterRequestOptions{};
        wgpuAdapterRequestOptions.powerPreference      = PowerPreference;
        wgpuAdapterRequestOptions.backendType          = WGPUBackendType_Undefined;
        wgpuAdapterRequestOptions.forceFallbackAdapter = false;

        WGPURequestAdapterCallbackInfo wgpuAdapterRequestCallbackInfo{};
        wgpuAdapterRequestCallbackInfo.callback  = OnAdapterRequestEnded;
        wgpuAdapterRequestCallbackInfo.mode      = WGPUCallbackMode_AllowSpontaneous;
        wgpuAdapterRequestCallbackInfo.userdata1 = &UserData;
        wgpuInstanceRequestAdapter(wgpuInstance, &wgpuAdapterRequestOptions, wgpuAdapterRequestCallbackInfo);

        while (!UserData.IsReady)
            InstancePoolEvents(wgpuInstance);

        if (UserData.wgpuRequestStatus == WGPURequestAdapterStatus_Success)
        {
            auto AdapterIter = std::find_if(wgpuAdapters.begin(), wgpuAdapters.end(),
                                            [&](const auto& wgpuAdapter) { return wgpuAdapter.Get() == UserData.wgpuAdapter; });

            if (AdapterIter == wgpuAdapters.end())
                wgpuAdapters.emplace_back(UserData.wgpuAdapter);
        }
        else
        {
            LOG_WARNING_MESSAGE(UserData.Message);
        }
    }

    return wgpuAdapters;
}

static void DeviceLostCallback(WGPUDevice const*    wgpuDevice,
                               WGPUDeviceLostReason wgpuReason,
                               WGPUStringView       Message,
                               void*                userdata,
                               void*                userdata2)
{
    bool Expression = wgpuReason != WGPUDeviceLostReason_Destroyed;
    Expression &= (wgpuReason != WGPUDeviceLostReason_CallbackCancelled);

    if (Expression && WGPUStringViewValid(Message))
    {
        LOG_DEBUG_MESSAGE(DEBUG_MESSAGE_SEVERITY_ERROR, "WebGPU: ", WGPUStringViewToString(Message));
    }
}

WebGPUDeviceWrapper CreateDeviceForAdapter(const DeviceFeatures& Features, WGPUInstance wgpuInstance, WGPUAdapter wgpuAdapter)
{
    WGPULimits wgpuSupportedLimits{};
    wgpuAdapterGetLimits(wgpuAdapter, &wgpuSupportedLimits);

    std::vector<WGPUFeatureName> wgpuFeatures{};
    {
        auto AddWGPUFeature = [wgpuAdapter, &wgpuFeatures](bool Required, WGPUFeatureName wgpuFeature) {
            if (Required && wgpuAdapterHasFeature(wgpuAdapter, wgpuFeature))
                wgpuFeatures.push_back(wgpuFeature);
        };

        AddWGPUFeature(Features.DepthBiasClamp, WGPUFeatureName_DepthClipControl);
        AddWGPUFeature(Features.TimestampQueries || Features.DurationQueries, WGPUFeatureName_TimestampQuery);
        AddWGPUFeature(Features.DurationQueries, WGPUFeatureName_ChromiumExperimentalTimestampQueryInsidePasses);
        AddWGPUFeature(Features.TextureCompressionBC, WGPUFeatureName_TextureCompressionBC);
        AddWGPUFeature(Features.TextureCompressionETC2, WGPUFeatureName_TextureCompressionETC2);
        AddWGPUFeature(Features.ShaderFloat16, WGPUFeatureName_ShaderF16);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_Depth32FloatStencil8))
            wgpuFeatures.push_back(WGPUFeatureName_Depth32FloatStencil8);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_Float32Filterable))
            wgpuFeatures.push_back(WGPUFeatureName_Float32Filterable);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_IndirectFirstInstance))
            wgpuFeatures.push_back(WGPUFeatureName_IndirectFirstInstance);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_RG11B10UfloatRenderable))
            wgpuFeatures.push_back(WGPUFeatureName_RG11B10UfloatRenderable);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_BGRA8UnormStorage))
            wgpuFeatures.push_back(WGPUFeatureName_BGRA8UnormStorage);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_TextureFormatsTier1))
            wgpuFeatures.push_back(WGPUFeatureName_TextureFormatsTier1);

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_TextureFormatsTier2))
            wgpuFeatures.push_back(WGPUFeatureName_TextureFormatsTier2);
    }

    struct CallbackUserData
    {
        WGPUDevice              wgpuDevice        = nullptr;
        WGPURequestDeviceStatus wgpuRequestStatus = {};
        String                  Message           = {};
        bool                    IsReady           = {};
    } UserData;

    auto OnDeviceRequestEnded = [](WGPURequestDeviceStatus wgpuStatus, WGPUDevice wgpuDevice, WGPUStringView Message, void* pUserData1, void* pUserData2) {
        if (pUserData1 != nullptr)
        {
            CallbackUserData* pUserData  = static_cast<CallbackUserData*>(pUserData1);
            pUserData->wgpuDevice        = wgpuDevice;
            pUserData->wgpuRequestStatus = wgpuStatus;
            pUserData->IsReady           = true;
            if (WGPUStringViewValid(Message))
                pUserData->Message = WGPUStringViewToString(Message);
        }
    };

    auto UncapturedErrorCallback = [](WGPUDevice const* wgpuDevice, WGPUErrorType wgpuTypeError, WGPUStringView Message, void* pUserdata1, void* pUserdata2) {
        LOG_ERROR_MESSAGE("Uncaptured WebGPU error (type ", wgpuTypeError, "): ", WGPUStringViewToString(Message));
    };

    WGPUDeviceDescriptor wgpuDeviceDesc{};
    wgpuDeviceDesc.requiredLimits       = &wgpuSupportedLimits;
    wgpuDeviceDesc.requiredFeatureCount = wgpuFeatures.size();
    wgpuDeviceDesc.requiredFeatures     = wgpuFeatures.data();

    wgpuDeviceDesc.uncapturedErrorCallbackInfo.callback = UncapturedErrorCallback;
    wgpuDeviceDesc.deviceLostCallbackInfo.callback      = DeviceLostCallback;
    wgpuDeviceDesc.deviceLostCallbackInfo.mode          = WGPUCallbackMode_AllowSpontaneous;
#if !PLATFORM_WEB
    const char* ToggleNames[] = {
        "disable_timestamp_query_conversion",
        "use_dxc",
    };

    WGPUDawnTogglesDescriptor wgpuDawnTogglesDesc = {};
    wgpuDawnTogglesDesc.chain.sType               = WGPUSType_DawnTogglesDescriptor;
    wgpuDawnTogglesDesc.enabledToggleCount        = _countof(ToggleNames);
    wgpuDawnTogglesDesc.enabledToggles            = ToggleNames;

    wgpuDeviceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuDawnTogglesDesc);
#endif

    WGPURequestDeviceCallbackInfo wgpuDeviceRequestCallbackInfo{};
    wgpuDeviceRequestCallbackInfo.nextInChain = nullptr;
    wgpuDeviceRequestCallbackInfo.mode        = WGPUCallbackMode_AllowSpontaneous;
    wgpuDeviceRequestCallbackInfo.callback    = OnDeviceRequestEnded;
    wgpuDeviceRequestCallbackInfo.userdata1   = &UserData;
    wgpuAdapterRequestDevice(wgpuAdapter, &wgpuDeviceDesc, wgpuDeviceRequestCallbackInfo);

    while (!UserData.IsReady)
        InstancePoolEvents(wgpuInstance);

    if (UserData.wgpuRequestStatus != WGPURequestDeviceStatus_Success)
        LOG_ERROR_AND_THROW(UserData.Message);

    return WebGPUDeviceWrapper{UserData.wgpuDevice};
}

bool FeatureSupported(WGPUAdapter wgpuAdapter, WGPUDevice wgpuDevice, WGPUFeatureName Feature)
{
    if (wgpuAdapter != nullptr)
    {
        return wgpuAdapterHasFeature(wgpuAdapter, Feature);
    }
    else if (wgpuDevice != nullptr)
    {
        return wgpuDeviceHasFeature(wgpuDevice, Feature);
    }
    else
    {
        UNEXPECTED("Either adapter or device must not be null");
        return DEVICE_FEATURE_STATE_DISABLED;
    }
}

DeviceFeatures GetSupportedFeatures(WGPUAdapter wgpuAdapter, WGPUDevice wgpuDevice = nullptr)
{
    auto CheckFeature = [wgpuAdapter, wgpuDevice](WGPUFeatureName Feature) {
        return FeatureSupported(wgpuAdapter, wgpuDevice, Feature) ? DEVICE_FEATURE_STATE_ENABLED : DEVICE_FEATURE_STATE_DISABLED;
    };

    DeviceFeatures Features;
    Features.SeparablePrograms                 = DEVICE_FEATURE_STATE_ENABLED;
    Features.ShaderResourceQueries             = DEVICE_FEATURE_STATE_ENABLED;
    Features.WireframeFill                     = DEVICE_FEATURE_STATE_DISABLED;
    Features.MultithreadedResourceCreation     = DEVICE_FEATURE_STATE_DISABLED;
    Features.ComputeShaders                    = DEVICE_FEATURE_STATE_ENABLED;
    Features.GeometryShaders                   = DEVICE_FEATURE_STATE_DISABLED;
    Features.Tessellation                      = DEVICE_FEATURE_STATE_DISABLED;
    Features.MeshShaders                       = DEVICE_FEATURE_STATE_DISABLED;
    Features.RayTracing                        = DEVICE_FEATURE_STATE_DISABLED;
    Features.BindlessResources                 = DEVICE_FEATURE_STATE_DISABLED;
    Features.OcclusionQueries                  = DEVICE_FEATURE_STATE_ENABLED;
    Features.BinaryOcclusionQueries            = DEVICE_FEATURE_STATE_DISABLED;
    Features.PipelineStatisticsQueries         = DEVICE_FEATURE_STATE_DISABLED;
    Features.DepthBiasClamp                    = DEVICE_FEATURE_STATE_ENABLED;
    Features.DepthClamp                        = CheckFeature(WGPUFeatureName_DepthClipControl);
    Features.IndependentBlend                  = DEVICE_FEATURE_STATE_ENABLED;
    Features.DualSourceBlend                   = CheckFeature(WGPUFeatureName_DualSourceBlending);
    Features.MultiViewport                     = DEVICE_FEATURE_STATE_DISABLED;
    Features.TextureCompressionBC              = CheckFeature(WGPUFeatureName_TextureCompressionBC);
    Features.TextureCompressionETC2            = CheckFeature(WGPUFeatureName_TextureCompressionETC2);
    Features.VertexPipelineUAVWritesAndAtomics = DEVICE_FEATURE_STATE_ENABLED;
    Features.PixelUAVWritesAndAtomics          = DEVICE_FEATURE_STATE_ENABLED;
    Features.TextureUAVExtendedFormats         = DEVICE_FEATURE_STATE_ENABLED;
    Features.ShaderFloat16                     = CheckFeature(WGPUFeatureName_ShaderF16);
    Features.ResourceBuffer16BitAccess         = DEVICE_FEATURE_STATE_DISABLED;
    Features.UniformBuffer16BitAccess          = DEVICE_FEATURE_STATE_DISABLED;
    Features.ShaderInputOutput16               = DEVICE_FEATURE_STATE_DISABLED;
    Features.ShaderInt8                        = DEVICE_FEATURE_STATE_DISABLED;
    Features.ResourceBuffer8BitAccess          = DEVICE_FEATURE_STATE_DISABLED;
    Features.UniformBuffer8BitAccess           = DEVICE_FEATURE_STATE_DISABLED;
    Features.ShaderResourceStaticArrays        = DEVICE_FEATURE_STATE_DISABLED;
    Features.ShaderResourceRuntimeArrays       = DEVICE_FEATURE_STATE_DISABLED;
    Features.WaveOp                            = DEVICE_FEATURE_STATE_DISABLED;
    Features.InstanceDataStepRate              = DEVICE_FEATURE_STATE_DISABLED;
    Features.NativeFence                       = DEVICE_FEATURE_STATE_DISABLED;
    Features.TileShaders                       = DEVICE_FEATURE_STATE_DISABLED;
    Features.TransferQueueTimestampQueries     = DEVICE_FEATURE_STATE_DISABLED;
    Features.VariableRateShading               = DEVICE_FEATURE_STATE_DISABLED;
    Features.SparseResources                   = DEVICE_FEATURE_STATE_DISABLED;
    Features.SubpassFramebufferFetch           = DEVICE_FEATURE_STATE_DISABLED;
    Features.TextureComponentSwizzle           = DEVICE_FEATURE_STATE_DISABLED;
    Features.TextureSubresourceViews           = DEVICE_FEATURE_STATE_ENABLED;
    Features.NativeMultiDraw                   = DEVICE_FEATURE_STATE_DISABLED;
    Features.AsyncShaderCompilation            = DEVICE_FEATURE_STATE_ENABLED;
    Features.FormattedBuffers                  = DEVICE_FEATURE_STATE_DISABLED;

    Features.TimestampQueries = CheckFeature(WGPUFeatureName_TimestampQuery);
    Features.DurationQueries  = Features.TimestampQueries ?
        CheckFeature(WGPUFeatureName_ChromiumExperimentalTimestampQueryInsidePasses) :
        DEVICE_FEATURE_STATE_DISABLED;

    ASSERT_SIZEOF(DeviceFeatures, 47, "Did you add a new feature to DeviceFeatures? Please handle its status here.");

    return Features;
}


GraphicsAdapterInfo GetGraphicsAdapterInfo(WGPUAdapter wgpuAdapter, WGPUDevice wgpuDevice = nullptr)
{
    WGPUAdapterInfo wgpuAdapterInfo{};
    if (wgpuAdapter)
        wgpuAdapterGetInfo(wgpuAdapter, &wgpuAdapterInfo);

    GraphicsAdapterInfo AdapterInfo{};

    // Set graphics adapter properties
    {
        auto ConvertWPUAdapterType = [](WGPUAdapterType Type) -> ADAPTER_TYPE {
            switch (Type)
            {
                case WGPUAdapterType_CPU:
                    return ADAPTER_TYPE_SOFTWARE;
                case WGPUAdapterType_DiscreteGPU:
                    return ADAPTER_TYPE_DISCRETE;
                case WGPUAdapterType_IntegratedGPU:
                    return ADAPTER_TYPE_INTEGRATED;
                default:
                    return ADAPTER_TYPE_UNKNOWN;
            }
        };

        if (WGPUStringViewValid(wgpuAdapterInfo.vendor))
        {
            const std::string Description     = WGPUStringViewToString(wgpuAdapterInfo.vendor);
            const size_t      DescriptionSize = std::min(_countof(AdapterInfo.Description) - 1, Description.length());
            memcpy(AdapterInfo.Description, Description.c_str(), DescriptionSize);
        }
        AdapterInfo.Type       = ConvertWPUAdapterType(wgpuAdapterInfo.adapterType);
        AdapterInfo.Vendor     = VendorIdToAdapterVendor(wgpuAdapterInfo.vendorID);
        AdapterInfo.VendorId   = wgpuAdapterInfo.vendorID;
        AdapterInfo.DeviceId   = wgpuAdapterInfo.deviceID;
        AdapterInfo.NumOutputs = 0;
    }

    AdapterInfo.Features = GetSupportedFeatures(wgpuAdapter, wgpuDevice);

    WGPULimits wgpuSupportedLimits{};
    if (wgpuAdapter)
        wgpuAdapterGetLimits(wgpuAdapter, &wgpuSupportedLimits);
    else
        wgpuDeviceGetLimits(wgpuDevice, &wgpuSupportedLimits);

    // Set adapter memory info
    {
        AdapterMemoryInfo& DrawCommandInfo{AdapterInfo.Memory};
        DrawCommandInfo.UnifiedMemoryCPUAccess = CPU_ACCESS_NONE;
        DrawCommandInfo.UnifiedMemory          = 0;
    }

    // Draw command properties
    {
        DrawCommandProperties& DrawCommandInfo{AdapterInfo.DrawCommand};
        DrawCommandInfo.MaxDrawIndirectCount = ~0u;
        DrawCommandInfo.CapFlags             = DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT;

        if (FeatureSupported(wgpuAdapter, wgpuDevice, WGPUFeatureName_IndirectFirstInstance))
            DrawCommandInfo.CapFlags |= DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT_FIRST_INSTANCE;
    }

    // Set queue info
    {
        AdapterInfo.NumQueues                           = 1;
        AdapterInfo.Queues[0].QueueType                 = COMMAND_QUEUE_TYPE_GRAPHICS;
        AdapterInfo.Queues[0].MaxDeviceContexts         = 1;
        AdapterInfo.Queues[0].TextureCopyGranularity[0] = 1;
        AdapterInfo.Queues[0].TextureCopyGranularity[1] = 1;
        AdapterInfo.Queues[0].TextureCopyGranularity[2] = 1;
    }

    // Set compute shader info
    {
        ComputeShaderProperties& ComputeShaderInfo{AdapterInfo.ComputeShader};

        ComputeShaderInfo.MaxThreadGroupSizeX = wgpuSupportedLimits.maxComputeWorkgroupSizeX;
        ComputeShaderInfo.MaxThreadGroupSizeY = wgpuSupportedLimits.maxComputeWorkgroupSizeY;
        ComputeShaderInfo.MaxThreadGroupSizeZ = wgpuSupportedLimits.maxComputeWorkgroupSizeZ;

        ComputeShaderInfo.MaxThreadGroupCountX = wgpuSupportedLimits.maxComputeWorkgroupsPerDimension;
        ComputeShaderInfo.MaxThreadGroupCountY = wgpuSupportedLimits.maxComputeWorkgroupsPerDimension;
        ComputeShaderInfo.MaxThreadGroupCountZ = wgpuSupportedLimits.maxComputeWorkgroupsPerDimension;

        ComputeShaderInfo.SharedMemorySize          = wgpuSupportedLimits.maxComputeWorkgroupStorageSize;
        ComputeShaderInfo.MaxThreadGroupInvocations = wgpuSupportedLimits.maxComputeInvocationsPerWorkgroup;
    }

    // Set texture info
    {
        TextureProperties& TextureInfo{AdapterInfo.Texture};

        TextureInfo.MaxTexture1DArraySlices = 0; // Not supported in WebGPU
        TextureInfo.MaxTexture2DArraySlices = wgpuSupportedLimits.maxTextureArrayLayers;

        TextureInfo.MaxTexture1DDimension = wgpuSupportedLimits.maxTextureDimension1D;
        TextureInfo.MaxTexture2DDimension = wgpuSupportedLimits.maxTextureDimension2D;
        TextureInfo.MaxTexture3DDimension = wgpuSupportedLimits.maxTextureDimension3D;

        TextureInfo.Texture2DMSSupported       = True;
        TextureInfo.Texture2DMSArraySupported  = False;
        TextureInfo.TextureViewSupported       = True;
        TextureInfo.CubemapArraysSupported     = True;
        TextureInfo.TextureView2DOn3DSupported = True;
    }

    // Set buffer info
    {
        BufferProperties& BufferInfo{AdapterInfo.Buffer};
        BufferInfo.ConstantBufferOffsetAlignment   = wgpuSupportedLimits.minUniformBufferOffsetAlignment;
        BufferInfo.StructuredBufferOffsetAlignment = wgpuSupportedLimits.minStorageBufferOffsetAlignment;
    }

    // Set sampler info
    {
        SamplerProperties& BufferInfo{AdapterInfo.Sampler};
        BufferInfo.MaxAnisotropy = 16;
    }

    wgpuAdapterInfoFreeMembers(wgpuAdapterInfo);
    return AdapterInfo;
}

} // namespace

void EngineFactoryWebGPUImpl::EnumerateAdapters(Version              MinVersion,
                                                Uint32&              NumAdapters,
                                                GraphicsAdapterInfo* Adapters) const
{
    WebGPUInstanceWrapper             wgpuInstance = InitializeWebGPUInstance(true);
    std::vector<WebGPUAdapterWrapper> wgpuAdapters = FindCompatibleAdapters(wgpuInstance.Get(), MinVersion);

    if (Adapters == nullptr)
        NumAdapters = static_cast<Uint32>(wgpuAdapters.size());
    else
    {
        NumAdapters = (std::min)(NumAdapters, static_cast<Uint32>(wgpuAdapters.size()));
        for (Uint32 AdapterId = 0; AdapterId < NumAdapters; ++AdapterId)
        {
            WebGPUAdapterWrapper& wgpuAdapter{wgpuAdapters[AdapterId]};
            Adapters[AdapterId] = GetGraphicsAdapterInfo(wgpuAdapter.Get());
        }
    }
}

void EngineFactoryWebGPUImpl::CreateDearchiver(const DearchiverCreateInfo& CreateInfo,
                                               IDearchiver**               ppDearchiver) const
{
    TBase::CreateDearchiver<DearchiverWebGPUImpl>(CreateInfo, ppDearchiver);
}

void EngineFactoryWebGPUImpl::CreateDeviceAndContextsWebGPU(const EngineWebGPUCreateInfo& EngineCI,
                                                            IRenderDevice**               ppDevice,
                                                            IDeviceContext**              ppImmediateContext)
{
    DEV_CHECK_ERR(ppDevice && ppImmediateContext, "Null pointer provided");
    if (!ppDevice || !ppImmediateContext)
        return;

    *ppDevice           = nullptr;
    *ppImmediateContext = nullptr;

    try
    {
        WebGPUInstanceWrapper             wgpuInstance = InitializeWebGPUInstance(true);
        std::vector<WebGPUAdapterWrapper> wgpuAdapters = FindCompatibleAdapters(wgpuInstance.Get(), EngineCI.GraphicsAPIVersion);

        WebGPUAdapterWrapper SpecificAdapter{};
        if (EngineCI.AdapterId != DEFAULT_ADAPTER_ID)
        {
            if (EngineCI.AdapterId < wgpuAdapters.size())
                SpecificAdapter = std::move(wgpuAdapters[EngineCI.AdapterId]);
            else
                LOG_ERROR_AND_THROW(EngineCI.AdapterId, " is not a valid hardware adapter id. Total number of compatible adapters available on this system: ", wgpuAdapters.size());
        }
        else
        {
            SpecificAdapter = std::move(wgpuAdapters[0]);
        }

        WebGPUDeviceWrapper Device = CreateDeviceForAdapter(EngineCI.Features, wgpuInstance, SpecificAdapter);
        AttachToWebGPUDevice(wgpuInstance.Detach(), SpecificAdapter.Detach(), Device.Detach(), EngineCI, ppDevice, ppImmediateContext);
    }
    catch (const std::runtime_error&)
    {
    }
}

void EngineFactoryWebGPUImpl::CreateSwapChainWebGPU(IRenderDevice*       pDevice,
                                                    IDeviceContext*      pImmediateContext,
                                                    const SwapChainDesc& SCDesc,
                                                    const NativeWindow&  Window,
                                                    ISwapChain**         ppSwapChain)
{
    DEV_CHECK_ERR(ppSwapChain, "Null pointer provided");
    if (!ppSwapChain)
        return;

    *ppSwapChain = nullptr;

    try
    {
        RenderDeviceWebGPUImpl*  pDeviceWebGPU        = ClassPtrCast<RenderDeviceWebGPUImpl>(pDevice);
        DeviceContextWebGPUImpl* pDeviceContextWebGPU = ClassPtrCast<DeviceContextWebGPUImpl>(pImmediateContext);
        IMemoryAllocator&        RawMemAllocator      = GetRawAllocator();

        SwapChainWebGPUImpl* pSwapChainWebGPU = NEW_RC_OBJ(RawMemAllocator, "SwapChainWebGPUImpl instance", SwapChainWebGPUImpl)(SCDesc, pDeviceWebGPU, pDeviceContextWebGPU, Window);
        pSwapChainWebGPU->QueryInterface(IID_SwapChain, ppSwapChain);
    }
    catch (const std::runtime_error&)
    {
        if (*ppSwapChain)
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR("Failed to create WebGPU-based swapchain");
    }
}

void EngineFactoryWebGPUImpl::AttachToWebGPUDevice(void*                         wgpuInstance,
                                                   void*                         wgpuAdapter,
                                                   void*                         wgpuDevice,
                                                   const EngineWebGPUCreateInfo& EngineCI,
                                                   IRenderDevice**               ppDevice,
                                                   IDeviceContext**              ppImmediateContext)
{
    if (EngineCI.EngineAPIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", EngineCI.EngineAPIVersion, ")");
        return;
    }

    VERIFY(ppDevice && ppImmediateContext, "Null pointer provided");
    if (!ppDevice || !ppImmediateContext)
        return;

    if (EngineCI.NumImmediateContexts > 1)
    {
        LOG_ERROR_MESSAGE("WebGPU backend doesn't support multiple immediate contexts");
        return;
    }

    if (EngineCI.NumDeferredContexts > 0)
    {
        LOG_ERROR_MESSAGE("WebGPU backend doesn't support multiple deferred contexts");
        return;
    }

    *ppDevice           = nullptr;
    *ppImmediateContext = nullptr;

    try
    {
        const GraphicsAdapterInfo AdapterInfo = GetGraphicsAdapterInfo(static_cast<WGPUAdapter>(wgpuAdapter), static_cast<WGPUDevice>(wgpuDevice));
        VerifyEngineCreateInfo(EngineCI, AdapterInfo);

        const DeviceFeatures EnabledFeatures = GetSupportedFeatures(nullptr, static_cast<WGPUDevice>(wgpuDevice));

        IMemoryAllocator& RawMemAllocator = GetRawAllocator();

        RenderDeviceWebGPUImpl* pRenderDeviceWebGPU{
            NEW_RC_OBJ(RawMemAllocator, "RenderDeviceWebGPUImpl instance", RenderDeviceWebGPUImpl)(
                RenderDeviceWebGPUImpl::CreateInfo{
                    RawMemAllocator,
                    this,
                    EngineCI,
                    AdapterInfo,
                    EnabledFeatures,
                    static_cast<WGPUInstance>(wgpuInstance),
                    static_cast<WGPUAdapter>(wgpuAdapter),
                    static_cast<WGPUDevice>(wgpuDevice),
                })};
        pRenderDeviceWebGPU->QueryInterface(IID_RenderDevice, ppDevice);

        DeviceContextWebGPUImpl* pDeviceContextWebGPU{
            NEW_RC_OBJ(RawMemAllocator, "DeviceContextWebGPUImpl instance", DeviceContextWebGPUImpl)(
                pRenderDeviceWebGPU,
                DeviceContextDesc{
                    EngineCI.pImmediateContextInfo ? EngineCI.pImmediateContextInfo[0].Name : nullptr,
                    pRenderDeviceWebGPU->GetAdapterInfo().Queues[0].QueueType,
                    False, // IsDeferred
                    0,     // Context id
                    0      // Queue id
                })};
        pDeviceContextWebGPU->QueryInterface(IID_DeviceContext, ppImmediateContext);
        pRenderDeviceWebGPU->SetImmediateContext(0, pDeviceContextWebGPU);
    }
    catch (const std::runtime_error&)
    {
        if (*ppDevice)
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }

        if (*ppImmediateContext != nullptr)
        {
            (*ppImmediateContext)->Release();
            *ppImmediateContext = nullptr;
        }

        LOG_ERROR("Failed to create WebGPU-based render device and context");
    }
}

const void* EngineFactoryWebGPUImpl::GetProcessTable() const
{
#if !PLATFORM_WEB
    return &dawn::native::GetProcs();
#else
    return nullptr;
#endif
}

API_QUALIFIER IEngineFactoryWebGPU* GetEngineFactoryWebGPU()
{
    return EngineFactoryWebGPUImpl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER Diligent::IEngineFactoryWebGPU* Diligent_GetEngineFactoryWebGPU()
    {
        return Diligent::GetEngineFactoryWebGPU();
    }
}
