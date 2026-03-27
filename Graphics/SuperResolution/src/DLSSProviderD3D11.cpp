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

#include "SuperResolutionProvider.hpp"

#include "SuperResolutionDLSS.hpp"
#include "SuperResolutionBase.hpp"
#include "SuperResolutionVariants.hpp"

#include <d3d11.h>
#include <atlbase.h>
#include <nvsdk_ngx_helpers.h>

#include "RenderDeviceD3D11.h"
#include "DeviceContextD3D11.h"
#include "TextureD3D11.h"
#include "EngineMemory.h"

namespace Diligent
{

namespace
{

NVSDK_NGX_Result CreateDLSSFeatureD3D11(IDeviceContext*               pContext,
                                        NVSDK_NGX_Parameter*          pNGXParams,
                                        NVSDK_NGX_DLSS_Create_Params& DLSSCreateParams,
                                        NVSDK_NGX_Handle**            ppFeature)
{
    ID3D11DeviceContext* pd3d11Ctx = ClassPtrCast<IDeviceContextD3D11>(pContext)->GetD3D11DeviceContext();
    return NGX_D3D11_CREATE_DLSS_EXT(pd3d11Ctx, ppFeature, pNGXParams, &DLSSCreateParams);
}

class SuperResolutionD3D11_DLSS final : public SuperResolutionDLSS<CreateDLSSFeatureD3D11, NVSDK_NGX_D3D11_ReleaseFeature>
{
public:
    SuperResolutionD3D11_DLSS(IReferenceCounters*        pRefCounters,
                              const SuperResolutionDesc& Desc,
                              const SuperResolutionInfo& Info,
                              NVSDK_NGX_Parameter*       pNGXParams) :
        SuperResolutionDLSS{pRefCounters, Desc, Info, pNGXParams}
    {
    }

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final
    {
        ValidateExecuteSuperResolutionAttribs(m_Desc, m_Info, Attribs);

        NVSDK_NGX_Handle* pDLSSFeature = AcquireFeature(Attribs);
        if (pDLSSFeature == nullptr)
            return;

        TransitionResourceStates(Attribs);

        auto GetD3D11Resource = [](ITextureView* pView) -> ID3D11Resource* {
            return pView != nullptr ?
                ClassPtrCast<ITextureD3D11>(pView->GetTexture())->GetD3D11Texture() :
                nullptr;
        };

        NVSDK_NGX_D3D11_DLSS_Eval_Params EvalParams{};
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

        ID3D11DeviceContext* pd3d11Ctx = ClassPtrCast<IDeviceContextD3D11>(Attribs.pContext)->GetD3D11DeviceContext();

        NVSDK_NGX_Result Result = NGX_D3D11_EVALUATE_DLSS_EXT(pd3d11Ctx, pDLSSFeature, m_pNGXParams, &EvalParams);
        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_MESSAGE("DLSS D3D11 evaluation failed. NGX Result: ", static_cast<Uint32>(Result));
    }
};

class DLSSProviderD3D11 final : public DLSSProviderBase
{
public:
    DLSSProviderD3D11(IRenderDevice* pDevice)
    {
        if (pDevice == nullptr)
            LOG_ERROR_AND_THROW("Device must not be null");
        if (RefCntAutoPtr<IRenderDeviceD3D11> pDeviceD3D11{pDevice, IID_RenderDeviceD3D11})
        {
            m_pd3d11Device = pDeviceD3D11->GetD3D11Device();
        }
        else
        {
            LOG_ERROR_AND_THROW("Device must be of type RENDER_DEVICE_TYPE_D3D11");
        }

        NVSDK_NGX_Result Result = NVSDK_NGX_D3D11_Init_with_ProjectID(DLSSProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0", DLSSAppDataPath, m_pd3d11Device);
        if (NVSDK_NGX_FAILED(Result))
        {
            LOG_WARNING_MESSAGE("NVIDIA NGX D3D11 initialization failed. DLSS will not be available. Result: ", static_cast<Uint32>(Result));
            return;
        }

        Result = NVSDK_NGX_D3D11_GetCapabilityParameters(&m_pNGXParams);
        if (NVSDK_NGX_FAILED(Result) || m_pNGXParams == nullptr)
        {
            LOG_WARNING_MESSAGE("Failed to get NGX D3D11 capability parameters. DLSS will not be available. Result: ", static_cast<Uint32>(Result));
            m_pNGXParams = nullptr;
            NVSDK_NGX_D3D11_Shutdown1(m_pd3d11Device);
        }
    }

    ~DLSSProviderD3D11()
    {
        if (m_pNGXParams != nullptr)
        {
            NVSDK_NGX_D3D11_DestroyParameters(m_pNGXParams);
            NVSDK_NGX_D3D11_Shutdown1(m_pd3d11Device);
        }
    }

    virtual void CreateSuperResolution(const SuperResolutionDesc& Desc, const SuperResolutionInfo& Info, ISuperResolution** ppUpscaler) override final
    {
        DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

        SuperResolutionD3D11_DLSS* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionD3D11_DLSS instance", SuperResolutionD3D11_DLSS)(Desc, Info, m_pNGXParams);
        pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
    }

private:
    CComPtr<ID3D11Device> m_pd3d11Device;
};

} // anonymous namespace

std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderD3D11(IRenderDevice* pDevice)
{
    return pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11 ?
        std::make_unique<DLSSProviderD3D11>(pDevice) :
        nullptr;
}

} // namespace Diligent
