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

#include "DLSSProviderD3D11.hpp"

#if D3D11_SUPPORTED && DILIGENT_DLSS_SUPPORTED

#    include "SuperResolutionDLSS.hpp"
#    include "SuperResolutionBase.hpp"
#    include "SuperResolutionVariants.hpp"

#    include <nvsdk_ngx_helpers.h>

#    include "../../GraphicsEngineD3D11/include/pch.h"
#    include "RenderDeviceD3D11Impl.hpp"
#    include "DeviceContextD3D11Impl.hpp"
#    include "TextureBaseD3D11.hpp"

namespace Diligent
{

namespace
{

class SuperResolutionD3D11_DLSS final : public SuperResolutionBase
{
public:
    SuperResolutionD3D11_DLSS(IReferenceCounters*        pRefCounters,
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

    ~SuperResolutionD3D11_DLSS()
    {
        if (m_pDLSSFeature != nullptr)
            NVSDK_NGX_D3D11_ReleaseFeature(m_pDLSSFeature);
    }

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final
    {
        ValidateTemporalExecuteSuperResolutionAttribs(m_Desc, Attribs);

        if (m_pDLSSFeature == nullptr)
            CreateFeature(Attribs);

        DeviceContextD3D11Impl* pCtxImpl = ClassPtrCast<DeviceContextD3D11Impl>(Attribs.pContext);

        auto GetD3D11Resource = [](ITextureView* pView) -> ID3D11Resource* {
            if (pView != nullptr)
                return ClassPtrCast<TextureBaseD3D11>(pView->GetTexture())->GetD3D11Texture();
            return nullptr;
        };

        ID3D11DeviceContext* pd3d11DeviceContext = pCtxImpl->GetD3D11DeviceContext();

        NVSDK_NGX_D3D11_DLSS_Eval_Params EvalParams = {};
        EvalParams.Feature.pInColor                 = GetD3D11Resource(Attribs.pColorTextureSRV);
        EvalParams.Feature.pInOutput                = GetD3D11Resource(Attribs.pOutputTextureView);
        EvalParams.pInDepth                         = GetD3D11Resource(Attribs.pDepthTextureSRV);
        EvalParams.pInMotionVectors                 = GetD3D11Resource(Attribs.pMotionVectorsSRV);
        EvalParams.pInExposureTexture               = GetD3D11Resource(Attribs.pExposureTextureSRV);
        EvalParams.pInTransparencyMask              = GetD3D11Resource(Attribs.pReactiveMaskTextureSRV);
        EvalParams.pInBiasCurrentColorMask          = GetD3D11Resource(Attribs.pIgnoreHistoryMaskTextureSRV);
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

        NVSDK_NGX_Result Result = NGX_D3D11_EVALUATE_DLSS_EXT(pd3d11DeviceContext, m_pDLSSFeature, m_pNGXParams, &EvalParams);
        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_MESSAGE("DLSS D3D11 evaluation failed. NGX Result: ", static_cast<Uint32>(Result));
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

        ID3D11DeviceContext* pd3d11Ctx = ClassPtrCast<DeviceContextD3D11Impl>(Attribs.pContext)->GetD3D11DeviceContext();
        NVSDK_NGX_Result     Result    = NGX_D3D11_CREATE_DLSS_EXT(pd3d11Ctx, &m_pDLSSFeature, m_pNGXParams, &DLSSCreateParams);

        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_AND_THROW("Failed to create DLSS D3D11 feature. NGX Result: ", static_cast<Uint32>(Result));
    }

    RefCntAutoPtr<IRenderDevice> m_pDevice;
    NVSDK_NGX_Handle*            m_pDLSSFeature = nullptr;
    NVSDK_NGX_Parameter*         m_pNGXParams   = nullptr;
};

} // anonymous namespace


DLSSProviderD3D11::DLSSProviderD3D11(IRenderDevice* pDevice) :
    m_pDevice{pDevice}
{
    ID3D11Device*    pd3d11Device = ClassPtrCast<RenderDeviceD3D11Impl>(pDevice)->GetD3D11Device();
    NVSDK_NGX_Result Result       = NVSDK_NGX_D3D11_Init_with_ProjectID(DLSSProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0", DLSSAppDataPath, pd3d11Device);
    if (NVSDK_NGX_FAILED(Result))
        LOG_ERROR_AND_THROW("NVIDIA NGX D3D11 initialization failed. Result: ", static_cast<Uint32>(Result));

    Result = NVSDK_NGX_D3D11_GetCapabilityParameters(&m_pNGXParams);
    if (NVSDK_NGX_FAILED(Result) || m_pNGXParams == nullptr)
        LOG_ERROR_AND_THROW("Failed to get NGX D3D11 capability parameters. Result: ", static_cast<Uint32>(Result));
}

DLSSProviderD3D11::~DLSSProviderD3D11()
{
    if (m_pNGXParams != nullptr)
        NVSDK_NGX_D3D11_DestroyParameters(m_pNGXParams);
    NVSDK_NGX_D3D11_Shutdown1(ClassPtrCast<RenderDeviceD3D11Impl>(m_pDevice.RawPtr())->GetD3D11Device());
}

void DLSSProviderD3D11::EnumerateVariants(std::vector<SuperResolutionInfo>& Variants)
{
    EnumerateDLSSVariants(m_pNGXParams, Variants);
}

void DLSSProviderD3D11::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                                         SuperResolutionSourceSettings&              Settings)
{
    GetDLSSSourceSettings(m_pNGXParams, Attribs, Settings);
}

void DLSSProviderD3D11::CreateSuperResolution(const SuperResolutionDesc& Desc,
                                                             ISuperResolution**         ppUpscaler)
{
    DEV_CHECK_ERR(m_pDevice != nullptr, "Render device must not be null");
    DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

    SuperResolutionD3D11_DLSS* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionD3D11_DLSS instance", SuperResolutionD3D11_DLSS)(m_pDevice, Desc, m_pNGXParams);
    pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
}

} // namespace Diligent

#else

namespace Diligent
{

DLSSProviderD3D11::DLSSProviderD3D11(IRenderDevice*)
{
    LOG_INFO_MESSAGE("DLSS is not supported on this platform for D3D11 backend");
}
DLSSProviderD3D11::~DLSSProviderD3D11() {}
void DLSSProviderD3D11::EnumerateVariants(std::vector<SuperResolutionInfo>&) {}
void DLSSProviderD3D11::GetSourceSettings(const SuperResolutionSourceSettingsAttribs&, SuperResolutionSourceSettings&) {}
void DLSSProviderD3D11::CreateSuperResolution(const SuperResolutionDesc&, ISuperResolution**) {}

} // namespace Diligent

#endif
