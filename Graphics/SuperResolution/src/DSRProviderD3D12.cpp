/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "DSRProviderD3D12.hpp"

#if DILIGENT_DSR_SUPPORTED

#    include "SuperResolutionBase.hpp"
#    include "../../GraphicsEngineD3D12/include/pch.h"

#    include <directsr.h>

#    include "RenderDeviceD3D12Impl.hpp"
#    include "DeviceContextD3D12Impl.hpp"
#    include "DXGITypeConversions.hpp"

namespace Diligent
{

namespace
{

CComPtr<IDSRDevice> CreateDSRDevice(IRenderDevice* pDevice)
{
    HMODULE hD3D12 = GetModuleHandleA("d3d12.dll");
    if (!hD3D12)
    {
        LOG_WARNING_MESSAGE("d3d12.dll is not loaded. DirectSR features will be disabled.");
        return {};
    }

    using D3D12GetInterfaceProcType                = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    D3D12GetInterfaceProcType pfnD3D12GetInterface = reinterpret_cast<D3D12GetInterfaceProcType>(GetProcAddress(hD3D12, "D3D12GetInterface"));
    if (!pfnD3D12GetInterface)
    {
        LOG_WARNING_MESSAGE("D3D12GetInterface is not available. DirectSR features will be disabled.");
        return {};
    }

    CComPtr<ID3D12DSRDeviceFactory> pDSRFactory;
    if (HRESULT hr = pfnD3D12GetInterface(CLSID_D3D12DSRDeviceFactory, IID_PPV_ARGS(&pDSRFactory)); FAILED(hr))
    {
        LOG_WARNING_MESSAGE("Failed to create DirectSR device factory. HRESULT: ", hr);
        return {};
    }

    ID3D12Device* pd3d12Device = ClassPtrCast<RenderDeviceD3D12Impl>(pDevice)->GetD3D12Device();

    CComPtr<IDSRDevice> pDSRDevice;
    if (HRESULT hr = pDSRFactory->CreateDSRDevice(pd3d12Device, 0, IID_PPV_ARGS(&pDSRDevice)); FAILED(hr))
    {
        LOG_WARNING_MESSAGE("Failed to create DirectSR device. HRESULT: ", hr);
        return {};
    }

    LOG_INFO_MESSAGE("DirectSR device initialized successfully. ", pDSRDevice->GetNumSuperResVariants(), " upscaler variant(s) found.");
    return pDSRDevice;
}

DSR_SUPERRES_CREATE_ENGINE_FLAGS SuperResolutionFlagsToDSRFlags(SUPER_RESOLUTION_FLAGS Flags)
{
    DSR_SUPERRES_CREATE_ENGINE_FLAGS DSRFlags = DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE;

    if (Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE)
        DSRFlags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_AUTO_EXPOSURE;
    if (Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING)
        DSRFlags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_ENABLE_SHARPENING;

    return DSRFlags;
}

class SuperResolutionD3D12_DSR final : public SuperResolutionBase
{
public:
    SuperResolutionD3D12_DSR(IReferenceCounters*        pRefCounters,
                             RenderDeviceD3D12Impl*     pDevice,
                             const SuperResolutionDesc& Desc,
                             IDSRDevice*                pDSRDevice);

    ~SuperResolutionD3D12_DSR();

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final;

private:
    RefCntAutoPtr<RenderDeviceD3D12Impl>       m_pDevice;
    CComPtr<IDSRSuperResEngine>                m_pDSREngine;
    std::vector<CComPtr<IDSRSuperResUpscaler>> m_DSRUpscalers;
};

SuperResolutionD3D12_DSR::SuperResolutionD3D12_DSR(IReferenceCounters*        pRefCounters,
                                                   RenderDeviceD3D12Impl*     pDevice,
                                                   const SuperResolutionDesc& Desc,
                                                   IDSRDevice*                pDSRDevice) :
    SuperResolutionBase{pRefCounters, Desc},
    m_pDevice{pDevice},
    m_DSRUpscalers(pDevice->GetCommandQueueCount())
{

    ValidateTemporalSuperResolutionDesc(m_Desc);
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, Desc.MotionFormat == TEX_FORMAT_RG16_FLOAT, "MotionFormat must be TEX_FORMAT_RG16_FLOAT. Got: ", GetTextureFormatAttribs(Desc.MotionFormat).Name);
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, (Desc.Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE) != 0 || Desc.ExposureFormat != TEX_FORMAT_UNKNOWN,
                            "ExposureFormat must not be TEX_FORMAT_UNKNOWN when SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE is not set. "
                            "Either enable auto-exposure or specify a valid ExposureFormat (e.g. TEX_FORMAT_R32_FLOAT).");

