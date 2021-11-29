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

#include "../../../Common/interface/StringTools.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/Shader.h"
#include "../../GraphicsEngine/interface/RenderPass.h"
#include "../../GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Archiver.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {205BB0B2-0966-4F51-9380-46EE5BCED28B}
static const INTERFACE_ID IID_SerializationDevice =
    {0x205bb0b2, 0x966, 0x4f51, {0x93, 0x80, 0x46, 0xee, 0x5b, 0xce, 0xd2, 0x8b}};


#define DILIGENT_INTERFACE_NAME ISerializationDevice
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ISerializationDeviceInclusiveMethods \
    IRenderDeviceInclusiveMethods;           \
    ISerializationDeviceMethods SerializationDevice

// clang-format off

/// Serialization device attributes for Direct3D11 backend
struct SerializationDeviceD3D11Info
{
    Version FeatureLevel DEFAULT_INITIALIZER(Version(11, 0));

#if DILIGENT_CPP_INTERFACE
    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    constexpr bool operator==(const SerializationDeviceD3D11Info& RHS) const
    {
        return FeatureLevel == RHS.FeatureLevel;
    }
#endif

};
typedef struct SerializationDeviceD3D11Info SerializationDeviceD3D11Info;

/// Serialization device attributes for Direct3D12 backend
struct SerializationDeviceD3D12Info
{
    Version     ShaderVersion  DEFAULT_INITIALIZER(Version(6, 0));
    const Char* DxCompilerPath DEFAULT_INITIALIZER(nullptr);

#if DILIGENT_CPP_INTERFACE
    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    constexpr bool operator==(const SerializationDeviceD3D12Info& RHS) const
    {
        return ShaderVersion == RHS.ShaderVersion && SafeStrEqual(DxCompilerPath, RHS.DxCompilerPath);
    }
#endif

};
typedef struct SerializationDeviceD3D12Info SerializationDeviceD3D12Info;

/// Serialization device attributes for Vulkan backend
struct SerializationDeviceVkInfo
{
    Version     ApiVersion       DEFAULT_INITIALIZER(Version(1, 0));
    Bool        SupportedSpirv14 DEFAULT_INITIALIZER(False);
    const Char* DxCompilerPath   DEFAULT_INITIALIZER(nullptr);

#if DILIGENT_CPP_INTERFACE
    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    constexpr bool operator==(const SerializationDeviceVkInfo& RHS) const 
    {
        return ApiVersion       == RHS.ApiVersion &&
               SupportedSpirv14 == RHS.SupportedSpirv14 &&
               SafeStrEqual(DxCompilerPath, RHS.DxCompilerPath);
    }
#endif

};
typedef struct SerializationDeviceVkInfo SerializationDeviceVkInfo;

/// Serialization device attributes for Metal backend
struct SerializationDeviceMtlInfo
{
    /// Additional compilation options for Metal command-line compiler.
    const Char* CompileOptionsMacOS DEFAULT_INITIALIZER("-sdk macosx metal");
    const Char* CompileOptionsiOS   DEFAULT_INITIALIZER("-sdk iphoneos metal");

    /// Additional linker options for Metal command-line linker.
    const Char* LinkOptionsMacOS  DEFAULT_INITIALIZER("-sdk macosx metallib");
    const Char* LinkOptionsiOS    DEFAULT_INITIALIZER("-sdk iphoneos metallib");

    /// Name of command-line application which is used to preprocess Metal shader source before compiling to bytecode.
    const Char* MslPreprocessorCmd DEFAULT_INITIALIZER(nullptr);

    Bool  CompileForMacOS DEFAULT_INITIALIZER(True);
    Bool  CompileForiOS   DEFAULT_INITIALIZER(True);

#if DILIGENT_CPP_INTERFACE
    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    bool operator==(const SerializationDeviceMtlInfo& RHS) const 
    {
        return SafeStrEqual(CompileOptionsMacOS, RHS.CompileOptionsMacOS) &&
               SafeStrEqual(CompileOptionsiOS,   RHS.CompileOptionsiOS)   &&
               SafeStrEqual(LinkOptionsMacOS,    RHS.LinkOptionsMacOS)    &&
               SafeStrEqual(LinkOptionsiOS,      RHS.LinkOptionsiOS)      &&
               SafeStrEqual(MslPreprocessorCmd,  RHS.MslPreprocessorCmd)  &&
               CompileForMacOS == RHS.CompileForMacOS                     &&
               CompileForiOS   == RHS.CompileForiOS;
    }
#endif

};
typedef struct SerializationDeviceMtlInfo SerializationDeviceMtlInfo;

/// Serialization device creation information
struct SerializationDeviceCreateInfo
{
    /// Device info, contains enabled device features.
    /// Can be used to validate shader, render pass, resource signature and pipeline state.
    ///
    /// \note For OpenGL which is not support separable programs disable SeparablePrograms feature.
    RenderDeviceInfo    DeviceInfo;

    /// Adapter info, contains device parameters.
    /// Can be used to validate shader, render pass, resource signature and pipeline state.
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
    SHADER_TYPE                  ShaderStages      DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// Device type for which resource binding will be calculated.
    enum RENDER_DEVICE_TYPE      DeviceType        DEFAULT_INITIALIZER(RENDER_DEVICE_TYPE_UNDEFINED);
};
typedef struct PipelineResourceBindingAttribs PipelineResourceBindingAttribs;

/// Pipeline resource binding
struct PipelineResourceBinding
{
    const Char*          Name           DEFAULT_INITIALIZER(nullptr);
    SHADER_RESOURCE_TYPE ResourceType   DEFAULT_INITIALIZER(SHADER_RESOURCE_TYPE_UNKNOWN);
    SHADER_TYPE          ShaderStages   DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);
    Uint16               Space          DEFAULT_INITIALIZER(0);
    Uint32               Register       DEFAULT_INITIALIZER(0);
    Uint32               ArraySize      DEFAULT_INITIALIZER(0);
};
typedef struct PipelineResourceBinding PipelineResourceBinding;


/// Defines the methods to manipulate a serialization device object
DILIGENT_BEGIN_INTERFACE(ISerializationDevice, IRenderDevice)
{
    /// Creates a serialized shader.
    VIRTUAL void METHOD(CreateShader)(THIS_
                                      const ShaderCreateInfo REF ShaderCI,
                                      ARCHIVE_DEVICE_DATA_FLAGS  DeviceFlags,
                                      IShader**                  ppShader) PURE;

 
    /// Creates a serialized pipeline resource signature.
    VIRTUAL void METHOD(CreatePipelineResourceSignature)(THIS_
                                                         const PipelineResourceSignatureDesc REF Desc,
                                                         ARCHIVE_DEVICE_DATA_FLAGS               DeviceFlags,
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
#    define ISerializationDevice_CreatePipelineResourceSignature(This, ...) CALL_IFACE_METHOD(SerializationDevice, CreatePipelineResourceSignature, This, __VA_ARGS__)
#    define ISerializationDevice_GetPipelineResourceBindings(This, ...)     CALL_IFACE_METHOD(SerializationDevice, GetPipelineResourceBindings,     This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
