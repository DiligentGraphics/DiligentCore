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

/// Serialization device attributes for Direct3D11 backend
struct SerializationDeviceD3D11Info
{
    Version FeatureLevel DEFAULT_INITIALIZER(Version(11, 0));
};
typedef struct SerializationDeviceD3D11Info SerializationDeviceD3D11Info;

/// Serialization device attributes for Direct3D12 backend
struct SerializationDeviceD3D12Info
{
    Version     ShaderVersion  DEFAULT_INITIALIZER(Version(6, 0));
    const Char* DxCompilerPath DEFAULT_INITIALIZER(nullptr);
};
typedef struct SerializationDeviceD3D12Info SerializationDeviceD3D12Info;

/// Serialization device attributes for Vulkan backend
struct SerializationDeviceVkInfo
{
    Version     ApiVersion       DEFAULT_INITIALIZER(Version(1, 0));
    Bool        SupportedSpirv14 DEFAULT_INITIALIZER(False);
    const Char* DxCompilerPath   DEFAULT_INITIALIZER(nullptr);
};
typedef struct SerializationDeviceVkInfo SerializationDeviceVkInfo;

/// Serialization device attributes for Metal backend
struct SerializationDeviceMtlInfo
{
    /// Path to folder where temporary Metal shaders will be stored and compiled to bytecode.
    const Char* TempShaderFolder   DEFAULT_INITIALIZER(nullptr);

    /// 
    const Char* CompileOptions     DEFAULT_INITIALIZER(nullptr);

    /// 
    const Char* LinkOptions        DEFAULT_INITIALIZER(nullptr);

    /// Name of command-line application which is used to preprocess Metal shader source before compiling to bytecode.
    const Char* MslPreprocessorCmd DEFAULT_INITIALIZER(nullptr);
};
typedef struct SerializationDeviceMtlInfo SerializationDeviceMtlInfo;

/// Serialization device creation information
struct SerializationDeviceCreateInfo
{
    RenderDeviceInfo    DeviceInfo;
    GraphicsAdapterInfo AdapterInfo;

    SerializationDeviceD3D11Info D3D11;
    SerializationDeviceD3D12Info D3D12;
    SerializationDeviceVkInfo    Vulkan;
    SerializationDeviceMtlInfo   Metal;

#if DILIGENT_CPP_INTERFACE
    SerializationDeviceCreateInfo() noexcept
    {
        DeviceInfo.Features  = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};
        AdapterInfo.Features = DeviceFeatures{DEVICE_FEATURE_STATE_ENABLED};
    }
#endif
};
typedef struct SerializationDeviceCreateInfo SerializationDeviceCreateInfo;


/// Contains attributes to calculate pipeline resource bindings
struct PipelineResourceBindingAttribs
{
    /// An array of ResourceSignaturesCount shader resource signatures that
    /// define the layout of shader resources in this pipeline state object.
    /// See Diligent::IPipelineResourceSignature.
    IPipelineResourceSignature** ppResourceSignatures      DEFAULT_INITIALIZER(nullptr);

    /// The number of elements in ppResourceSignatures array.
    Uint32                       ResourceSignaturesCount   DEFAULT_INITIALIZER(0);

    /// The number of render targets, only for graphics pipeline.
    /// \note Required for Direct3D11 graphics pipelines that use UAVs.
    Uint32                       NumRenderTargets  DEFAULT_INITIALIZER(0);

    /// The number of vertex buffers, only for graphics pipeline.
    /// \note Required for Metal.
    Uint32                       NumVertexBuffers  DEFAULT_INITIALIZER(0);

    /// Vertex buffer names.
    /// \note Required for Metal.
    Char const* const*           VertexBufferNames DEFAULT_INITIALIZER(nullptr);

    /// Combination of shader stages.
    /// \note Single shader stage is required for Direct3D11, OpenGL, Metal.
    SHADER_TYPE                  ShaderStages      DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// Device type for which resource binding will be calculated.
    enum RENDER_DEVICE_TYPE      DeviceType        DEFAULT_INITIALIZER(RENDER_DEVICE_TYPE_UNDEFINED);
};
typedef struct PipelineResourceBindingAttribs PipelineResourceBindingAttribs;

/// Pipeline resource binding
struct PipelineResourceBinding
{
    const Char*          Name;
    SHADER_RESOURCE_TYPE ResourceType;
    SHADER_TYPE          ShaderStages;
    Uint16               Space;
    Uint32               Register;
    Uint32               ArraySize;
};
typedef struct PipelineResourceBinding PipelineResourceBinding;


/// Defines the methods to manipulate a serialization device object
DILIGENT_BEGIN_INTERFACE(ISerializationDevice, IObject)
{
    /// Creates a serialized shader.
    VIRTUAL void METHOD(CreateShader)(THIS_
                                      const ShaderCreateInfo REF ShaderCI,
                                      RENDER_DEVICE_TYPE_FLAGS   DeviceFlags,
                                      IShader**                  ppShader) PURE;

    /// Creates a serialized render pass.
    VIRTUAL void METHOD(CreateRenderPass)(THIS_
                                          const RenderPassDesc REF Desc,
                                          IRenderPass**            ppRenderPass) PURE;

    /// Creates a serialized pipeline resource signature.
    VIRTUAL void METHOD(CreatePipelineResourceSignature)(THIS_
                                                         const PipelineResourceSignatureDesc REF Desc,
                                                         RENDER_DEVICE_TYPE_FLAGS                DeviceFlags,
                                                         IPipelineResourceSignature**            ppSignature) PURE;

    /// Populates an array of pipeline resource bindings.
    VIRTUAL void METHOD(GetPipelineResourceBindings)(THIS_
                                                     const PipelineResourceBindingAttribs REF Attribs,
                                                     Uint32 REF                               NumBindings,
                                                     const PipelineResourceBinding* REF       pBindings) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define ISerializationDevice_CreateShader(This, ...)                    CALL_IFACE_METHOD(SerializationDevice, CreateShader,                    This, __VA_ARGS__)
#    define ISerializationDevice_CreateRenderPass(This, ...)                CALL_IFACE_METHOD(SerializationDevice, CreateRenderPass,                This, __VA_ARGS__)
#    define ISerializationDevice_CreatePipelineResourceSignature(This, ...) CALL_IFACE_METHOD(SerializationDevice, CreatePipelineResourceSignature, This, __VA_ARGS__)
#    define ISerializationDevice_GetPipelineResourceBindings(This, ...)     CALL_IFACE_METHOD(SerializationDevice, GetPipelineResourceBindings,     This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
