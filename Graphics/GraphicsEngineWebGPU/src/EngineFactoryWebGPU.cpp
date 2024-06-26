/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "StringTools.hpp"
#include "GraphicsAccessories.hpp"

#if !PLATFORM_EMSCRIPTEN
#    include "dawn/native/DawnNative.h"
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

    void DILIGENT_CALL_TYPE AttachToWebGPUDevice(WGPUInstance                  wgpuInstance,
                                                 WGPUAdapter                   wgpuAdapter,
                                                 WGPUDevice                    wgpuDevice,
                                                 const EngineWebGPUCreateInfo& EngineCI,
                                                 IRenderDevice**               ppDevice,
                                                 IDeviceContext**              ppContexts);

    const void* DILIGENT_CALL_TYPE GetProcessTable() const override final;
};

namespace
{

std::vector<WebGPUAdapterWrapper> FindCompatibleAdapters(WGPUInstance wgpuInstance, Version MinVersion)
{
    std::vector<WebGPUAdapterWrapper> wgpuAdapters;

    struct CallbackUserData
    {
        WGPUAdapter              Adapter       = nullptr;
        WGPURequestAdapterStatus RequestStatus = {};
        String                   Message       = {};
    };

    auto OnAdapterRequestEnded = [](WGPURequestAdapterStatus Status, WGPUAdapter Adapter, char const* Message, void* pCallbackUserData) {
        auto* pUserData          = static_cast<CallbackUserData*>(pCallbackUserData);
        pUserData->Adapter       = Adapter;
        pUserData->RequestStatus = Status;
        if (Message != nullptr)
            pUserData->Message = Message;
    };

    WGPUPowerPreference PowerPreferences[] = {
        WGPUPowerPreference_HighPerformance,
        WGPUPowerPreference_LowPower};

    for (const auto& powerPreference : PowerPreferences)
    {
        CallbackUserData UserData{};
#if PLATFORM_EMSCRIPTEN
        WGPURequestAdapterOptions Options{nullptr, nullptr, powerPreference, WGPUBackendType_Undefined, false, false};
#else
        WGPURequestAdapterOptions Options{nullptr, nullptr, powerPreference, WGPUBackendType_Undefined, false};
#endif
        wgpuInstanceRequestAdapter(wgpuInstance, &Options, OnAdapterRequestEnded, &UserData);

        if (UserData.RequestStatus == WGPURequestAdapterStatus_Success)
        {
            auto IsFound = std::find_if(wgpuAdapters.begin(), wgpuAdapters.end(),
                                        [&](const auto& wgpuAdapter) { return wgpuAdapter.Get() == UserData.Adapter; });

            if (IsFound == wgpuAdapters.end())
                wgpuAdapters.emplace_back(UserData.Adapter);
        }
        else
        {
            LOG_WARNING_MESSAGE(UserData.Message);
        }
    }

    return wgpuAdapters;
}

WebGPUDeviceWrapper CreateDeviceForAdapter(EngineWebGPUCreateInfo const& EngineCI, WGPUAdapter Adapter)
{
    struct CallbackUserData
    {
        WGPUDevice              Device        = nullptr;
        WGPURequestDeviceStatus RequestStatus = {};
        String                  Message       = {};
    } UserData;

    auto OnDeviceRequestEnded = [](WGPURequestDeviceStatus Status, WGPUDevice Device, char const* Message, void* pCallbackUserData) {
        auto* pUserData          = static_cast<CallbackUserData*>(pCallbackUserData);
        pUserData->Device        = Device;
        pUserData->RequestStatus = Status;
        if (Message != nullptr)
            pUserData->Message = Message;
    };

    WGPUSupportedLimits SupportedLimits{};
    wgpuAdapterGetLimits(Adapter, &SupportedLimits);

    std::vector<WGPUFeatureName> Features{};
    {
        if (EngineCI.Features.DepthBiasClamp && wgpuAdapterHasFeature(Adapter, WGPUFeatureName_DepthClipControl))
            Features.push_back(WGPUFeatureName_DepthClipControl);

        if (EngineCI.Features.TimestampQueries && wgpuAdapterHasFeature(Adapter, WGPUFeatureName_TimestampQuery))
            Features.push_back(WGPUFeatureName_TimestampQuery);

        if (EngineCI.Features.TextureCompressionBC && wgpuAdapterHasFeature(Adapter, WGPUFeatureName_TextureCompressionBC))
            Features.push_back(WGPUFeatureName_TextureCompressionBC);

        if (EngineCI.Features.ShaderFloat16 && wgpuAdapterHasFeature(Adapter, WGPUFeatureName_ShaderF16))
            Features.push_back(WGPUFeatureName_ShaderF16);

        if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_Depth32FloatStencil8))
            Features.push_back(WGPUFeatureName_Depth32FloatStencil8);

