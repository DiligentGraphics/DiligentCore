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

// clang-format off

/// \file
/// Definition of the Diligent::IDearchiver interface and related data structures

#include "../../../Primitives/interface/DataBlob.h"
#include "PipelineResourceSignature.h"
#include "PipelineState.h"
#include "DeviceObjectArchive.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)



// AZ TODO
/// Resource signature unpack parameters
struct ResourceSignatureUnpackInfo
{
    struct IRenderDevice* pDevice DEFAULT_INITIALIZER(nullptr);

    IDeviceObjectArchive* pArchive DEFAULT_INITIALIZER(nullptr);

    /// Signature name to unpack. If there is only
    /// one signature in the archive, the name may be null.
    const char* Name DEFAULT_INITIALIZER(nullptr);

    /// Shader resource binding allocation granularity

    /// This member defines the allocation granularity for internal resources required by
    /// the shader resource binding object instances.
    Uint32 SRBAllocationGranularity DEFAULT_INITIALIZER(1);
};
typedef struct ResourceSignatureUnpackInfo ResourceSignatureUnpackInfo;



// AZ TODO
DILIGENT_TYPED_ENUM(PSO_ARCHIVE_FLAGS, Uint32)
{
    PSO_ARCHIVE_FLAG_NONE = 0u,

    /// By default, shader reflection information will be preserved
    /// during the PSO serialization. When this flag is specified,
    /// it will be stripped from the bytecode. This will reduce
    /// the binary size, but also make run-time checks not possible.
    /// Applications should generally use this flag for Release builds.
    /// TODO: this flag may need to be defined when archive is created
    /// to avoid situations where the same byte code is archived with
    /// and without reflection from different PSOs.
    PSO_ARCHIVE_FLAG_STRIP_REFLECTION = 1u << 0,
};

// AZ TODO
DILIGENT_TYPED_ENUM(PSO_UNPACK_FLAGS, Uint32)
{
    PSO_UNPACK_FLAG_NONE = 0u,

    /// Do not perform validation when unpacking the pipeline state.
    /// (TODO: maybe this flag is not needed as validation will not be performed
    ///        if there is no reflection information anyway).

    /// \remarks Parameter validation will only be performed if the PSO
    ///          was serialized without stripping the reflection. If
    ///          reflection was stripped, validation will never be performed
    ///          and this flag will have no effect.
    PSO_UNPACK_FLAG_NO_VALIDATION = 1u << 0,
};

// AZ TODO
DILIGENT_TYPED_ENUM(PSO_UNPACK_OVERRIDE_FLAGS, Uint32)
{
    PSO_UNPACK_OVERRIDE_FLAG_NONE = 0,
    PSO_UNPACK_OVERRIDE_FLAG_NAME = 1u << 0,
    PSO_UNPACK_OVERRIDE_FLAG_RASTERIZER = 1u << 1,
    PSO_UNPACK_OVERRIDE_FLAG_BLEND_STATE = 1u << 1,
    // AZ TODO: flags
};

// AZ TODO
/// Pipeline state unpack parameters
struct PipelineStateUnpackInfo
{
    struct IRenderDevice* pDevice DEFAULT_INITIALIZER(nullptr);

    IDeviceObjectArchive* pArchive DEFAULT_INITIALIZER(nullptr);

    /// PSO name to unpack. If there is only
    /// one PSO in the archive, the name may be null.
    const char* Name DEFAULT_INITIALIZER(nullptr);

    PIPELINE_TYPE PipelineType DEFAULT_INITIALIZER(PIPELINE_TYPE_INVALID);

    union
    {
         const GraphicsPipelineDesc* pGraphicsPipelineDesc DEFAULT_INITIALIZER(nullptr);
         //const ComputePipelineDesc* pComputePipelineDesc;
         const RayTracingPipelineDesc* pRayTracingPipelineDesc;
    };

