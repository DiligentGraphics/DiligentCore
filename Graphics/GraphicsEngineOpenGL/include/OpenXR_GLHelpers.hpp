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

#if GL_SUPPORTED

#    define XR_USE_GRAPHICS_API_OPENGL
#    include <openxr/openxr_platform.h>

using XrGraphicsRequirementsGL                                             = XrGraphicsRequirementsOpenGLKHR;
using PFN_xrGetGLGraphicsRequirements                                      = PFN_xrGetOpenGLGraphicsRequirementsKHR;
static constexpr char            xrGetGLGraphicsRequirementsFunctionName[] = "xrGetOpenGLGraphicsRequirementsKHR";
static constexpr XrStructureType XR_TYPE_GRAPHICS_REQUIREMENTS_GL          = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;


#else GLES_SUPPORTED

#    define XR_USE_GRAPHICS_API_OPENGL_ES
#    include <openxr/openxr_platform.h>

using XrGraphicsRequirementsGL                                             = XrGraphicsRequirementsOpenGLESKHR;
using PFN_xrGetGLGraphicsRequirements                                      = PFN_xrGetOpenGLESGraphicsRequirementsKHR;
static constexpr char            xrGetGLGraphicsRequirementsFunctionName[] = "xrGetOpenGLESGraphicsRequirementsKHR";
static constexpr XrStructureType XR_TYPE_GRAPHICS_REQUIREMENTS_GL          = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;

#endif

namespace Diligent
{

static Version GetOpenXRRequiredGLVersion(const OpenXRAttribs* pXR) noexcept(false)
{
    if (pXR == nullptr || pXR->Instance == 0)
        return {};

    if (pXR->GetInstanceProcAddr == nullptr)
        LOG_ERROR_AND_THROW("GetInstanceProcAddr must not be null");

    XrInstance xrInstance = XR_NULL_HANDLE;
    static_assert(sizeof(xrInstance) == sizeof(pXR->Instance), "XrInstance size mismatch");
    memcpy(&xrInstance, &pXR->Instance, sizeof(xrInstance));

    XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
    static_assert(sizeof(xrSystemId) == sizeof(pXR->SystemId), "XrSystemId size mismatch");
    memcpy(&xrSystemId, &pXR->SystemId, sizeof(XrSystemId));

    PFN_xrGetInstanceProcAddr       xrGetInstanceProcAddr           = reinterpret_cast<PFN_xrGetInstanceProcAddr>(pXR->GetInstanceProcAddr);
    PFN_xrGetGLGraphicsRequirements xrGetOpenGLGraphicsRequirements = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(xrInstance, xrGetGLGraphicsRequirementsFunctionName, reinterpret_cast<PFN_xrVoidFunction*>(&xrGetOpenGLGraphicsRequirements))))
    {
        LOG_ERROR_AND_THROW("Failed to get ", xrGetGLGraphicsRequirementsFunctionName, ". Make sure that XR_KHR_opengl_enable/XR_KHR_opengl_es_enable extension is enabled.");
    }

    XrGraphicsRequirementsGL xrGraphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_GL};
    if (XR_FAILED(xrGetOpenGLGraphicsRequirements(xrInstance, xrSystemId, &xrGraphicsRequirements)))
    {
        LOG_ERROR_AND_THROW("Failed to get OpenGL graphics requirements");
    }

    return Version{
        XR_VERSION_MAJOR(xrGraphicsRequirements.minApiVersionSupported),
        XR_VERSION_MINOR(xrGraphicsRequirements.minApiVersionSupported),
    };
}

} // namespace Diligent