        if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_Float32Filterable))
            Features.push_back(WGPUFeatureName_Float32Filterable);

        if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_IndirectFirstInstance))
            Features.push_back(WGPUFeatureName_IndirectFirstInstance);

        if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_RG11B10UfloatRenderable))
            Features.push_back(WGPUFeatureName_RG11B10UfloatRenderable);

        if (wgpuAdapterHasFeature(Adapter, WGPUFeatureName_BGRA8UnormStorage))
            Features.push_back(WGPUFeatureName_BGRA8UnormStorage);
    }

    auto DeviceLostCallback = [](WGPUDeviceLostReason Reason, char const* Message, void* pUserdata) {
        bool Expression = Reason != WGPUDeviceLostReason_Destroyed;
#if !PLATFORM_EMSCRIPTEN
        Expression &= Reason != WGPUDeviceLostReason_InstanceDropped;
#endif
        if (Expression && Message != nullptr)
            LOG_DEBUG_MESSAGE(DEBUG_MESSAGE_SEVERITY_ERROR, "WebGPU: ", Message);
    };

    WGPURequiredLimits   RequiredLimits{nullptr, SupportedLimits.limits};
    WGPUDeviceDescriptor DeviceDesc{};
    DeviceDesc.requiredLimits       = &RequiredLimits;
    DeviceDesc.requiredFeatureCount = Features.size();
    DeviceDesc.requiredFeatures     = Features.data();
    DeviceDesc.deviceLostCallback   = DeviceLostCallback;
    wgpuAdapterRequestDevice(Adapter, &DeviceDesc, OnDeviceRequestEnded, &UserData);

    if (UserData.RequestStatus != WGPURequestDeviceStatus_Success)
        LOG_ERROR_AND_THROW(UserData.Message);

    return WebGPUDeviceWrapper{UserData.Device};
}

