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
/// Defines Diligent::IArchiver interface

#include "../../GraphicsEngine/interface/Dearchiver.h"
#include "../../GraphicsEngine/interface/PipelineResourceSignature.h"
#include "../../GraphicsEngine/interface/PipelineState.h"
#include "../../GraphicsEngine/interface/DeviceObjectArchive.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {D8EBEC99-5A44-41A3-968F-1D7127ABEC79}
static const INTERFACE_ID IID_Archiver =
    {0xd8ebec99, 0x5a44, 0x41a3, {0x96, 0x8f, 0x1d, 0x71, 0x27, 0xab, 0xec, 0x79}};

#define DILIGENT_INTERFACE_NAME IArchiver
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IArchiverInclusiveMethods \
    IObjectInclusiveMethods;      \
    IArchiverMethods Archiver

// clang-format off

/// Flags that indicate which device data will be packed into the archive
DILIGENT_TYPED_ENUM(ARCHIVE_DEVICE_DATA_FLAGS, Uint32)
{
    /// No data
    ARCHIVE_DEVICE_DATA_FLAG_NONE        = 0u,

    /// Archive will contain Direct3D11 device data.
    ARCHIVE_DEVICE_DATA_FLAG_D3D11       = 1u << RENDER_DEVICE_TYPE_D3D11,

    /// Archive will contain Direct3D12 device data
    ARCHIVE_DEVICE_DATA_FLAG_D3D12       = 1u << RENDER_DEVICE_TYPE_D3D12,

    /// Archive will contain OpenGL device data
    ARCHIVE_DEVICE_DATA_FLAG_GL          = 1u << RENDER_DEVICE_TYPE_GL,

    /// Archive will contain OpenGLES device data
    ARCHIVE_DEVICE_DATA_FLAG_GLES        = 1u << RENDER_DEVICE_TYPE_GLES,

    /// Archive will contain Vulkan device data
    ARCHIVE_DEVICE_DATA_FLAG_VULKAN      = 1u << RENDER_DEVICE_TYPE_VULKAN,

    /// Archive will contain Metal device data for MacOS
    ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS = 1u << RENDER_DEVICE_TYPE_METAL,

    /// Archive will contain Metal device data for iOS
    ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS   = 2u << RENDER_DEVICE_TYPE_METAL,

    ARCHIVE_DEVICE_DATA_FLAG_LAST        = ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS
};
DEFINE_FLAG_ENUM_OPERATORS(ARCHIVE_DEVICE_DATA_FLAGS)

/// Pipeline state archive info
struct PipelineStateArchiveInfo
{
    /// Pipeline state archive flags, see Diligent::PSO_ARCHIVE_FLAGS.
    PSO_ARCHIVE_FLAGS PSOFlags DEFAULT_INITIALIZER(PSO_ARCHIVE_FLAG_NONE);

    /// Bitset of Diligent::ARCHIVE_DEVICE_DATA_FLAGS.
    /// Specifies for which backends the pipeline state data will be archived.
    ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags DEFAULT_INITIALIZER(ARCHIVE_DEVICE_DATA_FLAG_NONE);
};
typedef struct PipelineStateArchiveInfo PipelineStateArchiveInfo;

// Pipeline resource signature archive info
struct ResourceSignatureArchiveInfo
{
    /// Bitset of ARCHIVE_DEVICE_DATA_FLAGS.
    /// Specifies for which backends the resource signature data will be archived.
    ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags DEFAULT_INITIALIZER(ARCHIVE_DEVICE_DATA_FLAG_NONE);
};
typedef struct ResourceSignatureArchiveInfo ResourceSignatureArchiveInfo;


/// Defines the methods to manipulate an Archive object
DILIGENT_BEGIN_INTERFACE(IArchiver, IObject)
{
    /// Writes an archive to a memory blob
    VIRTUAL Bool METHOD(SerializeToBlob)(THIS_
                                         IDataBlob** ppBlob) PURE;

    /// Writes an archive to a file stream
    VIRTUAL Bool METHOD(SerializeToStream)(THIS_
                                           IFileStream* pStream) PURE;


    /// Adds a graphics pipeline state to the archive currently being created.

    /// \note
    ///     All dependent objects (render pass, resource signatures, shaders) will be added too.
    ///
    /// \remarks
    ///     Pipeline archival requires the same information as PSO creation.
    ///     Multiple pipeline states may be packed into the same archive as long as they use unique names.
    ///     Pipeline resource signatures used by the pipeline state will be packed into the same archive.
    VIRTUAL Bool METHOD(AddGraphicsPipelineState)(THIS_
                                                  const GraphicsPipelineStateCreateInfo REF PSOCreateInfo,
                                                  const PipelineStateArchiveInfo REF        ArchiveInfo) PURE;


    /// Adds a compute pipeline state to the archive currently being created.

    /// \note   All dependent objects (resource signatures, shaders) will be added too.
    VIRTUAL Bool METHOD(AddComputePipelineState)(THIS_
                                                 const ComputePipelineStateCreateInfo REF PSOCreateInfo,
                                                 const PipelineStateArchiveInfo REF       ArchiveInfo) PURE;


    /// Adds a ray tracing pipeline state to the archive currently being created.

    /// \note   All dependent objects (resource signatures, shaders) will be added too.
    VIRTUAL Bool METHOD(AddRayTracingPipelineState)(THIS_
                                                    const RayTracingPipelineStateCreateInfo REF PSOCreateInfo,
                                                    const PipelineStateArchiveInfo REF          ArchiveInfo) PURE;


    /// Adds a tile pipeline state to the archive currently being created.

    /// \note   All dependent objects (resource signatures, shaders) will be added too.
    VIRTUAL Bool METHOD(AddTilePipelineState)(THIS_
                                              const TilePipelineStateCreateInfo REF PSOCreateInfo,
                                              const PipelineStateArchiveInfo REF    ArchiveInfo) PURE;


    /// Adds a pipeline resource signature to the archive currently being created.

    /// \note   Multiple PSOs and signatures may be packed into the same archive as long as they use distinct names.
    VIRTUAL Bool METHOD(AddPipelineResourceSignature)(THIS_
                                                      const PipelineResourceSignatureDesc REF SignatureDesc,
                                                      const ResourceSignatureArchiveInfo REF  ArchiveInfo) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IArchiver_SerializeToBlob(This, ...)              CALL_IFACE_METHOD(Archiver, SerializeToBlob,              This, __VA_ARGS__)
#    define IArchiver_SerializeToStream(This, ...)            CALL_IFACE_METHOD(Archiver, SerializeToStream,            This, __VA_ARGS__)
#    define IArchiver_AddGraphicsPipelineState(This, ...)     CALL_IFACE_METHOD(Archiver, AddGraphicsPipelineState,     This, __VA_ARGS__)
#    define IArchiver_AddComputePipelineState(This, ...)      CALL_IFACE_METHOD(Archiver, AddComputePipelineState,      This, __VA_ARGS__)
#    define IArchiver_AddRayTracingPipelineState(This, ...)   CALL_IFACE_METHOD(Archiver, AddRayTracingPipelineState,   This, __VA_ARGS__)
#    define IArchiver_AddTilePipelineState(This, ...)         CALL_IFACE_METHOD(Archiver, AddTilePipelineState,         This, __VA_ARGS__)
#    define IArchiver_AddPipelineResourceSignature(This, ...) CALL_IFACE_METHOD(Archiver, AddPipelineResourceSignature, This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