    VERIFY_SUPER_RESOLUTION(m_Desc.Name, pDSRDevice != nullptr, "DirectSR device is not available");

    DSR_SUPERRES_CREATE_ENGINE_PARAMETERS CreateInfo = {};
    CreateInfo.VariantId                             = reinterpret_cast<const GUID&>(Desc.VariantId);
    CreateInfo.TargetFormat                          = TexFormatToDXGI_Format(Desc.OutputFormat);
    CreateInfo.SourceColorFormat                     = TexFormatToDXGI_Format(Desc.ColorFormat);
    CreateInfo.SourceDepthFormat                     = TexFormatToDXGI_Format(Desc.DepthFormat);
    CreateInfo.ExposureScaleFormat                   = TexFormatToDXGI_Format(Desc.ExposureFormat);
    CreateInfo.Flags                                 = SuperResolutionFlagsToDSRFlags(Desc.Flags);
    CreateInfo.MaxSourceSize                         = {Desc.InputWidth, Desc.InputHeight};
    CreateInfo.TargetSize                            = {Desc.OutputWidth, Desc.OutputHeight};

    if (HRESULT hr = pDSRDevice->CreateSuperResEngine(&CreateInfo, IID_PPV_ARGS(&m_pDSREngine)); FAILED(hr))
        LOG_ERROR_AND_THROW("Failed to create DirectSR super resolution engine. HRESULT: ", hr);

    // Cache the optimal jitter pattern
    {
        DSR_SIZE SourceSize  = {Desc.InputWidth, Desc.InputHeight};
        DSR_SIZE TargetSize  = {Desc.OutputWidth, Desc.OutputHeight};
        Uint32   PatternSize = 0;

        if (HRESULT hr = m_pDSREngine->GetOptimalJitterPattern(SourceSize, TargetSize, &PatternSize, nullptr); SUCCEEDED(hr) && PatternSize > 0)
        {
            std::vector<DSR_FLOAT2> DSRPattern(PatternSize);
            if (hr = m_pDSREngine->GetOptimalJitterPattern(SourceSize, TargetSize, &PatternSize, DSRPattern.data()); SUCCEEDED(hr))
            {
                m_JitterPattern.resize(PatternSize);
                for (Uint32 i = 0; i < PatternSize; ++i)
                {
                    m_JitterPattern[i].X = DSRPattern[i].X;
                    m_JitterPattern[i].Y = DSRPattern[i].Y;
                }
            }
        }
        else
        {
            PopulateHaltonJitterPattern(m_JitterPattern, 64);
            LOG_WARNING_MESSAGE("Failed to get optimal jitter pattern from DirectSR engine. HRESULT: ", hr);
        }
    }
}

SuperResolutionD3D12_DSR::~SuperResolutionD3D12_DSR() = default;

