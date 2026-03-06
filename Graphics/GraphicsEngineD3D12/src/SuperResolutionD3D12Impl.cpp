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

#include "pch.h"

#include "SuperResolutionD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceContextD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TextureViewD3D12Impl.hpp"
#include "DXGITypeConversions.hpp"
#include "D3D12TypeConversions.hpp"
#include "GraphicsAccessories.hpp"
#include "DebugUtilities.hpp"

namespace Diligent
{

SuperResolutionD3D12Impl::SuperResolutionD3D12Impl(IReferenceCounters*        pRefCounters,
                                                   RenderDeviceD3D12Impl*     pDevice,
                                                   const SuperResolutionDesc& Desc) :
    TSRUpscalerBase{pRefCounters, pDevice, Desc},
    m_DSRUpscalers(pDevice->GetCommandQueueCount())
{
    DEV_CHECK_ERR(Desc.OutputWidth > 0 && Desc.OutputHeight > 0, "Output resolution must be greater than zero");
    DEV_CHECK_ERR(Desc.OutputFormat != TEX_FORMAT_UNKNOWN, "OutputFormat must not be TEX_FORMAT_UNKNOWN");
    DEV_CHECK_ERR(Desc.ColorFormat != TEX_FORMAT_UNKNOWN, "ColorFormat must not be TEX_FORMAT_UNKNOWN");
    DEV_CHECK_ERR(Desc.InputWidth > 0 && Desc.InputHeight > 0, "InputWidth and InputHeight must be greater than zero. ");
    DEV_CHECK_ERR(Desc.InputWidth <= Desc.OutputWidth && Desc.InputHeight <= Desc.OutputHeight, "Input resolution must not exceed output resolution");
    DEV_CHECK_ERR(Desc.DepthFormat != TEX_FORMAT_UNKNOWN, "DepthFormat must not be TEX_FORMAT_UNKNOWN. DirectSR upscalers are always temporal and require a depth buffer.");
    DEV_CHECK_ERR(Desc.MotionFormat != TEX_FORMAT_UNKNOWN, "MotionFormat must not be TEX_FORMAT_UNKNOWN. DirectSR upscalers are always temporal and require motion vectors.");
    DEV_CHECK_ERR(Desc.MotionFormat == TEX_FORMAT_RG16_FLOAT, "MotionFormat must be TEX_FORMAT_RG16_FLOAT. Got: ", GetTextureFormatAttribs(Desc.MotionFormat).Name);

    // Validate create flags against variant capabilities
    {
        const auto&                         SRProps         = pDevice->GetAdapterInfo().SuperResolution;
        SUPER_RESOLUTION_TEMPORAL_CAP_FLAGS VariantCapFlags = SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_NONE;
        for (Uint8 Idx = 0; Idx < SRProps.NumUpscalers; ++Idx)
        {
            if (SRProps.Upscalers[Idx].VariantId == Desc.VariantId)
            {
                VariantCapFlags = SRProps.Upscalers[Idx].TemporalCapFlags;
                break;
            }
        }

        if (Desc.Flags & SUPER_RESOLUTION_CREATE_FLAG_ENABLE_SHARPENING)
        {
            DEV_CHECK_ERR(VariantCapFlags & SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS,
                          "SUPER_RESOLUTION_CREATE_FLAG_ENABLE_SHARPENING is set, but the selected upscaler variant "
                          "does not report SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS capability.");
        }

        if (Desc.ReactiveMaskFormat != TEX_FORMAT_UNKNOWN)
        {
            DEV_CHECK_ERR(VariantCapFlags & SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_REACTIVE_MASK,
                          "ReactiveMaskFormat is set, but the selected upscaler variant "
                          "does not report SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_REACTIVE_MASK capability.");
        }

        if (Desc.IgnoreHistoryMaskFormat != TEX_FORMAT_UNKNOWN)
        {
            DEV_CHECK_ERR(VariantCapFlags & SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_IGNORE_HISTORY_MASK,
                          "IgnoreHistoryMaskFormat is set, but the selected upscaler variant "
                          "does not report SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_IGNORE_HISTORY_MASK capability.");
        }

        if (Desc.ExposureFormat != TEX_FORMAT_UNKNOWN)
        {
            DEV_CHECK_ERR(!(Desc.Flags & SUPER_RESOLUTION_CREATE_FLAG_AUTO_EXPOSURE),
                          "ExposureFormat is set, but SUPER_RESOLUTION_CREATE_FLAG_AUTO_EXPOSURE is also enabled. "
                          "Disable auto-exposure to use a custom exposure texture.");
            DEV_CHECK_ERR(VariantCapFlags & SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_EXPOSURE_SCALE_TEXTURE,
                          "ExposureFormat is set, but the selected upscaler variant "
                          "does not report SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_EXPOSURE_SCALE_TEXTURE capability.");
        }
    }

    IDSRDevice* pDSRDevice = pDevice->GetDSRDevice();
    DEV_CHECK_ERR(pDSRDevice != nullptr, "DirectSR device is not available");

    DSR_SUPERRES_CREATE_ENGINE_PARAMETERS CreateInfo = {};
    CreateInfo.VariantId                             = reinterpret_cast<const GUID&>(Desc.VariantId);
    CreateInfo.TargetFormat                          = TexFormatToDXGI_Format(Desc.OutputFormat);
    CreateInfo.SourceColorFormat                     = TexFormatToDXGI_Format(Desc.ColorFormat);
    CreateInfo.SourceDepthFormat                     = TexFormatToDXGI_Format(Desc.DepthFormat);
    CreateInfo.ExposureScaleFormat                   = TexFormatToDXGI_Format(Desc.ExposureFormat);
    CreateInfo.Flags                                 = SuperResolutionCreateFlagsToDSRFlags(Desc.Flags);
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
            m_JitterPattern.resize(PatternSize);
            if (hr = m_pDSREngine->GetOptimalJitterPattern(SourceSize, TargetSize, &PatternSize, m_JitterPattern.data()); FAILED(hr))
                m_JitterPattern.clear();
        }
    }
}

