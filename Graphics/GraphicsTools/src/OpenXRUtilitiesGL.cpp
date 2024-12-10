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

#if GL_SUPPORTED

#    if PLATFORM_WIN32

#        include "WinHPreface.h"
#        include <Windows.h>
#        include <Unknwn.h>
#        include "WinHPostface.h"

#        define XR_USE_PLATFORM_WIN32
#    endif

#    define XR_USE_GRAPHICS_API_OPENGL
#    include <openxr/openxr_platform.h>

using XrSwapchainImageGL                             = XrSwapchainImageOpenGLKHR;
constexpr XrStructureType XR_TYPE_SWAPCHAIN_IMAGE_GL = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;

#elif GLES_SUPPORTED

#    if PLATFORM_ANDROID
#        include <jni.h>
#        include <EGL/egl.h>

#        define XR_USE_PLATFORM_ANDROID
#    endif

typedef unsigned int EGLenum;

#    define XR_USE_GRAPHICS_API_OPENGL_ES
#    include <openxr/openxr_platform.h>

using XrSwapchainImageGL                             = XrSwapchainImageOpenGLESKHR;
constexpr XrStructureType XR_TYPE_SWAPCHAIN_IMAGE_GL = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;

#else
#    error Neither GL_SUPPORTED nor GLES_SUPPORTED is defined
#endif

#include "RenderDeviceGL.h"

namespace Diligent
{

void GetOpenXRGraphicsBindingGL(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding)
{
#if GL_SUPPORTED && PLATFORM_WIN32
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrGraphicsBindingOpenGLWin32KHR))};

    RefCntAutoPtr<IRenderDeviceGL> pDeviceGL{pDevice, IID_RenderDeviceGL};
    VERIFY_EXPR(pDeviceGL != nullptr);
    NativeGLContextAttribsWin32 GLCtxAttribs = pDeviceGL->GetNativeGLContextAttribs();

    XrGraphicsBindingOpenGLWin32KHR& Binding{*pDataBlob->GetDataPtr<XrGraphicsBindingOpenGLWin32KHR>()};
    Binding.type  = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
    Binding.next  = nullptr;
    Binding.hDC   = static_cast<HDC>(GLCtxAttribs.hDC);
    Binding.hGLRC = static_cast<HGLRC>(GLCtxAttribs.hGLRC);

    *ppGraphicsBinding = pDataBlob.Detach();
#elif GLES_SUPPORTED && PLATFORM_ANDROID
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrGraphicsBindingOpenGLESAndroidKHR))};

    RefCntAutoPtr<IRenderDeviceGL> pDeviceGL{pDevice, IID_RenderDeviceGL};
    VERIFY_EXPR(pDeviceGL != nullptr);
    NativeGLContextAttribsAndroid GLCtxAttribs = pDeviceGL->GetNativeGLContextAttribs();

    XrGraphicsBindingOpenGLESAndroidKHR& Binding{*pDataBlob->GetDataPtr<XrGraphicsBindingOpenGLESAndroidKHR>()};
    Binding.type    = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    Binding.next    = nullptr;
    Binding.display = GLCtxAttribs.Display;
    Binding.config  = GLCtxAttribs.Config;
    Binding.context = GLCtxAttribs.Context;

    *ppGraphicsBinding = pDataBlob.Detach();
#else
    UNEXPECTED("OpenXR GL bindings are not supported on this platform. The application should initialize the bindings manually.");
#endif
}

void AllocateOpenXRSwapchainImageDataGL(Uint32      ImageCount,
                                        IDataBlob** ppSwapchainImageData)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrSwapchainImageGL) * ImageCount)};
    for (Uint32 i = 0; i < ImageCount; ++i)
    {
        XrSwapchainImageGL& Image{pDataBlob->GetDataPtr<XrSwapchainImageGL>()[i]};
        Image.type = XR_TYPE_SWAPCHAIN_IMAGE_GL;
        Image.next = nullptr;
    }

    *ppSwapchainImageData = pDataBlob.Detach();
}


void GetOpenXRSwapchainImageGL(IRenderDevice*                    pDevice,
                               const XrSwapchainImageBaseHeader* ImageData,
                               Uint32                            ImageIndex,
                               const TextureDesc&                TexDesc,
                               ITexture**                        ppImage)
{
    const XrSwapchainImageGL* ImageGL = reinterpret_cast<const XrSwapchainImageGL*>(ImageData);

    if (ImageData->type != XR_TYPE_SWAPCHAIN_IMAGE_GL || ImageGL[ImageIndex].type != XR_TYPE_SWAPCHAIN_IMAGE_GL)
    {
        UNEXPECTED("Unexpected swapchain image type");
        return;
    }

    uint32_t image = ImageGL[ImageIndex].image;
    if (image == 0)
    {
        UNEXPECTED("OpenGL image is null");
        return;
    }

    RefCntAutoPtr<IRenderDeviceGL> pDeviceGL{pDevice, IID_RenderDeviceGL};
    VERIFY_EXPR(pDeviceGL != nullptr);

    pDeviceGL->CreateTextureFromGLHandle(image, 0, TexDesc, RESOURCE_STATE_UNDEFINED, ppImage);
}

} // namespace Diligent