void DILIGENT_CALL_TYPE SuperResolutionD3D12_DSR::Execute(const ExecuteSuperResolutionAttribs& Attribs)
{
    ValidateTemporalExecuteSuperResolutionAttribs(m_Desc, Attribs);
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, Attribs.CameraNear > 0, "CameraNear must be greater than zero for temporal upscaling");
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, Attribs.CameraFar > 0, "CameraFar must be greater than zero for temporal upscaling.");
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, Attribs.CameraFovAngleVert > 0, "CameraFovAngleVert must be greater than zero for temporal upscaling.");
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, Attribs.TimeDeltaInSeconds >= 0, "TimeDeltaInSeconds must be non-negative.");

    DeviceContextD3D12Impl* pCtx = ClassPtrCast<DeviceContextD3D12Impl>(Attribs.pContext);

    const SoftwareQueueIndex QueueId = pCtx->GetCommandQueueId();
    VERIFY_EXPR(static_cast<size_t>(QueueId) < m_DSRUpscalers.size());

    // Lazily create an upscaler for this queue on first use.
    CComPtr<IDSRSuperResUpscaler>& pDSRUpscaler = m_DSRUpscalers[static_cast<size_t>(QueueId)];
    if (!pDSRUpscaler)
    {
        m_pDevice->LockCmdQueueAndRun(QueueId, [&](ICommandQueueD3D12* pCmdQueue) {
            if (HRESULT hr = m_pDSREngine->CreateUpscaler(pCmdQueue->GetD3D12CommandQueue(), IID_PPV_ARGS(&pDSRUpscaler)); FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to create DirectSR upscaler for queue ", static_cast<Uint32>(QueueId), ". HRESULT: ", hr);
        });
    }

    auto GetD3D12Resource = [](ITextureView* pView) -> ID3D12Resource* {
        if (pView != nullptr)
        {
            TextureD3D12Impl* pTexD3D12 = ClassPtrCast<TextureD3D12Impl>(pView->GetTexture());
            return pTexD3D12->GetD3D12Resource();
        }
        return nullptr;
    };

    DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS ExecuteParams = {};

    ExecuteParams.pTargetTexture            = GetD3D12Resource(Attribs.pOutputTextureView);
    ExecuteParams.TargetRegion              = {0, 0, static_cast<LONG>(m_Desc.OutputWidth), static_cast<LONG>(m_Desc.OutputHeight)};
    ExecuteParams.pSourceColorTexture       = GetD3D12Resource(Attribs.pColorTextureSRV);
    ExecuteParams.SourceColorRegion         = {0, 0, static_cast<LONG>(m_Desc.InputWidth), static_cast<LONG>(m_Desc.InputHeight)};
    ExecuteParams.pSourceDepthTexture       = GetD3D12Resource(Attribs.pDepthTextureSRV);
    ExecuteParams.SourceDepthRegion         = {0, 0, static_cast<LONG>(m_Desc.InputWidth), static_cast<LONG>(m_Desc.InputHeight)};
    ExecuteParams.pMotionVectorsTexture     = GetD3D12Resource(Attribs.pMotionVectorsSRV);
    ExecuteParams.MotionVectorsRegion       = {0, 0, static_cast<LONG>(m_Desc.InputWidth), static_cast<LONG>(m_Desc.InputHeight)};
    ExecuteParams.MotionVectorScale         = {Attribs.MotionVectorScaleX, Attribs.MotionVectorScaleY};
    ExecuteParams.CameraJitter              = {Attribs.JitterX, Attribs.JitterY};
    ExecuteParams.ExposureScale             = Attribs.ExposureScale;
    ExecuteParams.PreExposure               = Attribs.PreExposure;
    ExecuteParams.Sharpness                 = Attribs.Sharpness;
    ExecuteParams.CameraNear                = Attribs.CameraNear;
    ExecuteParams.CameraFar                 = Attribs.CameraFar;
    ExecuteParams.CameraFovAngleVert        = Attribs.CameraFovAngleVert;
    ExecuteParams.pExposureScaleTexture     = GetD3D12Resource(Attribs.pExposureTextureSRV);
    ExecuteParams.pIgnoreHistoryMaskTexture = GetD3D12Resource(Attribs.pIgnoreHistoryMaskTextureSRV);
    ExecuteParams.IgnoreHistoryMaskRegion   = {0, 0, static_cast<LONG>(m_Desc.InputWidth), static_cast<LONG>(m_Desc.InputHeight)};
    ExecuteParams.pReactiveMaskTexture      = GetD3D12Resource(Attribs.pReactiveMaskTextureSRV);
    ExecuteParams.ReactiveMaskRegion        = {0, 0, static_cast<LONG>(m_Desc.InputWidth), static_cast<LONG>(m_Desc.InputHeight)};

    DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags = DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE;
    if (Attribs.ResetHistory)
        Flags |= DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY;

    // Transition all textures to the states expected by DirectSR and flush the context.
    // DirectSR submits its own command list(s) to the command queue, so all rendering work must be submitted before DirectSR reads the inputs.
    // Input textures must be in D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, output must be in D3D12_RESOURCE_STATE_UNORDERED_ACCESS.
    DeviceContextD3D12Impl* pDeviceCtx = ClassPtrCast<DeviceContextD3D12Impl>(Attribs.pContext);
    pDeviceCtx->TransitionTextureState(Attribs.pColorTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    pDeviceCtx->TransitionTextureState(Attribs.pDepthTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    pDeviceCtx->TransitionTextureState(Attribs.pMotionVectorsSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    pDeviceCtx->TransitionTextureState(Attribs.pOutputTextureView->GetTexture(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (Attribs.pExposureTextureSRV)
        pDeviceCtx->TransitionTextureState(Attribs.pExposureTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if (Attribs.pReactiveMaskTextureSRV)
        pDeviceCtx->TransitionTextureState(Attribs.pReactiveMaskTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if (Attribs.pIgnoreHistoryMaskTextureSRV)
        pDeviceCtx->TransitionTextureState(Attribs.pIgnoreHistoryMaskTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    pDeviceCtx->Flush();

    if (HRESULT hr = pDSRUpscaler->Execute(&ExecuteParams, Attribs.TimeDeltaInSeconds, Flags); FAILED(hr))
        LOG_ERROR_MESSAGE("DirectSR Execute failed. HRESULT: ", hr);

    pDeviceCtx->TransitionTextureState(Attribs.pOutputTextureView->GetTexture(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
}

} // anonymous namespace

DSRProviderD3D12::DSRProviderD3D12(IRenderDevice* pDevice) :
    m_pDevice{pDevice},
    m_pDSRDevice{CreateDSRDevice(pDevice).Detach()}
{
}

DSRProviderD3D12::~DSRProviderD3D12()
{
    if (m_pDSRDevice)
        m_pDSRDevice->Release();
}

void DSRProviderD3D12::EnumerateVariants(std::vector<SuperResolutionInfo>& Variants)
{
    if (!m_pDSRDevice)
        return;

    static_assert(sizeof(SuperResolutionInfo::VariantId) == sizeof(DSR_SUPERRES_VARIANT_DESC::VariantId), "GUID/INTERFACE_ID size mismatch");

    const Uint32 DSRNumVariants = m_pDSRDevice->GetNumSuperResVariants();
    for (Uint32 Idx = 0; Idx < DSRNumVariants; ++Idx)
    {
        DSR_SUPERRES_VARIANT_DESC VariantDesc = {};
        if (FAILED(m_pDSRDevice->GetSuperResVariantDesc(Idx, &VariantDesc)))
            continue;

        SuperResolutionInfo Info{};
        Info.Type = SUPER_RESOLUTION_TYPE_TEMPORAL;

        Info.TemporalCapFlags = SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_NATIVE;
        if (VariantDesc.Flags & DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_EXPOSURE_SCALE_TEXTURE)
            Info.TemporalCapFlags |= SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_EXPOSURE_SCALE_TEXTURE;
        if (VariantDesc.Flags & DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_IGNORE_HISTORY_MASK)
            Info.TemporalCapFlags |= SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_IGNORE_HISTORY_MASK;
        if (VariantDesc.Flags & DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_REACTIVE_MASK)
            Info.TemporalCapFlags |= SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_REACTIVE_MASK;
        if (VariantDesc.Flags & DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_SHARPNESS)
            Info.TemporalCapFlags |= SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS;

        snprintf(Info.Name, sizeof(Info.Name), "DSR: %s", VariantDesc.VariantName);
        memcpy(&Info.VariantId, &VariantDesc.VariantId, sizeof(Info.VariantId));

        Variants.push_back(Info);
    }
}

void DSRProviderD3D12::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                                        SuperResolutionSourceSettings&              Settings)
{
    Settings = {};

    DEV_CHECK_ERR(m_pDSRDevice != nullptr, "DirectSR device must not be null");
    ValidateSourceSettingsAttribs(Attribs);

    DSR_OPTIMIZATION_TYPE DSROptType = DSR_OPTIMIZATION_TYPE_BALANCED;
    switch (Attribs.OptimizationType)
    {
        // clang-format off
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY:      DSROptType = DSR_OPTIMIZATION_TYPE_MAX_QUALITY;       break;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_QUALITY:     DSROptType = DSR_OPTIMIZATION_TYPE_HIGH_QUALITY;      break;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED:         DSROptType = DSR_OPTIMIZATION_TYPE_BALANCED;          break;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_PERFORMANCE: DSROptType = DSR_OPTIMIZATION_TYPE_HIGH_PERFORMANCE;  break;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE:  DSROptType = DSR_OPTIMIZATION_TYPE_MAX_PERFORMANCE;   break;
        default: break;
            // clang-format on
    }

    const Uint32 NumVariants  = m_pDSRDevice->GetNumSuperResVariants();
    Uint32       VariantIndex = UINT32_MAX;
    for (Uint32 Idx = 0; Idx < NumVariants; ++Idx)
    {
        DSR_SUPERRES_VARIANT_DESC VariantDesc = {};
        if (SUCCEEDED(m_pDSRDevice->GetSuperResVariantDesc(Idx, &VariantDesc)))
        {
            if (memcmp(&VariantDesc.VariantId, &Attribs.VariantId, sizeof(GUID)) == 0)
            {
                VariantIndex = Idx;
                break;
            }
        }
    }

    if (VariantIndex == UINT32_MAX)
    {
        LOG_WARNING_MESSAGE("DirectSR variant not found for the specified VariantId");
        return;
    }

    DSR_SIZE TargetSize = {Attribs.OutputWidth, Attribs.OutputHeight};

    DSR_SUPERRES_CREATE_ENGINE_FLAGS DSRCreateFlags = DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE;
    if (Attribs.Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE)
        DSRCreateFlags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_AUTO_EXPOSURE;
    if (Attribs.Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING)
        DSRCreateFlags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_ENABLE_SHARPENING;

    DSR_SUPERRES_SOURCE_SETTINGS SourceSettings = {};
    if (HRESULT hr = m_pDSRDevice->QuerySuperResSourceSettings(VariantIndex, TargetSize, TexFormatToDXGI_Format(Attribs.OutputFormat), DSROptType, DSRCreateFlags, &SourceSettings); SUCCEEDED(hr))
    {
        Settings.OptimalInputWidth  = SourceSettings.OptimalSize.Width;
        Settings.OptimalInputHeight = SourceSettings.OptimalSize.Height;
    }
    else
    {
        LOG_WARNING_MESSAGE("DirectSR QuerySuperResSourceSettings failed. HRESULT: ", hr);
    }
}

void DSRProviderD3D12::CreateSuperResolution(const SuperResolutionDesc& Desc,
                                                            ISuperResolution**         ppUpscaler)
{
    DEV_CHECK_ERR(m_pDSRDevice != nullptr, "DirectSR device must not be null");
    DEV_CHECK_ERR(m_pDevice != nullptr, "Render device must not be null");

    RenderDeviceD3D12Impl*    pDeviceD3D12 = ClassPtrCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr());
    SuperResolutionD3D12_DSR* pUpscaler    = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionD3D12_DSR instance", SuperResolutionD3D12_DSR)(pDeviceD3D12, Desc, m_pDSRDevice);
    pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
}

} // namespace Diligent

#else

namespace Diligent
{

DSRProviderD3D12::DSRProviderD3D12(IRenderDevice*)
{
    LOG_INFO_MESSAGE("DirectSR is not supported on this platform");
}
DSRProviderD3D12::~DSRProviderD3D12() {}
void DSRProviderD3D12::EnumerateVariants(std::vector<SuperResolutionInfo>&) {}
void DSRProviderD3D12::GetSourceSettings(const SuperResolutionSourceSettingsAttribs&, SuperResolutionSourceSettings&) {}
void DSRProviderD3D12::CreateSuperResolution(const SuperResolutionDesc&, ISuperResolution**) {}

} // namespace Diligent

#endif
