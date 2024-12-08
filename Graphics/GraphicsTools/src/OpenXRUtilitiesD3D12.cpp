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
#include <d3d12.h>
#include <atlbase.h>
#include "WinHPostface.h"

#include "RenderDeviceD3D12.h"
#include "CommandQueueD3D12.h"


#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace Diligent
{

void GetOpenXRGraphicsBindingD3D12(IRenderDevice*  pDevice,
                                   IDeviceContext* pContext,
                                   IDataBlob**     ppGraphicsBinding)
{
    RefCntAutoPtr<DataBlobImpl> pDataBlob{DataBlobImpl::Create(sizeof(XrGraphicsBindingD3D12KHR))};

    RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12{pDevice, IID_RenderDeviceD3D12};
    VERIFY_EXPR(pDeviceD3D12 != nullptr);
    RefCntAutoPtr<ICommandQueueD3D12> pQueueD3D12{pContext->LockCommandQueue(), IID_CommandQueueD3D12};
    VERIFY_EXPR(pQueueD3D12 != nullptr);

    XrGraphicsBindingD3D12KHR& Binding = *reinterpret_cast<XrGraphicsBindingD3D12KHR*>(pDataBlob->GetDataPtr());
    Binding.type                       = XR_TYPE_GRAPHICS_BINDING_D3D12_KHR;
    Binding.next                       = nullptr;
    Binding.device                     = pDeviceD3D12->GetD3D12Device();
    Binding.queue                      = pQueueD3D12->GetD3D12CommandQueue();

    *ppGraphicsBinding = pDataBlob.Detach();
}

} // namespace Diligent
