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

#include "../../GraphicsEngineVulkan/include/pch.h"
#include <nvsdk_ngx_helpers_vk.h>
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "TextureVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

namespace
{

class SuperResolutionVk_DLSS final : public SuperResolutionBase
{
public:
    SuperResolutionVk_DLSS(IReferenceCounters*        pRefCounters,
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

    ~SuperResolutionVk_DLSS()
    {
        if (m_pDLSSFeature != nullptr)
            NVSDK_NGX_VULKAN_ReleaseFeature(m_pDLSSFeature);
    }

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final
    {
        ValidateTemporalExecuteSuperResolutionAttribs(m_Desc, Attribs);

        if (m_pDLSSFeature == nullptr)
            CreateFeature(Attribs);

        DeviceContextVkImpl* pCtxImpl = ClassPtrCast<DeviceContextVkImpl>(Attribs.pContext);

        auto CreateNGXResourceVK = [](ITextureView* pView, VkImageAspectFlags AspectMask, bool bReadWrite) -> NVSDK_NGX_Resource_VK {
            TextureVkImpl*     pTexVk  = ClassPtrCast<TextureVkImpl>(pView->GetTexture());
            TextureViewVkImpl* pViewVk = ClassPtrCast<TextureViewVkImpl>(pView);
            const TextureDesc& TexDesc = pTexVk->GetDesc();

            VkImageSubresourceRange SubresourceRange = {};
            SubresourceRange.aspectMask              = AspectMask;
            SubresourceRange.baseMipLevel            = 0;
            SubresourceRange.levelCount              = 1;
            SubresourceRange.baseArrayLayer          = 0;
            SubresourceRange.layerCount              = 1;

            return NVSDK_NGX_Create_ImageView_Resource_VK(pViewVk->GetVulkanImageView(), pTexVk->GetVkImage(), SubresourceRange, TexFormatToVkFormat(TexDesc.Format), TexDesc.Width, TexDesc.Height, bReadWrite);
        };

        VkCommandBuffer vkCmdBuffer = pCtxImpl->GetVkCommandBuffer();

        NVSDK_NGX_Resource_VK ColorResource  = CreateNGXResourceVK(Attribs.pColorTextureSRV, VK_IMAGE_ASPECT_COLOR_BIT, false);
        NVSDK_NGX_Resource_VK OutputResource = CreateNGXResourceVK(Attribs.pOutputTextureView, VK_IMAGE_ASPECT_COLOR_BIT, true);
        NVSDK_NGX_Resource_VK DepthResource  = CreateNGXResourceVK(Attribs.pDepthTextureSRV, VK_IMAGE_ASPECT_DEPTH_BIT, false);
        NVSDK_NGX_Resource_VK MotionResource = CreateNGXResourceVK(Attribs.pMotionVectorsSRV, VK_IMAGE_ASPECT_COLOR_BIT, false);

        NVSDK_NGX_Resource_VK ExposureResource = {};
        if (Attribs.pExposureTextureSRV)
            ExposureResource = CreateNGXResourceVK(Attribs.pExposureTextureSRV, VK_IMAGE_ASPECT_COLOR_BIT, false);

        NVSDK_NGX_Resource_VK TransparencyMaskResource = {};
        if (Attribs.pReactiveMaskTextureSRV)
            TransparencyMaskResource = CreateNGXResourceVK(Attribs.pReactiveMaskTextureSRV, VK_IMAGE_ASPECT_COLOR_BIT, false);

        NVSDK_NGX_Resource_VK BiasCurrentColorMaskResource = {};
        if (Attribs.pIgnoreHistoryMaskTextureSRV)
            BiasCurrentColorMaskResource = CreateNGXResourceVK(Attribs.pIgnoreHistoryMaskTextureSRV, VK_IMAGE_ASPECT_COLOR_BIT, false);

        NVSDK_NGX_VK_DLSS_Eval_Params EvalParams    = {};
        EvalParams.Feature.pInColor                 = &ColorResource;
        EvalParams.Feature.pInOutput                = &OutputResource;
        EvalParams.pInDepth                         = &DepthResource;
        EvalParams.pInMotionVectors                 = &MotionResource;
        EvalParams.pInExposureTexture               = Attribs.pExposureTextureSRV ? &ExposureResource : nullptr;
        EvalParams.pInTransparencyMask              = Attribs.pReactiveMaskTextureSRV ? &TransparencyMaskResource : nullptr;
        EvalParams.pInBiasCurrentColorMask          = Attribs.pIgnoreHistoryMaskTextureSRV ? &BiasCurrentColorMaskResource : nullptr;
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

        NVSDK_NGX_Result Result = NGX_VULKAN_EVALUATE_DLSS_EXT(vkCmdBuffer, m_pDLSSFeature, m_pNGXParams, &EvalParams);
        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_MESSAGE("DLSS Vulkan evaluation failed. NGX Result: ", static_cast<Uint32>(Result));
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

        VkCommandBuffer  vkCmdBuffer = ClassPtrCast<DeviceContextVkImpl>(Attribs.pContext)->GetVkCommandBuffer();
        NVSDK_NGX_Result Result      = NGX_VULKAN_CREATE_DLSS_EXT(vkCmdBuffer, 1, 1, &m_pDLSSFeature, m_pNGXParams, &DLSSCreateParams);

        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_AND_THROW("Failed to create DLSS Vulkan feature. NGX Result: ", static_cast<Uint32>(Result));
    }

    RefCntAutoPtr<IRenderDevice> m_pDevice;
    NVSDK_NGX_Handle*            m_pDLSSFeature = nullptr;
    NVSDK_NGX_Parameter*         m_pNGXParams   = nullptr;
};


class DLSSProviderVk final : public SuperResolutionProvider
{
public:
    DLSSProviderVk(IRenderDevice* pDevice) :
        m_pDevice{pDevice}
    {
        RenderDeviceVkImpl* pDeviceVk    = ClassPtrCast<RenderDeviceVkImpl>(pDevice);
        VkInstance          vkInstance   = pDeviceVk->GetVkInstance();
        VkPhysicalDevice    vkPhysDevice = pDeviceVk->GetVkPhysicalDevice();
        VkDevice            vkDevice     = pDeviceVk->GetVkDevice();

        NVSDK_NGX_Result Result = NVSDK_NGX_VULKAN_Init_with_ProjectID(DLSSProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0", DLSSAppDataPath, vkInstance, vkPhysDevice, vkDevice);

        {
            Uint32                         ExtCount    = 0;
            VkExtensionProperties*         pExtensions = nullptr;
            NVSDK_NGX_FeatureDiscoveryInfo FeatureInfo = {};
            NVSDK_NGX_Result               ExtResult   = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(vkInstance, vkPhysDevice, &FeatureInfo, &ExtCount, &pExtensions);
            if (NVSDK_NGX_SUCCEED(ExtResult) && ExtCount > 0 && pExtensions != nullptr)
            {
                /* TODO: Need to implement IsExtensionEnabled in VulkanUtilities::LogicalDevice
            const VulkanUtilities::LogicalDevice& LogicDevice = pDeviceVk->GetLogicalDevice();
            for (Uint32 ExtensionIdx = 0; ExtensionIdx < ExtCount; ++ExtensionIdx)
            {
                if (!LogicDevice.IsExtensionEnabled(pExtensions[ExtensionIdx].extensionName))
                {
                    LOG_ERROR_AND_THROW("DLSS requires Vulkan device extension '", pExtensions[ExtensionIdx].extensionName,
                                        "' which is not supported by the physical device. "
                                        "Enable it via EngineVkCreateInfo::ppDeviceExtensionNames.");
                }
            }
            */
            }
        }

        if (NVSDK_NGX_FAILED(Result))
            LOG_ERROR_AND_THROW("NVIDIA NGX Vulkan initialization failed. Result: ", static_cast<Uint32>(Result));

        Result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_pNGXParams);
        if (NVSDK_NGX_FAILED(Result) || m_pNGXParams == nullptr)
            LOG_ERROR_AND_THROW("Failed to get NGX Vulkan capability parameters. Result: ", static_cast<Uint32>(Result));
    }