   PSO_UNPACK_OVERRIDE_FLAGS OverrideFlags DEFAULT_INITIALIZER(PSO_UNPACK_OVERRIDE_FLAG_NONE);

    /// Shader resource binding allocation granularity

    /// This member defines allocation granularity for internal resources required by the shader resource
    /// binding object instances.
    /// Has no effect if the PSO is created with explicit pipeline resource signature(s).
    Uint32 SRBAllocationGranularity DEFAULT_INITIALIZER(1);

    /// Defines which immediate contexts are allowed to execute commands that use this pipeline state.

    /// When ImmediateContextMask contains a bit at position n, the pipeline state may be
    /// used in the immediate context with index n directly (see DeviceContextDesc::ContextId).
    /// It may also be used in a command list recorded by a deferred context that will be executed
    /// through that immediate context.
    ///
    /// \remarks    Only specify these bits that will indicate those immediate contexts where the PSO
    ///             will actually be used. Do not set unnecessary bits as this will result in extra overhead.
    Uint64 ImmediateContextMask     DEFAULT_INITIALIZER(1);

    // Optional PSO cache
    IPipelineStateCache* pCache DEFAULT_INITIALIZER(nullptr);
};
typedef struct PipelineStateUnpackInfo PipelineStateUnpackInfo;


/// AZ TODO
struct RenderPassUnpackInfo
{
    struct IRenderDevice* pDevice  DEFAULT_INITIALIZER(nullptr);

    IDeviceObjectArchive* pArchive DEFAULT_INITIALIZER(nullptr);
    
    /// Render pass name to unpack.
    const char* Name DEFAULT_INITIALIZER(nullptr);
};
typedef struct RenderPassUnpackInfo RenderPassUnpackInfo;

// clang-format on


// {ACB3F67A-CE3B-4212-9592-879122D3C191}
static const INTERFACE_ID IID_Dearchiver =
    {0xacb3f67a, 0xce3b, 0x4212, {0x95, 0x92, 0x87, 0x91, 0x22, 0xd3, 0xc1, 0x91}};

#define DILIGENT_INTERFACE_NAME IDearchiver
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IDearchiverInclusiveMethods \
    IDeviceObjectInclusiveMethods;  \
    IDearchiverMethods Dearchiver

// clang-format off


/// Dearchiver interface
DILIGENT_BEGIN_INTERFACE(IDearchiver, IObject)
{
    // AZ TODO
    VIRTUAL void METHOD(CreateDeviceObjectArchive)(THIS_
                                                   IArchive*              pSource,
                                                   IDeviceObjectArchive** ppArchive) PURE;
    
    /// Resource signatures used by the PSO will be unpacked from the same archive.
    VIRTUAL void METHOD(UnpackPipelineState)(THIS_
                                             const PipelineStateUnpackInfo REF DeArchiveInfo,
                                             IPipelineState**                  ppPSO) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(UnpackResourceSignature)(THIS_
                                                 const ResourceSignatureUnpackInfo REF DeArchiveInfo,
                                                 IPipelineResourceSignature**          ppSignature) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(UnpackRenderPass)(THIS_
                                          const RenderPassUnpackInfo REF DeArchiveInfo,
                                          IRenderPass**                  ppRP) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IDearchiver_CreateDeviceObjectArchive(This, ...)    CALL_IFACE_METHOD(Dearchiver, CreateDeviceObjectArchive,   This, __VA_ARGS__)
#    define IDearchiver_UnpackPipelineState(This, ...)          CALL_IFACE_METHOD(Dearchiver, UnpackPipelineState,         This, __VA_ARGS__)
#    define IDearchiver_UnpackResourceSignature(This, ...)      CALL_IFACE_METHOD(Dearchiver, UnpackResourceSignature,     This, __VA_ARGS__)
#    define IDearchiver_UnpackRenderPass(This, ...)             CALL_IFACE_METHOD(Dearchiver, UnpackRenderPass,            This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE
