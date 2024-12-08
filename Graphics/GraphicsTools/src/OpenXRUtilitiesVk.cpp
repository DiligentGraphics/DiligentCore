/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "OpenXRUtilities.h"

#include "DebugUtilities.hpp"
#include "DataBlobImpl.hpp"

#include "../../GraphicsEngineVulkan/include/VulkanUtilities/VulkanHeaders.h"

#include "RenderDeviceVk.h"
#include "CommandQueueVk.h"

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

namespace Diligent
{

void GetOpenXRGraphicsBindingVk(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrGraphicsBindingVulkanKHR))};

    RefCntAutoPtr<IRenderDeviceVk> pDeviceVk{pDevice, IID_RenderDeviceVk};
    VERIFY_EXPR(pDeviceVk != nullptr);

    uint32_t queueFamilyIndex = 0;
    {
        ICommandQueue* pQueue = pContext->LockCommandQueue();
        VERIFY_EXPR(pQueue != nullptr);
        if (RefCntAutoPtr<ICommandQueueVk> pQueueVk{pQueue, IID_CommandQueueVk})
        {
            queueFamilyIndex = pQueueVk->GetQueueFamilyIndex();
        }
        else
        {
            UNEXPECTED("Command queue is not a Vulkan queue");
        }
        pContext->UnlockCommandQueue();
    }

    XrGraphicsBindingVulkanKHR& Binding = *reinterpret_cast<XrGraphicsBindingVulkanKHR*>(pDataBlob->GetDataPtr());
    Binding.type                        = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
    Binding.next                        = nullptr;
    Binding.instance                    = pDeviceVk->GetVkInstance();
    Binding.physicalDevice              = pDeviceVk->GetVkPhysicalDevice();
    Binding.device                      = pDeviceVk->GetVkDevice();
    Binding.queueFamilyIndex            = queueFamilyIndex;
    Binding.queueIndex                  = pContext->GetDesc().ContextId;

    *ppGraphicsBinding = pDataBlob.Detach();
}

void AllocateOpenXRSwapchainImageDataVk(Uint32      ImageCount,
                                        IDataBlob** ppSwapchainImageData)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrSwapchainImageVulkanKHR) * ImageCount)};
    for (Uint32 i = 0; i < ImageCount; ++i)
    {
        XrSwapchainImageVulkanKHR& Image{pDataBlob->GetDataPtr<XrSwapchainImageVulkanKHR>()[i]};
        Image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        Image.next = nullptr;
    }

    *ppSwapchainImageData = pDataBlob.Detach();
}

void GetOpenXRSwapchainImageVk(IRenderDevice*                    pDevice,
                               const XrSwapchainImageBaseHeader* ImageData,
                               Uint32                            ImageIndex,
                               const TextureDesc&                TexDesc,
                               ITexture**                        ppImage)
{
    const XrSwapchainImageVulkanKHR* ImageVk = reinterpret_cast<const XrSwapchainImageVulkanKHR*>(ImageData);

    if (ImageData->type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR || ImageVk[ImageIndex].type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR)
    {
        UNEXPECTED("Unexpected swapchain image type");
        return;
    }

    VkImage image = ImageVk[ImageIndex].image;
    if (image == VK_NULL_HANDLE)
    {
        UNEXPECTED("Vulkan image is null");
        return;
    }

    RefCntAutoPtr<IRenderDeviceVk> pDeviceVk{pDevice, IID_RenderDeviceVk};
    VERIFY_EXPR(pDeviceVk != nullptr);

    pDeviceVk->CreateTextureFromVulkanImage(image, TexDesc, RESOURCE_STATE_UNDEFINED, ppImage);
}

} // namespace Diligent