GraphicsAdapterInfo GetGraphicsAdapterInfo(WGPUAdapter wgpuAdapter)
{
    WGPUAdapterProperties wgpuAdapterDesc{};
    wgpuAdapterGetProperties(wgpuAdapter, &wgpuAdapterDesc);

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

        const auto DescriptorSize = std::min(_countof(AdapterInfo.Description), wgpuAdapterDesc.name != nullptr ? strlen(wgpuAdapterDesc.name) : 0);
        memcpy(AdapterInfo.Description, wgpuAdapterDesc.name, DescriptorSize);
        AdapterInfo.Type       = ConvertWPUAdapterType(wgpuAdapterDesc.adapterType);
        AdapterInfo.Vendor     = VendorIdToAdapterVendor(wgpuAdapterDesc.vendorID);
        AdapterInfo.VendorId   = wgpuAdapterDesc.vendorID;
        AdapterInfo.DeviceId   = wgpuAdapterDesc.deviceID;
        AdapterInfo.NumOutputs = 0;
    }

    // Enable features
    {
        //TODO
        auto& Features{AdapterInfo.Features};
        Features.SeparablePrograms         = DEVICE_FEATURE_STATE_ENABLED;
        Features.ShaderResourceQueries     = DEVICE_FEATURE_STATE_ENABLED;
        Features.WireframeFill             = DEVICE_FEATURE_STATE_ENABLED;
        Features.ComputeShaders            = DEVICE_FEATURE_STATE_ENABLED;
        Features.OcclusionQueries          = DEVICE_FEATURE_STATE_ENABLED;
        Features.BinaryOcclusionQueries    = DEVICE_FEATURE_STATE_ENABLED;
        Features.DurationQueries           = DEVICE_FEATURE_STATE_ENABLED;
        Features.DepthBiasClamp            = DEVICE_FEATURE_STATE_ENABLED;
        Features.IndependentBlend          = DEVICE_FEATURE_STATE_ENABLED;
        Features.DualSourceBlend           = DEVICE_FEATURE_STATE_ENABLED;
        Features.MultiViewport             = DEVICE_FEATURE_STATE_ENABLED;
        Features.PixelUAVWritesAndAtomics  = DEVICE_FEATURE_STATE_ENABLED;
        Features.TextureUAVExtendedFormats = DEVICE_FEATURE_STATE_ENABLED;
        Features.DepthClamp                = DEVICE_FEATURE_STATE_ENABLED;

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_DepthClipControl))
            Features.DepthBiasClamp = DEVICE_FEATURE_STATE_ENABLED;

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_TimestampQuery))
            Features.TimestampQueries = DEVICE_FEATURE_STATE_ENABLED;

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_TextureCompressionBC))
            Features.TextureCompressionBC = DEVICE_FEATURE_STATE_ENABLED;

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_ShaderF16))
            Features.ShaderFloat16 = DEVICE_FEATURE_STATE_ENABLED;
    }

    WGPUSupportedLimits wgpuSupportedLimits{};
    wgpuAdapterGetLimits(wgpuAdapter, &wgpuSupportedLimits);

    // Set adapter memory info
    {
        auto& DrawCommandInfo                  = AdapterInfo.Memory;
        DrawCommandInfo.UnifiedMemoryCPUAccess = CPU_ACCESS_NONE;
        DrawCommandInfo.UnifiedMemory          = 0;
    }

    // Draw command properties
    {
        auto& DrawCommandInfo                = AdapterInfo.DrawCommand;
        DrawCommandInfo.MaxDrawIndirectCount = ~0u;
        DrawCommandInfo.CapFlags             = DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT;

        if (wgpuAdapterHasFeature(wgpuAdapter, WGPUFeatureName_IndirectFirstInstance))
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
        auto& ComputeShaderInfo = AdapterInfo.ComputeShader;

        ComputeShaderInfo.MaxThreadGroupSizeX = wgpuSupportedLimits.limits.maxComputeWorkgroupSizeX;
        ComputeShaderInfo.MaxThreadGroupSizeY = wgpuSupportedLimits.limits.maxComputeWorkgroupSizeY;
        ComputeShaderInfo.MaxThreadGroupSizeZ = wgpuSupportedLimits.limits.maxComputeWorkgroupSizeZ;

        ComputeShaderInfo.MaxThreadGroupCountX = wgpuSupportedLimits.limits.maxComputeWorkgroupsPerDimension;
        ComputeShaderInfo.MaxThreadGroupCountY = wgpuSupportedLimits.limits.maxComputeWorkgroupsPerDimension;
        ComputeShaderInfo.MaxThreadGroupCountZ = wgpuSupportedLimits.limits.maxComputeWorkgroupsPerDimension;

        ComputeShaderInfo.SharedMemorySize          = wgpuSupportedLimits.limits.maxComputeWorkgroupStorageSize;
        ComputeShaderInfo.MaxThreadGroupInvocations = wgpuSupportedLimits.limits.maxComputeInvocationsPerWorkgroup;
    }

    // Set texture info
    {
        auto& TextureInfo = AdapterInfo.Texture;

        TextureInfo.MaxTexture1DArraySlices = 0; // Not supported in WebGPU
        TextureInfo.MaxTexture2DArraySlices = wgpuSupportedLimits.limits.maxTextureArrayLayers;

        TextureInfo.MaxTexture1DDimension = wgpuSupportedLimits.limits.maxTextureDimension1D;
        TextureInfo.MaxTexture2DDimension = wgpuSupportedLimits.limits.maxTextureDimension2D;
        TextureInfo.MaxTexture3DDimension = wgpuSupportedLimits.limits.maxTextureDimension3D;

        TextureInfo.Texture2DMSSupported       = True;
        TextureInfo.Texture2DMSArraySupported  = False;
        TextureInfo.TextureViewSupported       = True;
        TextureInfo.CubemapArraysSupported     = True;
        TextureInfo.TextureView2DOn3DSupported = True;
    }

    // Set buffer info
    {
        auto& BufferInfo = AdapterInfo.Buffer;

        BufferInfo.ConstantBufferOffsetAlignment   = wgpuSupportedLimits.limits.minUniformBufferOffsetAlignment;
        BufferInfo.StructuredBufferOffsetAlignment = wgpuSupportedLimits.limits.minStorageBufferOffsetAlignment;
    }

    return AdapterInfo;
}

} // namespace

