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

namespace Diligent
{

#if DILIGENT_USE_OPENXR

#    if D3D11_SUPPORTED
void GetOpenXRGraphicsBindingD3D11(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding);
#    endif

#    if D3D12_SUPPORTED
void GetOpenXRGraphicsBindingD3D12(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding);
#    endif

#    if GL_SUPPORTED || GLES_SUPPORTED
void GetOpenXRGraphicsBindingGL(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding);
#    endif

#    if VULKAN_SUPPORTED
void GetOpenXRGraphicsBindingVk(IRenderDevice*  pDevice,
                                IDeviceContext* pContext,
                                IDataBlob**     ppGraphicsBinding);
#    endif

#endif

void GetOpenXRGraphicsBinding(IRenderDevice*  pDevice,
                              IDeviceContext* pContext,
                              IDataBlob**     ppGraphicsBinding)
{
#if DILIGENT_USE_OPENXR
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
#    if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            GetOpenXRGraphicsBindingD3D11(pDevice, pContext, ppGraphicsBinding);
            break;
#    endif

#    if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            GetOpenXRGraphicsBindingD3D12(pDevice, pContext, ppGraphicsBinding);
            break;
#    endif

#    if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            GetOpenXRGraphicsBindingGL(pDevice, pContext, ppGraphicsBinding);
            break;
#    endif

#    if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            GetOpenXRGraphicsBindingVk(pDevice, pContext, ppGraphicsBinding);
            break;
#    endif

        default:
            UNSUPPORTED("Unsupported device type");
    }
#else
    UNSUPPORTED("OpenXR is not supported");
#endif
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
}
