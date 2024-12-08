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

#pragma once

/// \file
/// OpenXR utilities

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../Primitives/interface/DataBlob.h"

#include <openxr/openxr.h>

DILIGENT_BEGIN_NAMESPACE(Diligent)

#include "../../../Primitives/interface/DefineRefMacro.h"

/// Prepares OpenXR graphics binding for the specified device and context.
///
/// \param [in]  pDevice           - Pointer to the render device.
/// \param [in]  pContext          - Pointer to the device context.
/// \param [out] ppGraphicsBinding - Address of the memory location where the pointer to the data blob
///                                  containing the graphics binding will be stored.
///
/// \remarks The function returns the data blob that contains the OpenXR graphics binding structure
///          (XrGraphicsBindingVulkanKHR, XrGraphicsBindingD3D11KHR, etc.).
///          The data blob should be used to create the OpenXR session, for example:
///
///             RefCntAutoPtr<IDataBlob> pGraphicsBinding;
///             GetOpenXRGraphicsBinding(m_pDevice, m_pImmediateContext, &pGraphicsBinding);
///
///             XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
///             sessionCI.next     = pGraphicsBinding->GetConstDataPtr();
///             sessionCI.systemId = m_SystemId;
///             xrCreateSession(m_xrInstance, &sessionCI, &m_xrSession);
///
void DILIGENT_GLOBAL_FUNCTION(GetOpenXRGraphicsBinding)(IRenderDevice*  pDevice,
                                                        IDeviceContext* pContext,
                                                        IDataBlob**     ppGraphicsBinding);

/// Creates OpenXR debug utils messenger.
XrDebugUtilsMessengerEXT DILIGENT_GLOBAL_FUNCTION(CreateOpenXRDebugUtilsMessenger)(XrInstance xrInstance);

/// Destroys OpenXR debug utils messenger.
void DILIGENT_GLOBAL_FUNCTION(DestroyOpenXRDebugUtilsMessenger)(XrDebugUtilsMessengerEXT xrDebugUtilsMessenger);

#include "../../../Primitives/interface/UndefRefMacro.h"

DILIGENT_END_NAMESPACE // namespace Diligent