SuperResolutionD3D12Impl::~SuperResolutionD3D12Impl() = default;

void DILIGENT_CALL_TYPE SuperResolutionD3D12Impl::GetOptimalJitterPattern(Uint32 Index, float* pJitterX, float* pJitterY) const
{
    DEV_CHECK_ERR(pJitterX != nullptr && pJitterY != nullptr, "pJitterX and pJitterY must not be null");

    if (!m_JitterPattern.empty())
    {
        const Uint32 WrappedIndex = Index % static_cast<Uint32>(m_JitterPattern.size());
        *pJitterX                 = m_JitterPattern[WrappedIndex].X;
        *pJitterY                 = m_JitterPattern[WrappedIndex].Y;
    }
    else
    {
        *pJitterX = 0.0f;
        *pJitterY = 0.0f;
    }
}

void SuperResolutionD3D12Impl::Execute(DeviceContextD3D12Impl& Ctx, const ExecuteSuperResolutionAttribs& Attribs)
{
    DEV_CHECK_ERR(Attribs.pColorTextureSRV != nullptr, "Color texture SRV must not be null");
    DEV_CHECK_ERR(Attribs.pOutputTextureView != nullptr, "Output texture view must not be null");
    DEV_CHECK_ERR(Attribs.pDepthTextureSRV != nullptr, "Depth texture SRV must not be null");
    DEV_CHECK_ERR(Attribs.pMotionVectorsSRV != nullptr, "Motion vectors SRV must not be null. DirectSR upscalers are always temporal");
    DEV_CHECK_ERR(Attribs.CameraNear > 0, "CameraNear must be greater than zero for temporal upscaling");
    DEV_CHECK_ERR(Attribs.CameraFar > 0, "CameraFar must be greater than zero for temporal upscaling.");
    DEV_CHECK_ERR(Attribs.CameraFovAngleVert > 0, "CameraFovAngleVert must be greater than zero for temporal upscaling.");
    DEV_CHECK_ERR(Attribs.TimeDeltaInSeconds >= 0, "TimeDeltaInSeconds must be non-negative.");

    const SoftwareQueueIndex QueueId = Ctx.GetCommandQueueId();
    VERIFY_EXPR(static_cast<size_t>(QueueId) < m_DSRUpscalers.size());

    // Lazily create an upscaler for this queue on first use.
    auto& pDSRUpscaler = m_DSRUpscalers[static_cast<size_t>(QueueId)];
    if (!pDSRUpscaler)
    {
        m_pDevice->LockCmdQueueAndRun(QueueId, [&](ICommandQueueD3D12* pCmdQueue) {
            if (HRESULT hr = m_pDSREngine->CreateUpscaler(pCmdQueue->GetD3D12CommandQueue(), IID_PPV_ARGS(&pDSRUpscaler)); FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to create DirectSR upscaler for queue ", static_cast<Uint32>(QueueId), ". HRESULT: ", hr);
        });
    }

    // Validate color texture
    {
        const auto& TexDesc = Attribs.pColorTextureSRV->GetTexture()->GetDesc();
        DEV_CHECK_ERR((TexDesc.BindFlags & BIND_SHADER_RESOURCE) != 0,
                      "Color texture '", TexDesc.Name, "' must have BIND_SHADER_RESOURCE flag");
        DEV_CHECK_ERR(TexDesc.Width >= m_Desc.InputWidth && TexDesc.Height >= m_Desc.InputHeight,
                      "Color texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                      ") must be at least the upscaler input resolution (", m_Desc.InputWidth, "x", m_Desc.InputHeight, ")");
    }

    // Validate output texture
    {
        const auto& TexDesc = Attribs.pOutputTextureView->GetTexture()->GetDesc();
        DEV_CHECK_ERR((TexDesc.BindFlags & BIND_UNORDERED_ACCESS) != 0,
                      "Output texture '", TexDesc.Name, "' must have BIND_UNORDERED_ACCESS flag");
        DEV_CHECK_ERR(TexDesc.Width == m_Desc.OutputWidth && TexDesc.Height == m_Desc.OutputHeight,
                      "Output texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                      ") must match the upscaler output resolution (", m_Desc.OutputWidth, "x", m_Desc.OutputHeight, ")");
    }

    // Validate depth texture
    if (Attribs.pDepthTextureSRV != nullptr)
    {
        const auto& TexDesc = Attribs.pDepthTextureSRV->GetTexture()->GetDesc();
        DEV_CHECK_ERR((TexDesc.BindFlags & BIND_SHADER_RESOURCE) != 0,
                      "Depth texture '", TexDesc.Name, "' must have BIND_SHADER_RESOURCE flag");
        DEV_CHECK_ERR(TexDesc.Width >= m_Desc.InputWidth && TexDesc.Height >= m_Desc.InputHeight,
                      "Depth texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                      ") must be at least the upscaler input resolution (", m_Desc.InputWidth, "x", m_Desc.InputHeight, ")");
    }

    // Validate motion vectors texture
    if (Attribs.pMotionVectorsSRV != nullptr)
    {
        const auto& TexDesc = Attribs.pMotionVectorsSRV->GetTexture()->GetDesc();
        DEV_CHECK_ERR((TexDesc.BindFlags & BIND_SHADER_RESOURCE) != 0,
                      "Motion vectors texture '", TexDesc.Name, "' must have BIND_SHADER_RESOURCE flag");
        DEV_CHECK_ERR(TexDesc.Width >= m_Desc.InputWidth && TexDesc.Height >= m_Desc.InputHeight,
                      "Motion vectors texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                      ") must be at least the upscaler input resolution (", m_Desc.InputWidth, "x", m_Desc.InputHeight, ")");
    }

    auto GetD3D12Resource = [](ITextureView* pView) -> ID3D12Resource* {
        if (pView != nullptr)
        {
            auto* pTexD3D12 = ClassPtrCast<TextureD3D12Impl>(pView->GetTexture());
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
    ExecuteParams.pReactiveMaskTexture      = GetD3D12Resource(Attribs.pReactiveMaskTextureSRV);

    DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags = DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE;
    if (Attribs.ResetHistory)
        Flags |= DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY;

    if (HRESULT hr = pDSRUpscaler->Execute(&ExecuteParams, Attribs.TimeDeltaInSeconds, Flags); FAILED(hr))
        LOG_ERROR_MESSAGE("DirectSR Execute failed. HRESULT: ", hr);
}

} // namespace Diligent
