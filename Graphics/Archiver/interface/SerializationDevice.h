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

#pragma once

/// \file
/// Defines Diligent::ISerializationDevice interface

#include "../../GraphicsEngine/interface/Shader.h"
#include "../../GraphicsEngine/interface/RenderPass.h"
#include "../../GraphicsEngine/interface/PipelineResourceSignature.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {205BB0B2-0966-4F51-9380-46EE5BCED28B}
static const INTERFACE_ID IID_SerializationDevice =
    {0x205bb0b2, 0x966, 0x4f51, {0x93, 0x80, 0x46, 0xee, 0x5b, 0xce, 0xd2, 0x8b}};


#define DILIGENT_INTERFACE_NAME ISerializationDevice
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ISerializationDeviceInclusiveMethods \
    IObjectInclusiveMethods;                 \
    ISerializationDeviceMethods SerializationDevice

// clang-format off

struct SerializationDeviceD3D11Info
{
    Version FeatureLevel;
    
#if DILIGENT_CPP_INTERFACE
    SerializationDeviceD3D11Info() noexcept : FeatureLevel{11, 0} {}
#endif
};

struct SerializationDeviceD3D12Info
{
    Version     ShaderVersion;
    const Char* DxCompilerPath DEFAULT_INITIALIZER(nullptr);
    
#if DILIGENT_CPP_INTERFACE
    SerializationDeviceD3D12Info() noexcept : ShaderVersion{6, 0} {}
#endif
};

struct SerializationDeviceVkInfo
{
    Version     ApiVersion;
    Bool        SupportedSpirv14 DEFAULT_INITIALIZER(False);
    const Char* DxCompilerPath   DEFAULT_INITIALIZER(nullptr);
    
#if DILIGENT_CPP_INTERFACE
    SerializationDeviceVkInfo() noexcept : ApiVersion{1, 0} {}
#endif
};

// AZ TODO
struct SerializationDeviceCreateInfo
{
    RenderDeviceInfo    DeviceInfo;
    GraphicsAdapterInfo AdapterInfo;

    struct SerializationDeviceD3D11Info D3D11;
    struct SerializationDeviceD3D12Info D3D12;
    struct SerializationDeviceVkInfo    Vulkan;
    
#if DILIGENT_CPP_INTERFACE
    SerializationDeviceCreateInfo()
    {
        DeviceInfo.Features  = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};
        AdapterInfo.Features = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};
    }
#endif
};
typedef struct SerializationDeviceCreateInfo SerializationDeviceCreateInfo;


// AZ TODO
DILIGENT_BEGIN_INTERFACE(ISerializationDevice, IObject)
{
    // AZ TODO
    VIRTUAL void METHOD(CreateShader)(THIS_
                                      const ShaderCreateInfo REF ShaderCI,
                                      Uint32                     DeviceBits,
                                      IShader**                  ppShader) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreateRenderPass)(THIS_
                                          const RenderPassDesc REF Desc,
                                          IRenderPass**            ppRenderPass) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreatePipelineResourceSignature)(THIS_
                                                         const PipelineResourceSignatureDesc REF Desc,
                                                         Uint32                                  DeviceBits,
                                                         IPipelineResourceSignature**            ppSignature) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define ISerializationDevice_CreateShader(This, ...)                    CALL_IFACE_METHOD(SerializationDevice, CreateShader,                    This, __VA_ARGS__)
#    define ISerializationDevice_CreateRenderPass(This, ...)                CALL_IFACE_METHOD(SerializationDevice, CreateRenderPass,                This, __VA_ARGS__)
#    define ISerializationDevice_CreatePipelineResourceSignature(This, ...) CALL_IFACE_METHOD(SerializationDevice, CreatePipelineResourceSignature, This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
