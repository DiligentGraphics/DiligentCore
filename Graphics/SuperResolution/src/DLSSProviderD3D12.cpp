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

#include <d3d12.h>
#include <atlbase.h>
#include <nvsdk_ngx_helpers.h>

#include "RenderDeviceD3D12.h"
#include "DeviceContextD3D12.h"
#include "TextureD3D12.h"
#include "EngineMemory.h"

namespace Diligent
{

namespace
{

NVSDK_NGX_Result CreateDLSSFeatureD3D12(IDeviceContext*               pContext,
                                        NVSDK_NGX_Parameter*          pNGXParams,
                                        NVSDK_NGX_DLSS_Create_Params& DLSSCreateParams,
                                        NVSDK_NGX_Handle**            ppFeature)
{
    ID3D12GraphicsCommandList* pCmdList = ClassPtrCast<IDeviceContextD3D12>(pContext)->GetD3D12CommandList();
    return NGX_D3D12_CREATE_DLSS_EXT(pCmdList, 1, 1, ppFeature, pNGXParams, &DLSSCreateParams);
}

class SuperResolutionD3D12_DLSS final : public SuperResolutionDLSS<CreateDLSSFeatureD3D12, NVSDK_NGX_D3D12_ReleaseFeature>
{
public:
    SuperResolutionD3D12_DLSS(IReferenceCounters*        pRefCounters,
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

        auto GetD3D12Resource = [](ITextureView* pView) -> ID3D12Resource* {
            return pView != nullptr ?
                ClassPtrCast<ITextureD3D12>(pView->GetTexture())->GetD3D12Texture() :
                nullptr;
        };

        NVSDK_NGX_D3D12_DLSS_Eval_Params EvalParams{};
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

        IDeviceContextD3D12*       pCtx     = ClassPtrCast<IDeviceContextD3D12>(Attribs.pContext);
        ID3D12GraphicsCommandList* pCmdList = pCtx->GetD3D12CommandList();

        NVSDK_NGX_Result Result = NGX_D3D12_EVALUATE_DLSS_EXT(pCmdList, pDLSSFeature, m_pNGXParams, &EvalParams);
        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_MESSAGE("DLSS D3D12 evaluation failed. NGX Result: ", static_cast<Uint32>(Result));

        pCtx->TransitionTextureState(Attribs.pOutputTextureView->GetTexture(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        pCtx->Flush();
    }
};


class DLSSProviderD3D12 final : public DLSSProviderBase
{
public:
    DLSSProviderD3D12(IRenderDevice* pDevice)
    {
        if (pDevice == nullptr)
            LOG_ERROR_AND_THROW("Device must not be null");
        if (RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12{pDevice, IID_RenderDeviceD3D12})
        {
            m_pd3d12Device = pDeviceD3D12->GetD3D12Device();
        }
        else
        {
            LOG_ERROR_AND_THROW("Device must be of type RENDER_DEVICE_TYPE_D3D12");
        }

        NVSDK_NGX_Result Result = NVSDK_NGX_D3D12_Init_with_ProjectID(DLSSProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0", DLSSAppDataPath, m_pd3d12Device);
        if (NVSDK_NGX_FAILED(Result))
        {
            LOG_WARNING_MESSAGE("NVIDIA NGX D3D12 initialization failed. DLSS will not be available. Result: ", static_cast<Uint32>(Result));
            return;
        }

        Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_pNGXParams);
        if (NVSDK_NGX_FAILED(Result) || m_pNGXParams == nullptr)
        {
            LOG_WARNING_MESSAGE("Failed to get NGX D3D12 capability parameters. DLSS will not be available. Result: ", static_cast<Uint32>(Result));
            m_pNGXParams = nullptr;
            NVSDK_NGX_D3D12_Shutdown1(m_pd3d12Device);
        }
    }

    ~DLSSProviderD3D12()
    {
        if (m_pNGXParams != nullptr)
        {
            NVSDK_NGX_D3D12_DestroyParameters(m_pNGXParams);
            NVSDK_NGX_D3D12_Shutdown1(m_pd3d12Device);
        }
    }

    virtual void CreateSuperResolution(const SuperResolutionDesc& Desc, const SuperResolutionInfo& Info, ISuperResolution** ppUpscaler) override final
    {
        DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

        SuperResolutionD3D12_DLSS* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionD3D12_DLSS instance", SuperResolutionD3D12_DLSS)(Desc, Info, m_pNGXParams);
        pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
    }

private:
    CComPtr<ID3D12Device> m_pd3d12Device;
};

} // anonymous namespace

std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderD3D12(IRenderDevice* pDevice)
{
    return pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12 ?
        std::make_unique<DLSSProviderD3D12>(pDevice) :
        nullptr;
}

} // namespace Diligent
