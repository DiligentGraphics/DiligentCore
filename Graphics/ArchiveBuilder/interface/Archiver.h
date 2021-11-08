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

#include "Dearchiver.h"
#include "PipelineResourceSignature.h"
#include "PipelineState.h"
#include "DeviceObjectArchive.h"

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

// AZ TODO
struct PipelineStateArchiveInfo
{
    PSO_ARCHIVE_FLAGS Flags DEFAULT_INITIALIZER(PSO_ARCHIVE_FLAG_NONE);
    
    // RENDER_DEVICE_TYPE
    Uint32 DeviceBits DEFAULT_INITIALIZER(0);
};
typedef struct PipelineStateArchiveInfo PipelineStateArchiveInfo;

// AZ TODO
struct ResourceSignatureArchiveInfo
{
    // RENDER_DEVICE_TYPE
    Uint32 DeviceBits DEFAULT_INITIALIZER(0);
};
typedef struct ResourceSignatureArchiveInfo ResourceSignatureArchiveInfo;


// AZ TODO
DILIGENT_BEGIN_INTERFACE(IArchiver, IObject)
{
    // AZ TODO
    VIRTUAL Bool METHOD(SerializeToBlob)(THIS_
                                         IDataBlob** ppBlob) PURE;
    
    // AZ TODO
    VIRTUAL Bool METHOD(SerializeToStream)(THIS_
                                           IFileStream* pStream) PURE;

    // AZ TODO
    /// Pipeline archival requires the same information as PSO creation

    /// Multiple pipeline states may be packed into the same archive as long as
    /// they use unique names.
    /// Pipeline resource signatures used by the pipeline stats will be packed into the same archive.
    VIRTUAL Bool METHOD(ArchiveGraphicsPipelineState)(THIS_
                                                      const GraphicsPipelineStateCreateInfo REF PSOCreateInfo,
                                                      const PipelineStateArchiveInfo REF        ArchiveInfo) PURE;
    
    // AZ TODO
    VIRTUAL Bool METHOD(ArchiveComputePipelineState)(THIS_
                                                     const ComputePipelineStateCreateInfo REF PSOCreateInfo,
                                                     const PipelineStateArchiveInfo REF       ArchiveInfo) PURE;
    
    // AZ TODO
    VIRTUAL Bool METHOD(ArchiveRayTracingPipelineState)(THIS_
                                                        const RayTracingPipelineStateCreateInfo REF PSOCreateInfo,
                                                        const PipelineStateArchiveInfo REF          ArchiveInfo) PURE;
    
    // AZ TODO
    VIRTUAL Bool METHOD(ArchiveTilePipelineState)(THIS_
                                                  const TilePipelineStateCreateInfo REF PSOCreateInfo,
                                                  const PipelineStateArchiveInfo REF    ArchiveInfo) PURE;
    

    /// Multiple PSOs and signatures may be packed into the same archive as long as they use
    /// distinct names
    VIRTUAL Bool METHOD(ArchivePipelineResourceSignature)(THIS_
                                                          const PipelineResourceSignatureDesc REF SignatureDesc,
                                                          const ResourceSignatureArchiveInfo REF  ArchiveInfo) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IArchiver_SerializeToBlob(This, ...)                  CALL_IFACE_METHOD(Archiver, SerializeToBlob,                  This, __VA_ARGS__)
#    define IArchiver_SerializeToStream(This, ...)                CALL_IFACE_METHOD(Archiver, SerializeToStream,                This, __VA_ARGS__)
#    define IArchiver_ArchiveGraphicsPipelineState(This, ...)     CALL_IFACE_METHOD(Archiver, ArchiveGraphicsPipelineState,     This, __VA_ARGS__)
#    define IArchiver_ArchiveComputePipelineState(This, ...)      CALL_IFACE_METHOD(Archiver, ArchiveComputePipelineState,      This, __VA_ARGS__)
#    define IArchiver_ArchiveRayTracingPipelineState(This, ...)   CALL_IFACE_METHOD(Archiver, ArchiveRayTracingPipelineState,   This, __VA_ARGS__)
#    define IArchiver_ArchiveTilePipelineState(This, ...)         CALL_IFACE_METHOD(Archiver, ArchiveTilePipelineState,         This, __VA_ARGS__)
#    define IArchiver_ArchivePipelineResourceSignature(This, ...) CALL_IFACE_METHOD(Archiver, ArchivePipelineResourceSignature, This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
