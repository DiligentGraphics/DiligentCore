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
#include "GraphicsAccessories.hpp"

namespace Diligent
{

#if D3D11_SUPPORTED
void GetOpenXRGraphicsBindingD3D11(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding);

void AllocateOpenXRSwapchainImageDataD3D11(Uint32      ImageCount,
                                           IDataBlob** ppSwapchainImageData);

void GetOpenXRSwapchainImageD3D11(IRenderDevice*                    pDevice,
                                  const XrSwapchainImageBaseHeader* ImageData,
                                  Uint32                            ImageIndex,
                                  ITexture**                        ppImage);
#endif

#if D3D12_SUPPORTED
void GetOpenXRGraphicsBindingD3D12(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding);

void AllocateOpenXRSwapchainImageDataD3D12(Uint32      ImageCount,
                                           IDataBlob** ppSwapchainImageData);

void GetOpenXRSwapchainImageD3D12(IRenderDevice*                    pDevice,
                                  const XrSwapchainImageBaseHeader* ImageData,
                                  Uint32                            ImageIndex,
                                  ITexture**                        ppImage);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
void GetOpenXRGraphicsBindingGL(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding);

void AllocateOpenXRSwapchainImageDataGL(Uint32      ImageCount,
                                        IDataBlob** ppSwapchainImageData);

void GetOpenXRSwapchainImageGL(IRenderDevice*                    pDevice,
                               const XrSwapchainImageBaseHeader* ImageData,
                               Uint32                            ImageIndex,
                               const TextureDesc&                TexDesc,
                               ITexture**                        ppImage);
#endif

#if VULKAN_SUPPORTED
void GetOpenXRGraphicsBindingVk(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding);

void AllocateOpenXRSwapchainImageDataVk(Uint32      ImageCount,
                                        IDataBlob** ppSwapchainImageData);

void GetOpenXRSwapchainImageVk(IRenderDevice*                    pDevice,
                               const XrSwapchainImageBaseHeader* ImageData,
                               Uint32                            ImageIndex,
                               const TextureDesc&                TexDesc,
                               ITexture**                        ppImage);
#endif


void GetOpenXRGraphicsBinding(IRenderDevice*  pDevice,
                              IDeviceContext* pContext,
                              IDataBlob**     ppGraphicsBinding)
{
    if (pDevice == nullptr)
    {
        UNEXPECTED("pDevice must not be null");
        return;
    }

    if (pContext == nullptr)
    {
        UNEXPECTED("pContext must not be null");
        return;
    }

    if (ppGraphicsBinding == nullptr)
    {
        UNEXPECTED("ppGraphicsBinding must not be null");
        return;
    }

    RENDER_DEVICE_TYPE DevType = pDevice->GetDeviceInfo().Type;
    switch (DevType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            GetOpenXRGraphicsBindingD3D11(pDevice, pContext, ppGraphicsBinding);
            break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            GetOpenXRGraphicsBindingD3D12(pDevice, pContext, ppGraphicsBinding);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            GetOpenXRGraphicsBindingGL(pDevice, pContext, ppGraphicsBinding);
            break;
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            GetOpenXRGraphicsBindingVk(pDevice, pContext, ppGraphicsBinding);
            break;
#endif

        default:
            UNSUPPORTED("Unsupported device type");
    }
}

void AllocateOpenXRSwapchainImageData(RENDER_DEVICE_TYPE DeviceType,
                                      Uint32             ImageCount,
                                      IDataBlob**        ppSwapchainImageData)
{
    if (ppSwapchainImageData == nullptr)
    {
        UNEXPECTED("ppSwapchainImageData must not be null");
        return;
    }

    switch (DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            AllocateOpenXRSwapchainImageDataD3D11(ImageCount, ppSwapchainImageData);
            break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            AllocateOpenXRSwapchainImageDataD3D12(ImageCount, ppSwapchainImageData);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            AllocateOpenXRSwapchainImageDataGL(ImageCount, ppSwapchainImageData);
            break;
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            AllocateOpenXRSwapchainImageDataVk(ImageCount, ppSwapchainImageData);
            break;
#endif

        default:
            UNSUPPORTED("Unsupported device type");
    }
}

void GetOpenXRSwapchainImage(IRenderDevice*                    pDevice,
                             const XrSwapchainImageBaseHeader* ImageData,
                             Uint32                            ImageIndex,
                             const TextureDesc&                TexDesc,
                             ITexture**                        ppImage)
{
    if (pDevice == nullptr)
    {
        UNEXPECTED("pDevice must not be null");
        return;
    }

    if (ImageData == nullptr)
    {
        UNEXPECTED("ImageData must not be null");
        return;
    }

    if (ppImage == nullptr)
    {
        UNEXPECTED("ppImage must not be null");
        return;
    }

    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;
    switch (DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            GetOpenXRSwapchainImageD3D11(pDevice, ImageData, ImageIndex, ppImage);
            break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            GetOpenXRSwapchainImageD3D12(pDevice, ImageData, ImageIndex, ppImage);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            GetOpenXRSwapchainImageGL(pDevice, ImageData, ImageIndex, TexDesc, ppImage);
            break;
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            GetOpenXRSwapchainImageVk(pDevice, ImageData, ImageIndex, TexDesc, ppImage);
            break;
#endif

        default:
            UNSUPPORTED("Unsupported device type");
    }

    if (*ppImage)
    {
        const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs((*ppImage)->GetDesc().Format);
        (*ppImage)->SetState(FmtAttribs.IsDepthStencil() ? RESOURCE_STATE_DEPTH_WRITE : RESOURCE_STATE_RENDER_TARGET);
    }
}

static XrBool32 OpenXRMessageCallbackFunction(XrDebugUtilsMessageSeverityFlagsEXT         xrMessageSeverity,
                                              XrDebugUtilsMessageTypeFlagsEXT             xrMessageType,
                                              const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                              void*                                       pUserData)
{
    DEBUG_MESSAGE_SEVERITY MessageSeverity = DEBUG_MESSAGE_SEVERITY_INFO;
    if (xrMessageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        MessageSeverity = DEBUG_MESSAGE_SEVERITY_ERROR;
    }
    else if (xrMessageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        MessageSeverity = DEBUG_MESSAGE_SEVERITY_WARNING;
    }

    std::string MessageTypeStr;
    if (xrMessageType & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        MessageTypeStr += "GEN";
    }
    if (xrMessageType & XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        if (!MessageTypeStr.empty())
        {
            MessageTypeStr += ",";
        }
        MessageTypeStr += "SPEC";
    }
    if (xrMessageType & XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        if (!MessageTypeStr.empty())
        {
            MessageTypeStr += ",";
        }
        MessageTypeStr += "PERF";
    }

    const char* FunctionName = (pCallbackData->functionName) ? pCallbackData->functionName : "";
    const char* MessageId    = (pCallbackData->messageId) ? pCallbackData->messageId : "";
    const char* Message      = (pCallbackData->message) ? pCallbackData->message : "";

    LOG_DEBUG_MESSAGE(MessageSeverity, '[', MessageTypeStr, "] ", FunctionName, MessageId, " - ", Message);

    return XrBool32{};
}


PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;

XrDebugUtilsMessengerEXT CreateOpenXRDebugUtilsMessenger(XrInstance                          xrInstance,
                                                         XrDebugUtilsMessageSeverityFlagsEXT xrMessageSeverities)
{
    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT;
    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&xrCreateDebugUtilsMessengerEXT)))
    {
        LOG_ERROR_MESSAGE("Failed to get xrCreateDebugUtilsMessengerEXT function pointer.");
        return {};
    }
    VERIFY_EXPR(xrCreateDebugUtilsMessengerEXT);

    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&xrDestroyDebugUtilsMessengerEXT)))
    {
        LOG_ERROR_MESSAGE("Failed to get xrDestroyDebugUtilsMessengerEXT function pointer.");
        return {};
    }
    VERIFY_EXPR(xrDestroyDebugUtilsMessengerEXT);

    // Fill out a XrDebugUtilsMessengerCreateInfoEXT structure specifying all severities and types.
    // Set the userCallback to OpenXRMessageCallbackFunction().
    XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugUtilsMessengerCI.messageSeverities = xrMessageSeverities;

    debugUtilsMessengerCI.messageTypes =
        XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;

    debugUtilsMessengerCI.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)OpenXRMessageCallbackFunction;
    debugUtilsMessengerCI.userData     = nullptr;

    XrDebugUtilsMessengerEXT debugUtilsMessenger{};
    // Finally create and return the XrDebugUtilsMessengerEXT.
    if (XR_FAILED(xrCreateDebugUtilsMessengerEXT(xrInstance, &debugUtilsMessengerCI, &debugUtilsMessenger)))
    {
        LOG_ERROR_MESSAGE("Failed to create OpenXR debug utils messenger.");
        return {};
    }

    return debugUtilsMessenger;
}