    ~DLSSProviderVk()
    {
        if (m_pNGXParams != nullptr)
            NVSDK_NGX_VULKAN_DestroyParameters(m_pNGXParams);
        NVSDK_NGX_VULKAN_Shutdown1(ClassPtrCast<RenderDeviceVkImpl>(m_pDevice.RawPtr())->GetVkDevice());
    }

    void EnumerateVariants(std::vector<SuperResolutionInfo>& Variants)
    {
        EnumerateDLSSVariants(m_pNGXParams, Variants);
    }
    void GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings)
    {
        GetDLSSSourceSettings(m_pNGXParams, Attribs, Settings);
    }

    void CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler)
    {
        DEV_CHECK_ERR(m_pDevice != nullptr, "Render device must not be null");
        DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

        SuperResolutionVk_DLSS* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionVk_DLSS instance", SuperResolutionVk_DLSS)(m_pDevice, Desc, m_pNGXParams);
        pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
    }

private:
    RefCntAutoPtr<IRenderDevice> m_pDevice;
    NVSDK_NGX_Parameter*         m_pNGXParams = nullptr;
};

} // anonymous namespace

std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderVk(IRenderDevice* pDevice)
{
    return pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_VULKAN ?
        std::make_unique<DLSSProviderVk>(pDevice) :
        nullptr;
}

} // namespace Diligent
