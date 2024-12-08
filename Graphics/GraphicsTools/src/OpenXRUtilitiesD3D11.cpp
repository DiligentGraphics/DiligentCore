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

#include "WinHPreface.h"
#include <d3d11.h>
#include <atlbase.h>
#include "WinHPostface.h"

#include "RenderDeviceD3D11.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

namespace Diligent
{

void GetOpenXRGraphicsBindingD3D11(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrGraphicsBindingD3D11KHR))};

    RefCntAutoPtr<IRenderDeviceD3D11> pDeviceD3D11{pDevice, IID_RenderDeviceD3D11};
    VERIFY_EXPR(pDeviceD3D11 != nullptr);

    XrGraphicsBindingD3D11KHR& Binding = *reinterpret_cast<XrGraphicsBindingD3D11KHR*>(pDataBlob->GetDataPtr());
    Binding.type                       = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
    Binding.next                       = nullptr;
    Binding.device                     = pDeviceD3D11->GetD3D11Device();

    *ppGraphicsBinding = pDataBlob.Detach();
}

void AllocateOpenXRSwapchainImageDataD3D11(Uint32      ImageCount,
                                           IDataBlob** ppSwapchainImageData)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrSwapchainImageD3D11KHR) * ImageCount)};
    for (Uint32 i = 0; i < ImageCount; ++i)
    {
        XrSwapchainImageD3D11KHR& Image{pDataBlob->GetDataPtr<XrSwapchainImageD3D11KHR>()[i]};
        Image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        Image.next = nullptr;
    }

    *ppSwapchainImageData = pDataBlob.Detach();
}

void GetOpenXRSwapchainImageD3D11(IRenderDevice*                    pDevice,
                                  const XrSwapchainImageBaseHeader* ImageData,
                                  Uint32                            ImageIndex,
                                  ITexture**                        ppImage)
{
    const XrSwapchainImageD3D11KHR* ImageD3D11 = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(ImageData);

    if (ImageData->type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR || ImageD3D11[ImageIndex].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR)
    {
        UNEXPECTED("Unexpected swapchain image type");
        return;
    }

    ID3D11Texture2D* texture = ImageD3D11[ImageIndex].texture;
    if (texture == nullptr)
    {
        UNEXPECTED("D3D11 texture is null");
        return;
    }

    RefCntAutoPtr<IRenderDeviceD3D11> pDeviceD3D11{pDevice, IID_RenderDeviceD3D11};
    VERIFY_EXPR(pDeviceD3D11 != nullptr);

    pDeviceD3D11->CreateTexture2DFromD3DResource(texture, RESOURCE_STATE_UNDEFINED, ppImage);
}

} // namespace Diligent
