/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "DummyRenderDevice.hpp"
#include "GLSLangUtils.hpp"

namespace Diligent
{
namespace
{
static constexpr Uint32 GetDeviceBits()
{
    Uint32 DeviceBits = 0;
#if D3D11_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D11;
#endif
#if D3D12_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D12;
#endif
#if GL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GL;
#endif
#if GLES_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GLES;
#endif
#if VULKAN_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_VULKAN;
#endif
#if METAL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_METAL;
#endif
    return DeviceBits;
}

static constexpr Uint32 ValidDeviceBits = GetDeviceBits();
} // namespace


DummyRenderDevice::DummyRenderDevice() :
    TBase{nullptr},
    m_pDxCompiler{CreateDXCompiler(DXCompilerTarget::Direct3D12, 0, nullptr)},
    m_pVkDxCompiler{CreateDXCompiler(DXCompilerTarget::Vulkan, GetVkVersion(), nullptr)}
{
    m_DeviceInfo.Features  = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};
    m_AdapterInfo.Features = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};

#if !DILIGENT_NO_GLSLANG
    GLSLangUtils::InitializeGlslang();
#endif
}

DummyRenderDevice::~DummyRenderDevice()
{
#if !DILIGENT_NO_GLSLANG
    GLSLangUtils::FinalizeGlslang();
#endif
}

Uint32 DummyRenderDevice::GetValidDeviceBits()
{
    return ValidDeviceBits;
}

} // namespace Diligent