XrResult DestroyOpenXRDebugUtilsMessenger(XrDebugUtilsMessengerEXT debugUtilsMessenger)
{
    XrResult Res = XR_ERROR_FUNCTION_UNSUPPORTED;
    if (xrDestroyDebugUtilsMessengerEXT)
    {
        Res = xrDestroyDebugUtilsMessengerEXT(debugUtilsMessenger);
    }
    return Res;
}

} // namespace Diligent

extern "C"
{
    void Diligent_GetOpenXRGraphicsBinding(Diligent::IRenderDevice*  pDevice,
                                           Diligent::IDeviceContext* pContext,
                                           Diligent::IDataBlob**     ppGraphicsBinding)
    {
        Diligent::GetOpenXRGraphicsBinding(pDevice, pContext, ppGraphicsBinding);
    }

    XrDebugUtilsMessengerEXT Diligent_CreateOpenXRDebugUtilsMessenger(XrInstance                          xrInstance,
                                                                      XrDebugUtilsMessageSeverityFlagsEXT xrMessageSeverities)
    {
        return Diligent::CreateOpenXRDebugUtilsMessenger(xrInstance, xrMessageSeverities);
    }

    void Diligent_DestroyOpenXRDebugUtilsMessenger(XrDebugUtilsMessengerEXT debugUtilsMessenger)
    {
        Diligent::DestroyOpenXRDebugUtilsMessenger(debugUtilsMessenger);
    }

    void Diligent_AllocateOpenXRSwapchainImageData(Diligent::RENDER_DEVICE_TYPE DeviceType,
                                                   Diligent::Uint32             ImageCount,
                                                   Diligent::IDataBlob**        ppSwapchainImageData)
    {
        Diligent::AllocateOpenXRSwapchainImageData(DeviceType, ImageCount, ppSwapchainImageData);
    }

    void Diligent_GetOpenXRSwapchainImage(Diligent::IRenderDevice*          pDevice,
                                          const XrSwapchainImageBaseHeader* ImageData,
                                          Diligent::Uint32                  ImageIndex,
                                          const Diligent::TextureDesc&      TexDesc,
                                          Diligent::ITexture**              ppImage)
    {
        Diligent::GetOpenXRSwapchainImage(pDevice, ImageData, ImageIndex, TexDesc, ppImage);
    }
}
