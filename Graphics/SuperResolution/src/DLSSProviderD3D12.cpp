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

#include "DLSSProviderD3D12.hpp"

#if D3D12_SUPPORTED && DILIGENT_DLSS_SUPPORTED

#    include "SuperResolutionDLSS.hpp"
#    include "SuperResolutionBase.hpp"
#    include "SuperResolutionVariants.hpp"

#    include <nvsdk_ngx_helpers.h>

#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "DeviceContextD3D12Impl.hpp"
#    include "TextureD3D12Impl.hpp"

namespace Diligent
{

namespace
{

class SuperResolutionD3D12_DLSS final : public SuperResolutionBase
{
public:
    SuperResolutionD3D12_DLSS(IReferenceCounters*        pRefCounters,
                              IRenderDevice*             pDevice,
                              const SuperResolutionDesc& Desc,
                              NVSDK_NGX_Parameter*       pNGXParams) :
        SuperResolutionBase{pRefCounters, Desc},
        m_pDevice{pDevice},
        m_pNGXParams{pNGXParams}
    {
        ValidateTemporalSuperResolutionDesc(m_Desc);
        PopulateHaltonJitterPattern(m_JitterPattern, 64);
    }

    ~SuperResolutionD3D12_DLSS()
    {
        if (m_pDLSSFeature != nullptr)
            NVSDK_NGX_D3D12_ReleaseFeature(m_pDLSSFeature);
    }

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final
    {
        ValidateTemporalExecuteSuperResolutionAttribs(m_Desc, Attribs);

        if (m_pDLSSFeature == nullptr)
            CreateFeature(Attribs);

        DeviceContextD3D12Impl* pCtxImpl = ClassPtrCast<DeviceContextD3D12Impl>(Attribs.pContext);

        auto GetD3D12Resource = [](ITextureView* pView) -> ID3D12Resource* {
            if (pView != nullptr)
                return ClassPtrCast<TextureD3D12Impl>(pView->GetTexture())->GetD3D12Resource();
            return nullptr;
        };

        pCtxImpl->TransitionTextureState(Attribs.pColorTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        pCtxImpl->TransitionTextureState(Attribs.pDepthTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        pCtxImpl->TransitionTextureState(Attribs.pMotionVectorsSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        pCtxImpl->TransitionTextureState(Attribs.pOutputTextureView->GetTexture(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (Attribs.pExposureTextureSRV)
            pCtxImpl->TransitionTextureState(Attribs.pExposureTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if (Attribs.pReactiveMaskTextureSRV)
            pCtxImpl->TransitionTextureState(Attribs.pReactiveMaskTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if (Attribs.pIgnoreHistoryMaskTextureSRV)
            pCtxImpl->TransitionTextureState(Attribs.pIgnoreHistoryMaskTextureSRV->GetTexture(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        ID3D12GraphicsCommandList* pCmdList = pCtxImpl->GetD3D12CommandList();

        NVSDK_NGX_D3D12_DLSS_Eval_Params EvalParams = {};
        EvalParams.Feature.pInColor                 = GetD3D12Resource(Attribs.pColorTextureSRV);
        EvalParams.Feature.pInOutput                = GetD3D12Resource(Attribs.pOutputTextureView);
        EvalParams.pInDepth                         = GetD3D12Resource(Attribs.pDepthTextureSRV);
        EvalParams.pInMotionVectors                 = GetD3D12Resource(Attribs.pMotionVectorsSRV);
        EvalParams.pInExposureTexture               = GetD3D12Resource(Attribs.pExposureTextureSRV);
        EvalParams.pInTransparencyMask              = GetD3D12Resource(Attribs.pReactiveMaskTextureSRV);
        EvalParams.pInBiasCurrentColorMask          = GetD3D12Resource(Attribs.pIgnoreHistoryMaskTextureSRV);
        EvalParams.Feature.InSharpness              = Attribs.Sharpness;
        EvalParams.InJitterOffsetX                  = Attribs.JitterX;
        EvalParams.InJitterOffsetY                  = Attribs.JitterY;
        EvalParams.InReset                          = Attribs.ResetHistory ? 1 : 0;
        EvalParams.InMVScaleX                       = Attribs.MotionVectorScaleX;
        EvalParams.InMVScaleY                       = Attribs.MotionVectorScaleY;
        EvalParams.InRenderSubrectDimensions.Width  = m_Desc.InputWidth;
        EvalParams.InRenderSubrectDimensions.Height = m_Desc.InputHeight;
        EvalParams.InPreExposure                    = Attribs.PreExposure;
        EvalParams.InExposureScale                  = Attribs.ExposureScale;

        NVSDK_NGX_Result Result = NGX_D3D12_EVALUATE_DLSS_EXT(pCmdList, m_pDLSSFeature, m_pNGXParams, &EvalParams);
        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_MESSAGE("DLSS D3D12 evaluation failed. NGX Result: ", static_cast<Uint32>(Result));

        pCtxImpl->TransitionTextureState(Attribs.pOutputTextureView->GetTexture(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        pCtxImpl->Flush();
    }

private:
    void CreateFeature(const ExecuteSuperResolutionAttribs& Attribs)
    {
        Int32 DLSSCreateFeatureFlags = SuperResolutionFlagsToDLSSFeatureFlags(m_Desc.Flags);
        if (Attribs.CameraNear > Attribs.CameraFar)
            DLSSCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;

        NVSDK_NGX_DLSS_Create_Params DLSSCreateParams = {};
        DLSSCreateParams.Feature.InWidth              = m_Desc.InputWidth;
        DLSSCreateParams.Feature.InHeight             = m_Desc.InputHeight;
        DLSSCreateParams.Feature.InTargetWidth        = m_Desc.OutputWidth;
        DLSSCreateParams.Feature.InTargetHeight       = m_Desc.OutputHeight;
        DLSSCreateParams.InFeatureCreateFlags         = DLSSCreateFeatureFlags;

        ID3D12GraphicsCommandList* pCmdList = ClassPtrCast<DeviceContextD3D12Impl>(Attribs.pContext)->GetD3D12CommandList();
        NVSDK_NGX_Result           Result   = NGX_D3D12_CREATE_DLSS_EXT(pCmdList, 1, 1, &m_pDLSSFeature, m_pNGXParams, &DLSSCreateParams);

        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_AND_THROW("Failed to create DLSS D3D12 feature. NGX Result: ", static_cast<Uint32>(Result));
    }

    RefCntAutoPtr<IRenderDevice> m_pDevice;
    NVSDK_NGX_Handle*            m_pDLSSFeature = nullptr;
    NVSDK_NGX_Parameter*         m_pNGXParams   = nullptr;
};

} // anonymous namespace


DLSSProviderD3D12::DLSSProviderD3D12(IRenderDevice* pDevice) :
    m_pDevice{pDevice}
{
    ID3D12Device*    pd3d12Device = ClassPtrCast<RenderDeviceD3D12Impl>(pDevice)->GetD3D12Device();
    NVSDK_NGX_Result Result       = NVSDK_NGX_D3D12_Init_with_ProjectID(DLSSProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0", DLSSAppDataPath, pd3d12Device);
    if (NVSDK_NGX_FAILED(Result))
        LOG_ERROR_AND_THROW("NVIDIA NGX D3D12 initialization failed. Result: ", static_cast<Uint32>(Result));

    Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_pNGXParams);
    if (NVSDK_NGX_FAILED(Result) || m_pNGXParams == nullptr)
        LOG_ERROR_AND_THROW("Failed to get NGX D3D12 capability parameters. Result: ", static_cast<Uint32>(Result));
}

DLSSProviderD3D12::~DLSSProviderD3D12()
{
    if (m_pNGXParams != nullptr)
        NVSDK_NGX_D3D12_DestroyParameters(m_pNGXParams);
    NVSDK_NGX_D3D12_Shutdown1(ClassPtrCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr())->GetD3D12Device());
}

void DLSSProviderD3D12::EnumerateVariants(std::vector<SuperResolutionInfo>& Variants)
{
    EnumerateDLSSVariants(m_pNGXParams, Variants);
}

void DLSSProviderD3D12::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                                         SuperResolutionSourceSettings&              Settings)
{
    GetDLSSSourceSettings(m_pNGXParams, Attribs, Settings);
}

void DLSSProviderD3D12::CreateSuperResolution(const SuperResolutionDesc& Desc,
                                                             ISuperResolution**         ppUpscaler)
{
    DEV_CHECK_ERR(m_pDevice != nullptr, "Render device must not be null");
    DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

    SuperResolutionD3D12_DLSS* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionD3D12_DLSS instance", SuperResolutionD3D12_DLSS)(m_pDevice, Desc, m_pNGXParams);
    pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
}

} // namespace Diligent

#else

namespace Diligent
{

DLSSProviderD3D12::DLSSProviderD3D12(IRenderDevice*)
{
    LOG_INFO_MESSAGE("DLSS is not supported on this platform for D3D12 backend");
}
DLSSProviderD3D12::~DLSSProviderD3D12() {}
void DLSSProviderD3D12::EnumerateVariants(std::vector<SuperResolutionInfo>&) {}
void DLSSProviderD3D12::GetSourceSettings(const SuperResolutionSourceSettingsAttribs&, SuperResolutionSourceSettings&) {}
void DLSSProviderD3D12::CreateSuperResolution(const SuperResolutionDesc&, ISuperResolution**) {}

} // namespace Diligent

#endif