void EngineFactoryWebGPUImpl::EnumerateAdapters(Version              MinVersion,
                                                Uint32&              NumAdapters,
                                                GraphicsAdapterInfo* Adapters) const
{

    WGPUInstanceDescriptor wgpuInstanceDesc = {};
    WebGPUInstanceWrapper  wgpuInstance{wgpuCreateInstance(&wgpuInstanceDesc)};
    if (!wgpuInstance)
        LOG_ERROR_AND_THROW("Failed to create WebGPU instance");

    auto wgpuAdapters = FindCompatibleAdapters(wgpuInstance.Get(), MinVersion);
    if (Adapters == nullptr)
        NumAdapters = static_cast<Uint32>(wgpuAdapters.size());
    else
    {
        NumAdapters = (std::min)(NumAdapters, static_cast<Uint32>(wgpuAdapters.size()));
        for (Uint32 AdapterId = 0; AdapterId < NumAdapters; ++AdapterId)
        {
            auto& wgpuAdapter   = wgpuAdapters[AdapterId];
            Adapters[AdapterId] = GetGraphicsAdapterInfo(wgpuAdapter.Get());
        }
    }
}

void EngineFactoryWebGPUImpl::CreateDearchiver(const DearchiverCreateInfo& CreateInfo,
                                               IDearchiver**               ppDearchiver) const
{
    // TBase::CreateDearchiver<DearchiverVkImpl>(CreateInfo, ppDearchiver);
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
        WGPUInstanceDescriptor wgpuInstanceDesc = {};
        WebGPUInstanceWrapper  wgpuInstance{wgpuCreateInstance(&wgpuInstanceDesc)};
        if (!wgpuInstance)
            LOG_ERROR_AND_THROW("Failed to create WebGPU instance");

        auto Adapters = FindCompatibleAdapters(wgpuInstance.Get(), EngineCI.GraphicsAPIVersion);

        WebGPUAdapterWrapper SpecificAdapter{};
        if (EngineCI.AdapterId != DEFAULT_ADAPTER_ID)
        {
            if (EngineCI.AdapterId < Adapters.size())
                SpecificAdapter = std::move(Adapters[EngineCI.AdapterId]);
            else
                LOG_ERROR_AND_THROW(EngineCI.AdapterId, " is not a valid hardware adapter id. Total number of compatible adapters available on this system: ", Adapters.size());
        }
        else
        {
            SpecificAdapter = std::move(Adapters[0]);
        }

        WebGPUDeviceWrapper Device = CreateDeviceForAdapter(EngineCI, SpecificAdapter.Get());
        AttachToWebGPUDevice(wgpuInstance.Release(), SpecificAdapter.Release(), Device.Release(), EngineCI, ppDevice, ppImmediateContext);
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
        auto* pDeviceWebGPU        = ClassPtrCast<RenderDeviceWebGPUImpl>(pDevice);
        auto* pDeviceContextWebGPU = ClassPtrCast<DeviceContextWebGPUImpl>(pImmediateContext);
        auto& RawMemAllocator      = GetRawAllocator();

        auto* pSwapChainWebGPU = NEW_RC_OBJ(RawMemAllocator, "SwapChainWebGPUImpl instance", SwapChainWebGPUImpl)(SCDesc, pDeviceWebGPU, pDeviceContextWebGPU, Window);
        pSwapChainWebGPU->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain));
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

void EngineFactoryWebGPUImpl::AttachToWebGPUDevice(WGPUInstance                  wgpuInstance,
                                                   WGPUAdapter                   wgpuAdapter,
                                                   WGPUDevice                    wgpuDevice,
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
        const auto AdapterInfo = GetGraphicsAdapterInfo(wgpuAdapter);
        VerifyEngineCreateInfo(EngineCI, AdapterInfo);

        SetRawAllocator(EngineCI.pRawMemAllocator);
        auto& RawMemAllocator = GetRawAllocator();

        RenderDeviceWebGPUImpl* pRenderDeviceWebGPU{
            NEW_RC_OBJ(RawMemAllocator, "RenderDeviceWebGPUImpl instance", RenderDeviceWebGPUImpl)(
                RawMemAllocator, this, EngineCI, AdapterInfo, wgpuInstance, wgpuAdapter, wgpuDevice)};
        pRenderDeviceWebGPU->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice));

        DeviceContextWebGPUImpl* pDeviceContextWebGPU{
            NEW_RC_OBJ(RawMemAllocator, "DeviceContextWebGPUImpl instance", DeviceContextWebGPUImpl)(
                pRenderDeviceWebGPU, EngineCI,
                DeviceContextDesc{
                    EngineCI.pImmediateContextInfo ? EngineCI.pImmediateContextInfo[0].Name : nullptr,
                    pRenderDeviceWebGPU->GetAdapterInfo().Queues[0].QueueType,
                    False, // IsDeferred
                    0,     // Context id
                    0      // Queue id
                })};
        pDeviceContextWebGPU->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppImmediateContext));
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
#if !PLATFORM_EMSCRIPTEN
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
